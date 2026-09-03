#include "FFmpegMergeDownloader.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
}

namespace
{

    static QString ffmpegErrorString(int error)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror(error, buffer, sizeof(buffer));
        return QString::fromUtf8(buffer);
    }

    static qint64 packetTimestampUs(
        const AVPacket* packet,
        const AVStream* stream)
    {
        if (!packet || !stream)
            return std::numeric_limits<qint64>::max();

        int64_t ts = packet->dts;

        if (ts == AV_NOPTS_VALUE)
            ts = packet->pts;

        if (ts == AV_NOPTS_VALUE)
            return std::numeric_limits<qint64>::max();

        return static_cast<qint64>(
            av_rescale_q(
                ts,
                stream->time_base,
                AVRational{ 1, 1000000 }));
    }

    static bool isAudioStream(const AVStream* stream)
    {
        return stream &&
            stream->codecpar &&
            stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO;
    }

    static bool isVideoStream(const AVStream* stream)
    {
        return stream &&
            stream->codecpar &&
            stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
    }

} // namespace


// ============================================================================
// Internal output AVIO
// ============================================================================

struct FFmpegMergeDownloader::OutputContext
{
    QFile file;

    qint64 bytesWritten = 0;
    qint64 estimatedSize = -1;

    QElapsedTimer timer;

    qint64 lastReportedBytes = 0;
    qint64 lastReportMs = 0;

    FFmpegMergeDownloader* owner = nullptr;

    bool writeError = false;

    static int writePacket(
        void* opaque,
        uint8_t* buffer,
        int size)
    {
        auto* ctx = static_cast<OutputContext*>(opaque);

        if (!ctx || !buffer || size < 0)
            return AVERROR(EINVAL);

        if (size == 0)
            return 0;

        qint64 remaining = size;
        const char* ptr =
            reinterpret_cast<const char*>(buffer);

        while (remaining > 0)
        {
            const qint64 written =
                ctx->file.write(ptr, remaining);

            if (written <= 0)
            {
                ctx->writeError = true;
                return AVERROR(EIO);
            }

            ctx->bytesWritten += written;
            ptr += written;
            remaining -= written;
        }

        ctx->reportProgress();

        return size;
    }

    static int64_t seek(
        void* opaque,
        int64_t offset,
        int whence)
    {
        auto* ctx = static_cast<OutputContext*>(opaque);

        if (!ctx)
            return AVERROR(EINVAL);

        if (whence == AVSEEK_SIZE)
            return ctx->file.size();

        whence &= ~AVSEEK_FORCE;

        qint64 position = offset;

        switch (whence)
        {
        case SEEK_SET:
            break;

        case SEEK_CUR:
            position += ctx->file.pos();
            break;

        case SEEK_END:
            position += ctx->file.size();
            break;

        default:
            return AVERROR(EINVAL);
        }

        if (position < 0)
            return AVERROR(EINVAL);

        if (!ctx->file.seek(position))
            return AVERROR(EIO);

        return position;
    }

    void reportProgress()
    {
        if (!owner)
            return;

        const qint64 now =
            timer.isValid()
            ? timer.elapsed()
            : 0;

        // Avoid flooding the Qt event queue.
        if (bytesWritten == lastReportedBytes &&
            now - lastReportMs < 100)
        {
            return;
        }

        if (now - lastReportMs < 100 &&
            bytesWritten - lastReportedBytes < 64 * 1024)
        {
            return;
        }

        lastReportedBytes = bytesWritten;
        lastReportMs = now;

        owner->notifyProgress(bytesWritten);

        if (now > 0)
        {
            const qint64 speed =
                static_cast<qint64>(
                    (static_cast<double>(bytesWritten) * 1000.0) /
                    static_cast<double>(now));

            owner->notifySpeed(speed);
        }
    }
};


// ============================================================================
// RAII helpers
// ============================================================================

namespace
{

    struct FormatContextDeleter
    {
        void operator()(AVFormatContext* context) const
        {
            if (!context)
                return;

            avformat_close_input(&context);
        }
    };

    using InputFormatPtr =
        std::unique_ptr<AVFormatContext, FormatContextDeleter>;


    struct PacketDeleter
    {
        void operator()(AVPacket* packet) const
        {
            if (packet)
                av_packet_free(&packet);
        }
    };

    using PacketPtr =
        std::unique_ptr<AVPacket, PacketDeleter>;

} // namespace


// ============================================================================
// Construction / destruction
// ============================================================================

FFmpegMergeDownloader::FFmpegMergeDownloader(QObject* parent)
    : QObject(parent)
{
}

FFmpegMergeDownloader::~FFmpegMergeDownloader()
{
    Stop();

    if (m_worker.joinable())
        m_worker.join();
}


// ============================================================================
// IDownloader
// ============================================================================

const QString& FFmpegMergeDownloader::destinationPath() const
{
    return m_destinationPath;
}

bool FFmpegMergeDownloader::setDestinationPath(
    const QString& destination_path)
{
    if (m_running)
        return false;

    m_destinationPath = destination_path;
    return true;
}

qint64 FFmpegMergeDownloader::totalFileSize() const
{
    return m_totalFileSize.load();
}

void FFmpegMergeDownloader::setTotalFileSize(qint64 value)
{
    m_totalFileSize.store(value);
}

void FFmpegMergeDownloader::setExpectedFileSize(
    qint64 expected_size)
{
    m_expectedFileSize.store(expected_size);
    m_totalFileSize.store(expected_size);
}

int FFmpegMergeDownloader::speedLimit() const
{
    return m_speedLimit.load();
}

void FFmpegMergeDownloader::setSpeedLimit(int value)
{
    m_speedLimit.store(value);
}

void FFmpegMergeDownloader::setDownloadNamePolicy(
    DuplicateDownloadNamePolicy policy)
{
    m_downloadNamePolicy = policy;
}

void FFmpegMergeDownloader::setObserver(
    DownloaderObserverInterface* observer)
{
    std::lock_guard<std::mutex> lock(m_observerMutex);
    m_observer = observer;
}


// ============================================================================
// Filename
// ============================================================================

QString FFmpegMergeDownloader::makeOutputFilename(
    const QList<QUrl>& urls,
    const QString& filename) const
{
    QString result = filename;

    if (result.isEmpty())
    {
        if (!urls.isEmpty())
        {
            QString base =
                QFileInfo(urls.first().path()).completeBaseName();

            if (base.isEmpty())
                base = QStringLiteral("download");

            result = base + QStringLiteral(".mkv");
        }
        else
        {
            result = QStringLiteral("download.mkv");
        }
    }

    QFileInfo fi(result);

    if (fi.isAbsolute())
    {
        result = fi.absoluteFilePath();
    }
    else
    {
        result =
            QDir(m_destinationPath).filePath(result);
    }

    if (m_downloadNamePolicy == kReplaceFile)
        return result;

    QFileInfo original(result);

    if (!original.exists())
        return result;

    const QString directory =
        original.absolutePath();

    const QString base =
        original.completeBaseName();

    const QString suffix =
        original.suffix().isEmpty()
        ? QString()
        : QStringLiteral(".") + original.suffix();

    for (qint64 n = 1;
        n <= std::numeric_limits<int>::max();
        ++n)
    {
        const QString candidate =
            QDir(directory).filePath(
                QStringLiteral("%1(%2)%3")
                .arg(base)
                .arg(n)
                .arg(suffix));

        if (!QFileInfo::exists(candidate))
            return candidate;
    }

    return QString();
}


// ============================================================================
// Observer notifications
// ============================================================================

void FFmpegMergeDownloader::notifyStart(
    const QByteArray& data)
{
    DownloaderObserverInterface* observer = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_observerMutex);
        observer = m_observer;
    }

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [this, observer, data]()
        {
            Q_UNUSED(this);

            observer->onStart(data);
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyProgress(
    qint64 bytes)
{
    DownloaderObserverInterface* observer = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_observerMutex);
        observer = m_observer;
    }

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [this, observer, bytes]()
        {
            Q_UNUSED(this);

            observer->onProgress(bytes);
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifySpeed(
    qint64 bytesPerSecond)
{
    DownloaderObserverInterface* observer = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_observerMutex);
        observer = m_observer;
    }

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [this, observer, bytesPerSecond]()
        {
            Q_UNUSED(this);

            observer->onSpeed(bytesPerSecond);
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyFileCreated(
    const QString& filename)
{
    DownloaderObserverInterface* observer = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_observerMutex);
        observer = m_observer;
    }

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [this, observer, filename]()
        {
            Q_UNUSED(this);

            observer->onFileCreated(filename);
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyFinished()
{
    DownloaderObserverInterface* observer = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_observerMutex);
        observer = m_observer;
    }

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [this, observer]()
        {
            Q_UNUSED(this);

            observer->onFinished();
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyError(
    utilities::ErrorCode::ERROR_CODES code,
    const QString& description)
{
    DownloaderObserverInterface* observer = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_observerMutex);
        observer = m_observer;
    }

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [this, observer, code, description]()
        {
            Q_UNUSED(this);

            observer->onError(code, description);
        },
        Qt::QueuedConnection);
}


// ============================================================================
// Start / Resume / Pause / Stop
// ============================================================================

void FFmpegMergeDownloader::Start(
    const QList<QUrl>& urls,
    QNetworkAccessManager* network_manager,
    const QString& filename,
    const QStringList& httpHeaders)
{
    Q_UNUSED(network_manager);
    Q_UNUSED(httpHeaders);

    if (m_running)
        return;

    if (urls.size() != 2)
    {
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Exactly two URLs are required: video and audio."));

        return;
    }

    if (!urls[0].isValid() ||
        !urls[1].isValid())
    {
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Invalid media URL."));

        return;
    }

    if (m_worker.joinable())
        m_worker.join();

    m_stopRequested.store(false);
    m_pauseRequested.store(false);
    m_running.store(true);

    m_totalFileSize.store(-1);
    m_expectedFileSize.store(-1);

    const QString outputFilename =
        makeOutputFilename(urls, filename);

    if (outputFilename.isEmpty())
    {
        m_running.store(false);

        notifyError(
            utilities::ErrorCode::eDOWLDOPENFILERR,
            QStringLiteral(
                "Could not generate output filename."));

        return;
    }

    m_worker =
        std::thread(
            [this, urls, outputFilename]()
            {
                mergeWorker(urls, outputFilename);
            });
}

void FFmpegMergeDownloader::Resume(
    const QList<QUrl>& urls,
    QNetworkAccessManager* network_manager,
    const QString& filename,
    const QStringList& httpHeaders)
{
    // Resume support is intentionally not implemented yet.
    //
    // For now a Resume() starts the merge again.

    Start(
        urls,
        network_manager,
        filename,
        httpHeaders);
}

void FFmpegMergeDownloader::Pause()
{
    m_pauseRequested.store(true);
}

void FFmpegMergeDownloader::Stop()
{
    m_stopRequested.store(true);
}


// ============================================================================
// Merge worker
// ============================================================================

void FFmpegMergeDownloader::mergeWorker(
    QList<QUrl> urls,
    QString outputFilename)
{
    auto finishWorker =
        [this]()
        {
            m_running.store(false);
        };

    notifyStart(QByteArray());

    // ------------------------------------------------------------------------
    // Open video input
    // ------------------------------------------------------------------------

    AVFormatContext* videoRaw = nullptr;

    int ret =
        avformat_open_input(
            &videoRaw,
            urls[0].toString().toUtf8().constData(),
            nullptr,
            nullptr);

    if (ret < 0)
    {
        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDNETWORKERR,
            QStringLiteral(
                "Could not open video input: %1")
            .arg(ffmpegErrorString(ret)));

        return;
    }

    InputFormatPtr videoInput(videoRaw);

    ret =
        avformat_find_stream_info(
            videoInput.get(),
            nullptr);

    if (ret < 0)
    {
        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not read video stream information: %1")
            .arg(ffmpegErrorString(ret)));

        return;
    }

    // ------------------------------------------------------------------------
    // Open audio input
    // ------------------------------------------------------------------------

    AVFormatContext* audioRaw = nullptr;

    ret =
        avformat_open_input(
            &audioRaw,
            urls[1].toString().toUtf8().constData(),
            nullptr,
            nullptr);

    if (ret < 0)
    {
        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDNETWORKERR,
            QStringLiteral(
                "Could not open audio input: %1")
            .arg(ffmpegErrorString(ret)));

        return;
    }

    InputFormatPtr audioInput(audioRaw);

    ret =
        avformat_find_stream_info(
            audioInput.get(),
            nullptr);

    if (ret < 0)
    {
        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not read audio stream information: %1")
            .arg(ffmpegErrorString(ret)));

        return;
    }

    // ------------------------------------------------------------------------
    // Select best video
    // ------------------------------------------------------------------------

    const int videoStreamIndex =
        av_find_best_stream(
            videoInput.get(),
            AVMEDIA_TYPE_VIDEO,
            -1,
            -1,
            nullptr,
            0);

    if (videoStreamIndex < 0)
    {
        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not find a video stream: %1")
            .arg(ffmpegErrorString(videoStreamIndex)));

        return;
    }

    AVStream* inputVideoStream =
        videoInput->streams[videoStreamIndex];

    // ------------------------------------------------------------------------
    // Collect ALL audio streams
    // ------------------------------------------------------------------------

    struct AudioBinding
    {
        int inputIndex = -1;
        AVStream* inputStream = nullptr;
        AVStream* outputStream = nullptr;

        AVPacket* pendingPacket = nullptr;

        bool eof = false;
    };

    std::vector<AudioBinding> audioBindings;

    for (unsigned int i = 0;
        i < audioInput->nb_streams;
        ++i)
    {
        AVStream* stream =
            audioInput->streams[i];

        if (!isAudioStream(stream))
            continue;

        AudioBinding binding;

        binding.inputIndex =
            static_cast<int>(i);

        binding.inputStream =
            stream;

        binding.pendingPacket =
            av_packet_alloc();

        if (!binding.pendingPacket)
        {
            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);

            finishWorker();

            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral(
                    "Could not allocate audio packet."));

            return;
        }

        audioBindings.push_back(binding);
    }

    if (audioBindings.empty())
    {
        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "The audio input contains no audio streams."));

        return;
    }

    // ------------------------------------------------------------------------
    // Estimate output size
    // ------------------------------------------------------------------------

    qint64 estimatedInputSize = 0;
    bool haveInputSize = false;

    if (videoInput->pb)
    {
        const int64_t size =
            avio_size(videoInput->pb);

        if (size > 0)
        {
            estimatedInputSize += size;
            haveInputSize = true;
        }
    }

    if (audioInput->pb)
    {
        const int64_t size =
            avio_size(audioInput->pb);

        if (size > 0)
        {
            estimatedInputSize += size;
            haveInputSize = true;
        }
    }

    qint64 estimatedOutputSize =
        haveInputSize
        ? estimatedInputSize
        : -1;

    // ------------------------------------------------------------------------
    // Create output file FIRST
    // ------------------------------------------------------------------------

    OutputContext outputContext;

    outputContext.owner = this;

    if (m_downloadNamePolicy == kReplaceFile)
    {
        outputContext.file.setFileName(outputFilename);

        if (!outputContext.file.open(
            QIODevice::ReadWrite |
            QIODevice::Truncate))
        {
            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);

            finishWorker();

            notifyError(
                utilities::ErrorCode::eDOWLDOPENFILERR,
                QStringLiteral(
                    "Could not create output file '%1': %2")
                .arg(outputFilename,
                    outputContext.file.errorString()));

            return;
        }
    }
    else
    {
        outputContext.file.setFileName(outputFilename);

        if (!outputContext.file.open(
            QIODevice::ReadWrite |
            QIODevice::NewOnly))
        {
            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);

            finishWorker();

            notifyError(
                utilities::ErrorCode::eDOWLDOPENFILERR,
                QStringLiteral(
                    "Could not create output file '%1': %2")
                .arg(outputFilename,
                    outputContext.file.errorString()));

            return;
        }
    }

    // This notification intentionally happens immediately after QFile
    // creation/open succeeds.
    notifyFileCreated(outputFilename);

    outputContext.timer.start();

    // ------------------------------------------------------------------------
    // Output format
    // ------------------------------------------------------------------------

    AVFormatContext* output = nullptr;

    ret =
        avformat_alloc_output_context2(
            &output,
            nullptr,
            "matroska",
            nullptr);

    if (ret < 0 || !output)
    {
        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);

        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not create Matroska output context: %1")
            .arg(ffmpegErrorString(ret)));

        return;
    }

    // ------------------------------------------------------------------------
    // Custom QFile AVIO
    // ------------------------------------------------------------------------

    const int ioBufferSize = 32 * 1024;

    unsigned char* ioBuffer =
        static_cast<unsigned char*>(
            av_malloc(ioBufferSize));

    if (!ioBuffer)
    {
        avformat_free_context(output);

        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);

        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not allocate output IO buffer."));

        return;
    }

    AVIOContext* outputIo =
        avio_alloc_context(
            ioBuffer,
            ioBufferSize,
            1,
            &outputContext,
            nullptr,
            &OutputContext::writePacket,
            &OutputContext::seek);

    if (!outputIo)
    {
        av_free(ioBuffer);
        avformat_free_context(output);

        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);

        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not create output AVIO context."));

        return;
    }

    output->pb = outputIo;
    output->flags |= AVFMT_FLAG_CUSTOM_IO;

    // ------------------------------------------------------------------------
    // Create output video stream
    // ------------------------------------------------------------------------

    AVStream* outputVideoStream =
        avformat_new_stream(output, nullptr);

    if (!outputVideoStream)
    {
        avio_context_free(&outputIo);
        avformat_free_context(output);

        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);

        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not create output video stream."));

        return;
    }

    ret =
        avcodec_parameters_copy(
            outputVideoStream->codecpar,
            inputVideoStream->codecpar);

    if (ret < 0)
    {
        avio_context_free(&outputIo);
        avformat_free_context(output);

        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);

        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not copy video codec parameters: %1")
            .arg(ffmpegErrorString(ret)));

        return;
    }

    /*
     * IMPORTANT:
     *
     * codec_tag values such as:
     *
     *   avc1
     *   hvc1
     *   hev1
     *   av01
     *
     * belong to formats such as MP4/MOV.
     *
     * They must not simply be copied into Matroska.
     *
     * Let the Matroska muxer choose the appropriate mapping.
     */
    outputVideoStream->codecpar->codec_tag = 0;

    outputVideoStream->time_base =
        inputVideoStream->time_base;

    outputVideoStream->sample_aspect_ratio =
        inputVideoStream->sample_aspect_ratio;

    outputVideoStream->disposition =
        inputVideoStream->disposition;

    av_dict_copy(
        &outputVideoStream->metadata,
        inputVideoStream->metadata,
        0);

    // ------------------------------------------------------------------------
    // Create ALL output audio streams
    // ------------------------------------------------------------------------

    for (auto& binding : audioBindings)
    {
        AVStream* outStream =
            avformat_new_stream(output, nullptr);

        if (!outStream)
        {
            avio_context_free(&outputIo);
            avformat_free_context(output);

            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);

            finishWorker();

            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral(
                    "Could not create output audio stream."));

            return;
        }

        ret =
            avcodec_parameters_copy(
                outStream->codecpar,
                binding.inputStream->codecpar);

        if (ret < 0)
        {
            avio_context_free(&outputIo);
            avformat_free_context(output);

            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);

            finishWorker();

            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral(
                    "Could not copy audio codec parameters: %1")
                .arg(ffmpegErrorString(ret)));

            return;
        }

        // Same Matroska requirement as for video.
        outStream->codecpar->codec_tag = 0;

        outStream->time_base =
            binding.inputStream->time_base;

        outStream->sample_aspect_ratio =
            binding.inputStream->sample_aspect_ratio;

        outStream->disposition =
            binding.inputStream->disposition;

        av_dict_copy(
            &outStream->metadata,
            binding.inputStream->metadata,
            0);

        binding.outputStream = outStream;
    }

    // ------------------------------------------------------------------------
    // Preserve container metadata
    // ------------------------------------------------------------------------

    av_dict_copy(
        &output->metadata,
        videoInput->metadata,
        0);

    // Do not shift timestamps to zero.
    output->avoid_negative_ts =
        AVFMT_AVOID_NEG_TS_DISABLED;

    // ------------------------------------------------------------------------
    // Write Matroska header
    // ------------------------------------------------------------------------

    ret =
        avformat_write_header(
            output,
            nullptr);

    if (ret < 0)
    {
        avio_context_free(&outputIo);
        avformat_free_context(output);

        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);

        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not write Matroska header: %1")
            .arg(ffmpegErrorString(ret)));

        return;
    }

    // ------------------------------------------------------------------------
    // Pending video packet
    // ------------------------------------------------------------------------

    AVPacket* pendingVideoPacket =
        av_packet_alloc();

    if (!pendingVideoPacket)
    {
        av_write_trailer(output);
        avio_context_free(&outputIo);
        avformat_free_context(output);

        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);

        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not allocate video packet."));

        return;
    }

    bool videoEof = false;

    // ------------------------------------------------------------------------
    // Helper: read next video packet
    // ------------------------------------------------------------------------

    auto readNextVideoPacket =
        [&]() -> bool
        {
            av_packet_unref(pendingVideoPacket);

            while (!videoEof)
            {
                if (m_stopRequested.load())
                    return false;

                ret =
                    av_read_frame(
                        videoInput.get(),
                        pendingVideoPacket);

                if (ret == AVERROR_EOF)
                {
                    videoEof = true;
                    return false;
                }

                if (ret < 0)
                {
                    videoEof = true;
                    return false;
                }

                if (pendingVideoPacket->stream_index ==
                    videoStreamIndex)
                {
                    return true;
                }

                av_packet_unref(pendingVideoPacket);
            }

            return false;
        };

    // ------------------------------------------------------------------------
    // Helper: read next packet for ONE particular audio stream
    // ------------------------------------------------------------------------

    auto readNextAudioPacket =
        [&](AudioBinding& binding) -> bool
        {
            av_packet_unref(binding.pendingPacket);

            if (binding.eof)
                return false;

            while (!binding.eof)
            {
                if (m_stopRequested.load())
                    return false;

                ret =
                    av_read_frame(
                        audioInput.get(),
                        binding.pendingPacket);

                if (ret == AVERROR_EOF)
                {
                    binding.eof = true;
                    av_packet_unref(binding.pendingPacket);
                    return false;
                }

                if (ret < 0)
                {
                    binding.eof = true;
                    av_packet_unref(binding.pendingPacket);
                    return false;
                }

                const int index =
                    binding.pendingPacket->stream_index;

                if (index == binding.inputIndex)
                    return true;

                /*
                 * This is critical.
                 *
                 * We have read a packet belonging to another audio stream.
                 *
                 * We cannot put it back into FFmpeg's demuxer, so this packet
                 * is not retained here.
                 *
                 * Therefore the implementation below uses a shared demux
                 * queue for audio packets.
                 */

                av_packet_unref(binding.pendingPacket);
            }

            return false;
        };

    // ========================================================================
    // IMPORTANT:
    //
    // av_read_frame() is sequential. Therefore the simple helper above is not
    // sufficient when multiple audio streams are present: while looking for
    // stream #1 we may consume packets belonging to stream #0.
    //
    // Use a demux queue for the complete audio input.
    // ========================================================================

    struct QueuedAudioPacket
    {
        AVPacket* packet = nullptr;
        int streamIndex = -1;
    };

    std::vector<QueuedAudioPacket> audioQueue;

    auto freeAudioQueue =
        [&]()
        {
            for (auto& item : audioQueue)
                av_packet_free(&item.packet);

            audioQueue.clear();
        };

    bool audioEof = false;

    auto selectedAudioStream =
        [&](int streamIndex) -> AudioBinding*
        {
            for (auto& binding : audioBindings)
            {
                if (binding.inputIndex == streamIndex)
                    return &binding;
            }

            return nullptr;
        };

    /*
     * Fill one pending packet for every selected audio stream.
     *
     * We keep demuxing the audio input until every selected stream has a
     * packet, or EOF is reached.
     */
    auto fillAudioPending =
        [&]()
        {
            if (audioEof)
                return;

            for (;;)
            {
                bool allHavePacket = true;

                for (const auto& binding : audioBindings)
                {
                    if (binding.eof)
                        continue;

                    if (!binding.pendingPacket ||
                        binding.pendingPacket->size <= 0)
                    {
                        allHavePacket = false;
                        break;
                    }
                }

                if (allHavePacket)
                    return;

                AVPacket* packet =
                    av_packet_alloc();

                if (!packet)
                {
                    audioEof = true;
                    return;
                }

                const int readRet =
                    av_read_frame(
                        audioInput.get(),
                        packet);

                if (readRet == AVERROR_EOF)
                {
                    av_packet_free(&packet);
                    audioEof = true;

                    for (auto& binding : audioBindings)
                    {
                        if (!binding.pendingPacket ||
                            binding.pendingPacket->size <= 0)
                        {
                            binding.eof = true;
                        }
                    }

                    return;
                }

                if (readRet < 0)
                {
                    av_packet_free(&packet);
                    audioEof = true;

                    for (auto& binding : audioBindings)
                    {
                        if (!binding.pendingPacket ||
                            binding.pendingPacket->size <= 0)
                        {
                            binding.eof = true;
                        }
                    }

                    return;
                }

                AudioBinding* binding =
                    selectedAudioStream(
                        packet->stream_index);

                if (!binding)
                {
                    av_packet_free(&packet);
                    continue;
                }

                if (binding->pendingPacket &&
                    binding->pendingPacket->size > 0)
                {
                    /*
                     * There is already a packet pending for this stream.
                     *
                     * Put this packet into the queue. It will be promoted
                     * when the pending packet for this stream is consumed.
                     */
                    audioQueue.push_back(
                        { packet, packet->stream_index });

                    continue;
                }

                av_packet_ref(
                    binding->pendingPacket,
                    packet);

                av_packet_free(&packet);
            }
        };

    /*
     * Promote a queued packet to a stream's pending slot.
     */
    auto promoteQueuedAudioPacket =
        [&](AudioBinding& binding)
        {
            if (binding.pendingPacket &&
                binding.pendingPacket->size > 0)
            {
                return;
            }

            for (auto it = audioQueue.begin();
                it != audioQueue.end();
                ++it)
            {
                if (it->streamIndex != binding.inputIndex)
                    continue;

                av_packet_ref(
                    binding.pendingPacket,
                    it->packet);

                av_packet_free(&it->packet);

                audioQueue.erase(it);

                return;
            }

            if (audioEof)
                binding.eof = true;
        };

    // ------------------------------------------------------------------------
    // Initial packets
    // ------------------------------------------------------------------------

    const bool haveVideo =
        readNextVideoPacket();

    Q_UNUSED(haveVideo);

    fillAudioPending();

    for (auto& binding : audioBindings)
        promoteQueuedAudioPacket(binding);

    // ------------------------------------------------------------------------
    // Main merge loop
    // ------------------------------------------------------------------------

    while (!m_stopRequested.load())
    {
        if (!videoEof &&
            (!pendingVideoPacket ||
                pendingVideoPacket->size <= 0))
        {
            readNextVideoPacket();
        }

        fillAudioPending();

        for (auto& binding : audioBindings)
            promoteQueuedAudioPacket(binding);

        // ------------------------------------------------------------
        // Find earliest packet among video + ALL audio streams.
        // ------------------------------------------------------------

        AudioBinding* selectedAudio = nullptr;

        qint64 selectedAudioTimestamp =
            std::numeric_limits<qint64>::max();

        for (auto& binding : audioBindings)
        {
            if (binding.eof)
                continue;

            if (!binding.pendingPacket ||
                binding.pendingPacket->size <= 0)
                continue;

            const qint64 ts =
                packetTimestampUs(
                    binding.pendingPacket,
                    binding.inputStream);

            if (ts < selectedAudioTimestamp)
            {
                selectedAudioTimestamp = ts;
                selectedAudio = &binding;
            }
        }

        const bool haveVideoPacket =
            !videoEof &&
            pendingVideoPacket &&
            pendingVideoPacket->size > 0;

        const qint64 videoTimestamp =
            haveVideoPacket
            ? packetTimestampUs(
                pendingVideoPacket,
                inputVideoStream)
            : std::numeric_limits<qint64>::max();

        if (!haveVideoPacket &&
            !selectedAudio)
        {
            /*
             * There are no immediately available packets.
             *
             * If the audio queue contains packets, promote them and retry.
             */
            bool promoted = false;

            for (auto& binding : audioBindings)
            {
                if (!binding.eof &&
                    (!binding.pendingPacket ||
                        binding.pendingPacket->size <= 0))
                {
                    const int before =
                        static_cast<int>(audioQueue.size());

                    promoteQueuedAudioPacket(binding);

                    if (static_cast<int>(audioQueue.size()) != before ||
                        (binding.pendingPacket &&
                            binding.pendingPacket->size > 0))
                    {
                        promoted = true;
                    }
                }
            }

            if (promoted)
                continue;

            break;
        }

        bool writeVideo = false;

        if (haveVideoPacket && !selectedAudio)
        {
            writeVideo = true;
        }
        else if (!haveVideoPacket && selectedAudio)
        {
            writeVideo = false;
        }
        else
        {
            writeVideo =
                videoTimestamp <= selectedAudioTimestamp;
        }

        // ------------------------------------------------------------
        // Write video
        // ------------------------------------------------------------

        if (writeVideo)
        {
            AVPacket* packet =
                pendingVideoPacket;

            packet->stream_index =
                outputVideoStream->index;

            av_packet_rescale_ts(
                packet,
                inputVideoStream->time_base,
                outputVideoStream->time_base);

            ret =
                av_interleaved_write_frame(
                    output,
                    packet);

            if (ret < 0)
            {
                av_packet_free(&pendingVideoPacket);

                av_write_trailer(output);

                avio_context_free(&outputIo);
                avformat_free_context(output);

                freeAudioQueue();

                for (auto& a : audioBindings)
                    av_packet_free(&a.pendingPacket);

                finishWorker();

                notifyError(
                    utilities::ErrorCode::eDOWLDUNKWNFILERR,
                    QStringLiteral(
                        "Could not write video packet: %1")
                    .arg(ffmpegErrorString(ret)));

                return;
            }

            /*
             * av_interleaved_write_frame() takes ownership of the packet's
             * contents / unrefs it, so the packet can be reused.
             */
            av_packet_unref(packet);

            if (!readNextVideoPacket())
                videoEof = true;
        }
        // ------------------------------------------------------------
        // Write audio
        // ------------------------------------------------------------
        else
        {
            AudioBinding& binding =
                *selectedAudio;

            AVPacket* packet =
                binding.pendingPacket;

            packet->stream_index =
                binding.outputStream->index;

            av_packet_rescale_ts(
                packet,
                binding.inputStream->time_base,
                binding.outputStream->time_base);

            ret =
                av_interleaved_write_frame(
                    output,
                    packet);

            if (ret < 0)
            {
                av_packet_free(&pendingVideoPacket);

                av_write_trailer(output);

                avio_context_free(&outputIo);
                avformat_free_context(output);

                freeAudioQueue();

                for (auto& a : audioBindings)
                    av_packet_free(&a.pendingPacket);

                finishWorker();

                notifyError(
                    utilities::ErrorCode::eDOWLDUNKWNFILERR,
                    QStringLiteral(
                        "Could not write audio packet: %1")
                    .arg(ffmpegErrorString(ret)));

                return;
            }

            av_packet_unref(packet);

            /*
             * The next packet for this audio stream may already be sitting
             * in the queue because av_read_frame() is shared by all audio
             * streams.
             */
            promoteQueuedAudioPacket(binding);
        }

        // ------------------------------------------------------------
        // Dynamic size estimate
        // ------------------------------------------------------------

        if (estimatedOutputSize > 0)
        {
            /*
             * Muxed output is normally smaller than the sum of both
             * complete source files because only the selected video and
             * audio packets are copied.
             *
             * Keep a conservative estimate above the amount already
             * written.
             */
            const qint64 minimumEstimate =
                outputContext.bytesWritten +
                std::max<qint64>(
                    64 * 1024,
                    outputContext.bytesWritten / 20);

            estimatedOutputSize =
                std::max(
                    estimatedOutputSize,
                    minimumEstimate);
        }
        else
        {
            /*
             * We don't know the input lengths.
             *
             * Grow the estimate dynamically so it always remains ahead
             * of the actual written amount during the operation.
             */
            const qint64 minimumEstimate =
                outputContext.bytesWritten +
                std::max<qint64>(
                    1024 * 1024,
                    outputContext.bytesWritten / 5);

            estimatedOutputSize =
                std::max(
                    estimatedOutputSize,
                    minimumEstimate);
        }

        m_totalFileSize.store(
            std::max(
                estimatedOutputSize,
                outputContext.bytesWritten));
    }

    // ------------------------------------------------------------------------
    // Stop requested
    // ------------------------------------------------------------------------

    if (m_stopRequested.load())
    {
        av_packet_free(&pendingVideoPacket);

        av_write_trailer(output);

        avio_flush(output->pb);
        outputContext.file.flush();

        avio_context_free(&outputIo);
        avformat_free_context(output);

        freeAudioQueue();

        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);

        finishWorker();

        return;
    }

    // ------------------------------------------------------------------------
    // Finish Matroska
    // ------------------------------------------------------------------------

    ret =
        av_write_trailer(output);

    if (ret < 0)
    {
        av_packet_free(&pendingVideoPacket);

        avio_context_free(&outputIo);
        avformat_free_context(output);

        freeAudioQueue();

        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);

        finishWorker();

        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "Could not finalize Matroska file: %1")
            .arg(ffmpegErrorString(ret)));

        return;
    }

    // Explicitly flush the custom AVIO and QFile.
    avio_flush(output->pb);
    outputContext.file.flush();

    // ------------------------------------------------------------------------
    // Final exact output size
    // ------------------------------------------------------------------------

    const qint64 finalSize =
        outputContext.file.size();

    m_totalFileSize.store(finalSize);

    notifyProgress(finalSize);

    if (outputContext.timer.elapsed() > 0)
    {
        const qint64 speed =
            static_cast<qint64>(
                (static_cast<double>(finalSize) * 1000.0) /
                static_cast<double>(
                    outputContext.timer.elapsed()));

        notifySpeed(speed);
    }

    // ------------------------------------------------------------------------
    // Cleanup
    // ------------------------------------------------------------------------

    av_packet_free(&pendingVideoPacket);

    avio_context_free(&outputIo);
    avformat_free_context(output);

    freeAudioQueue();

    for (auto& binding : audioBindings)
        av_packet_free(&binding.pendingPacket);

    outputContext.file.close();

    finishWorker();

    notifyFinished();
}
