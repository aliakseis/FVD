#include "FFmpegMergeDownloader.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
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

//#pragma optimize( "", off )

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

    int InterruptionRequested(void* ptr)
    {
        return ptr && static_cast<std::atomic<bool>*>(ptr)->load();
    }

#if 0 
    static bool readEbmlVint(
        QFile& file,
        quint64& value,
        int& length,
        bool& unknown)
    {
        const QByteArray firstByte = file.read(1);
        if (firstByte.size() != 1)
            return false;

        const unsigned char first =
            static_cast<unsigned char>(firstByte[0]);

        if (first == 0)
            return false;

        unsigned char mask = 0x80;
        length = 1;
        while (length <= 8 && (first & mask) == 0)
        {
            mask >>= 1;
            ++length;
        }

        if (length > 8)
            return false;

        value = first & static_cast<unsigned char>(mask - 1);

        for (int i = 1; i < length; ++i)
        {
            const QByteArray b = file.read(1);
            if (b.size() != 1)
                return false;
            value = (value << 8) |
                static_cast<unsigned char>(b[0]);
        }

        unknown =
            value == ((quint64(1) << (7 * length)) - 1);
        return true;
    }

    static bool readEbmlId(
        QFile& file,
        quint32& id,
        int& length)
    {
        const QByteArray firstByte = file.read(1);
        if (firstByte.size() != 1)
            return false;

        const unsigned char first =
            static_cast<unsigned char>(firstByte[0]);

        if (first == 0)
            return false;

        unsigned char mask = 0x80;
        length = 1;
        while (length <= 4 && (first & mask) == 0)
        {
            mask >>= 1;
            ++length;
        }

        if (length > 4)
            return false;

        id = first;
        for (int i = 1; i < length; ++i)
        {
            const QByteArray b = file.read(1);
            if (b.size() != 1)
                return false;
            id = (id << 8) |
                static_cast<unsigned char>(b[0]);
        }

        return true;
    }

    // Return the end of the last complete Matroska Cluster. Cues and the old
    // trailer are intentionally excluded; they are regenerated after resume.
    // A truncated final Cluster is discarded in its entirety.
    static qint64 findMatroskaAppendPosition(QFile& file)
    {
        constexpr quint32 kEbml = 0x1A45DFA3u;
        constexpr quint32 kSegment = 0x18538067u;
        constexpr quint32 kCluster = 0x1F43B675u;

        if (!file.seek(0))
            return -1;

        quint32 id = 0;
        int idLength = 0;
        quint64 size = 0;
        int sizeLength = 0;
        bool unknown = false;

        if (!readEbmlId(file, id, idLength) ||
            id != kEbml ||
            !readEbmlVint(file, size, sizeLength, unknown))
            return -1;

        if (unknown ||
            file.pos() + static_cast<qint64>(size) > file.size())
            return -1;

        if (!file.seek(file.pos() + static_cast<qint64>(size)))
            return -1;

        if (!readEbmlId(file, id, idLength) ||
            id != kSegment ||
            !readEbmlVint(file, size, sizeLength, unknown))
            return -1;

        const qint64 segmentStart = file.pos();
        const qint64 segmentEnd =
            unknown
            ? file.size()
            : std::min<qint64>(
                file.size(),
                segmentStart + static_cast<qint64>(size));

        qint64 lastClusterEnd = -1;

        while (file.pos() < segmentEnd)
        {
            if (!readEbmlId(file, id, idLength) ||
                !readEbmlVint(file, size, sizeLength, unknown))
                break;

            const qint64 dataStart = file.pos();

            if (unknown)
            {
                // Unknown-sized Clusters cannot be bounded safely here. The
                // previous complete Cluster remains the safe append point.
                break;
            }

            const qint64 dataEnd =
                dataStart + static_cast<qint64>(size);

            if (dataEnd < dataStart || dataEnd > segmentEnd)
                break;

            if (id == kCluster)
                lastClusterEnd = dataEnd;

            if (!file.seek(dataEnd))
                break;
        }

        return lastClusterEnd;
    }
#endif

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
    bool suppressWrites = false;
    bool startNotified = false;
    qint64 virtualPosition = 0;
    qint64 virtualSize = 0;

#if LIBAVFORMAT_VERSION_MAJOR < 61
#define FFMPEG_AVIO_WRITE_BUFFER uint8_t*
#else
#define FFMPEG_AVIO_WRITE_BUFFER const uint8_t*
#endif

    static int writePacket(
        void* opaque,
        FFMPEG_AVIO_WRITE_BUFFER buffer,
        int size)
    {
        auto* ctx = static_cast<OutputContext*>(opaque);

        if (!ctx || !buffer || size < 0)
            return AVERROR(EINVAL);

        if (size == 0)
            return 0;

        if (ctx->suppressWrites)
        {
            bool match = true;

            if (ctx->file.isOpen())
            {
                const qint64 pos = ctx->virtualPosition;
                const qint64 fileSize = ctx->file.size();

                if (pos < fileSize)
                {
                    const qint64 toRead = std::min<qint64>(size, fileSize - pos);
                    QByteArray realData(toRead, Qt::Uninitialized);

                    if (ctx->file.seek(pos))
                    {
                        const qint64 read = ctx->file.read(realData.data(), toRead);

                        if (read == toRead)
                        {
                            match = (memcmp(realData.data(), buffer, toRead) == 0);
                        }
                        else
                        {
                            match = false;
                        }
                    }
                    else
                    {
                        match = false;
                    }
                }
                else
                {
                    match = false;
                }
            }
            else
            {
                match = false;
            }

            // --- Qt logging of comparison result ---
            if (!match)
            {
                qWarning() << "writePacket: MISMATCH at offset"
                    << ctx->virtualPosition
                    << "size" << size;
            }
            //else
            //{
            //    qDebug() << "writePacket: match at offset"
            //        << ctx->virtualPosition
            //        << "size" << size;
            //}

            // Maintain virtual write semantics
            ctx->virtualPosition += size;
            ctx->virtualSize = std::max(ctx->virtualSize, ctx->virtualPosition);

            return size;
        }

        // --- Normal write path ---
        qint64 remaining = size;
        const char* ptr = reinterpret_cast<const char*>(buffer);

        while (remaining > 0)
        {
            const qint64 written = ctx->file.write(ptr, remaining);

            if (written <= 0)
            {
                ctx->writeError = true;
                return AVERROR(EIO);
            }

            ctx->bytesWritten += written;
            ctx->virtualPosition += written;
            ctx->virtualSize = std::max(ctx->virtualSize, ctx->virtualPosition);
            ptr += written;
            remaining -= written;
        }

        if (!ctx->suppressWrites && !ctx->startNotified && ctx->owner)
        {
            const int notifySize = std::min(size, 64 * 1024);
            ctx->startNotified = true;
            ctx->owner->notifyStart(
                QByteArray(reinterpret_cast<const char*>(buffer), notifySize));
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
            return ctx->suppressWrites
                ? ctx->virtualSize
                : ctx->file.size();

        whence &= ~AVSEEK_FORCE;

        qint64 position = offset;

        if (ctx->suppressWrites)
        {
            switch (whence)
            {
            case SEEK_SET:
                break;

            case SEEK_CUR:
                position += ctx->virtualPosition;
                break;

            case SEEK_END:
                position += ctx->virtualSize;
                break;

            default:
                return AVERROR(EINVAL);
            }

            if (position < 0)
                return AVERROR(EINVAL);

            ctx->virtualPosition = position;
            return position;
        }

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

        ctx->virtualPosition = position;
        ctx->virtualSize = std::max(ctx->virtualSize, position);
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

    if (destination_path.isEmpty())
        return false;

    QDir path(destination_path);
    if (!path.exists() && !path.mkpath(QStringLiteral(".")))
        return false;

    m_destinationPath = path.absolutePath();
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
    if (expected_size > 0)
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
    m_observer.store(observer);
}


// ============================================================================
// Filename
// ============================================================================

QString FFmpegMergeDownloader::makeOutputFilename(
    const QList<QUrl>& urls,
    const QString& filename, bool resume) const
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

    if (resume || (m_downloadNamePolicy == kReplaceFile))
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
    DownloaderObserverInterface* const observer = m_observer.load();
 
    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [observer, data]()
        {
            observer->onStart(data);
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyProgress(
    qint64 bytes)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [observer, bytes]()
        {
            observer->onProgress(bytes);
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifySpeed(
    qint64 bytesPerSecond)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [observer, bytesPerSecond]()
        {
            observer->onSpeed(bytesPerSecond);
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyFileCreated(
    const QString& filename)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [observer, filename]()
        {
            observer->onFileCreated(filename);
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyFileToBeReleased(
    const QString& filename)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [observer, filename]()
        {
            observer->onFileToBeReleased(filename);
        },
        Qt::QueuedConnection);
}


void FFmpegMergeDownloader::notifyFinished()
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [observer]()
        {
            observer->onFinished();
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyError(
    utilities::ErrorCode::ERROR_CODES code,
    const QString& description)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this,
        [observer, code, description]()
        {
            observer->onError(code, description);
        },
        Qt::QueuedConnection);
}


void FFmpegMergeDownloader::run(
    const QList<QUrl>& urls,
    QNetworkAccessManager* network_manager,
    const QString& filename,
    const QStringList& httpHeaders,
    bool resume)
{
    Q_UNUSED(network_manager);

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

    const QString outputFilename = makeOutputFilename(urls, filename, resume);

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
            [this, urls, outputFilename, resume, httpHeaders]()
            {
                mergeWorker(urls, outputFilename, resume, httpHeaders);
            });
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
    run(urls,
        network_manager,
        filename,
        httpHeaders,
        false);
}

void FFmpegMergeDownloader::Resume(
    const QList<QUrl>& urls,
    QNetworkAccessManager* network_manager,
    const QString& filename,
    const QStringList& httpHeaders)
{
    // Resume support is intentionally not implemented yet.
    run(urls,
        network_manager,
        filename,
        httpHeaders,
        true);
}

void FFmpegMergeDownloader::Pause()
{
    if (!m_running.load())
        return;

    // Interrupt FFmpeg's blocking network read just like Stop(), but keep
    // the distinction so the worker leaves the partial output in place.
    m_pauseRequested.store(true);
    m_stopRequested.store(true);
}

void FFmpegMergeDownloader::Stop()
{
    // Stop wins over a pending pause request.
    m_pauseRequested.store(false);
    m_stopRequested.store(true);
}


// ============================================================================
// Merge worker
// ============================================================================

void FFmpegMergeDownloader::mergeWorker(
    QList<QUrl> urls,
    QString outputFilename,
    bool resume,
    const QStringList& httpHeaders)
{
    auto finishWorker =
        [this]()
        {
            m_running.store(false);
        };

    auto openNetworkInput =
        [&](const QUrl& url, InputFormatPtr& result) -> int
        {
            AVFormatContext* raw = avformat_alloc_context();
            if (!raw)
                return AVERROR(ENOMEM);

            raw->interrupt_callback.opaque = &m_stopRequested;
            raw->interrupt_callback.callback = InterruptionRequested;

            AVDictionary* opts = nullptr;
            av_dict_set(&opts, "reconnect", "1", 0);
            av_dict_set(&opts, "reconnect_streamed", "1", 0);
            av_dict_set(&opts, "reconnect_delay_max", "10", 0);
            av_dict_set(&opts, "respect_retry_after", "1", 0);
            av_dict_set(&opts, "reconnect_on_http_error", "404,429,500,503", 0);

            QByteArray headerBlock;
            if (httpHeaders.isEmpty())
            {
                headerBlock =
                    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                    "AppleWebKit/537.36 (KHTML, like Gecko) "
                    "Chrome/126.0.0.0 Safari/537.36\r\n";
            }
            else
            {
                for (int i = 0; i + 1 < httpHeaders.size(); i += 2)
                {
                    headerBlock += httpHeaders[i].toUtf8();
                    headerBlock += ": ";
                    headerBlock += httpHeaders[i + 1].toUtf8();
                    headerBlock += "\r\n";
                }
            }
            if (!headerBlock.isEmpty())
                av_dict_set(&opts, "headers", headerBlock.constData(), 0);

            const QByteArray urlBytes = url.toString().toUtf8();
            const int r = avformat_open_input(
                &raw,
                urlBytes.constData(),
                nullptr,
                &opts);
            av_dict_free(&opts);

            if (r < 0)
            {
                if (raw)
                    avformat_close_input(&raw);
                return r;
            }

            result.reset(raw);
            return avformat_find_stream_info(result.get(), nullptr);
        };

    InputFormatPtr videoInput;
    InputFormatPtr audioInput;

    int ret = openNetworkInput(urls[0], videoInput);
    if (ret < 0)
    {
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDNETWORKERR,
            QStringLiteral("Could not open video input: %1")
                .arg(ffmpegErrorString(ret)));
        return;
    }

    ret = openNetworkInput(urls[1], audioInput);
    if (ret < 0)
    {
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDNETWORKERR,
            QStringLiteral("Could not open audio input: %1")
                .arg(ffmpegErrorString(ret)));
        return;
    }

    // The progress numerator is the actual number of bytes written to the
    // output file. Keep the denominator independent from it: for a merged
    // download it is the sum of the sizes of the video and audio inputs.
    // avio_size() uses the protocol's AVSEEK_SIZE support and does not consume
    // the input stream. If a server does not provide a size, leave an already
    // supplied expected size untouched.
    auto inputContentLength =
        [](const InputFormatPtr& input) -> qint64
        {
            if (!input || !input->pb)
                return -1;

            const int64_t size = avio_size(input->pb);
            return size > 0 ? static_cast<qint64>(size) : -1;
        };

    const qint64 videoSize = inputContentLength(videoInput);
    const qint64 audioSize = inputContentLength(audioInput);

    if (videoSize > 0 && audioSize > 0)
    {
        m_totalFileSize.store(videoSize + audioSize);
    }

    // ------------------------------------------------------------------------
    // Output file. Resume deliberately opens the existing file read/write;
    // nothing is written to it until the old content has been replayed.
    // ------------------------------------------------------------------------
    OutputContext outputContext;
    outputContext.owner = this;

    const auto openMode =
        resume
        ? QIODevice::ReadWrite
        : ((m_downloadNamePolicy == kReplaceFile)
            ? QIODevice::ReadWrite | QIODevice::Truncate
            : QIODevice::ReadWrite | QIODevice::NewOnly);

    outputContext.file.setFileName(outputFilename);

    if (!outputContext.file.open(openMode))
    {
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDOPENFILERR,
            QStringLiteral("Could not create output file '%1': %2")
                .arg(outputFilename, outputContext.file.errorString()));
        return;
    }

    const qint64 existingFileSize =
        resume ? outputContext.file.size() : 0;

    if (resume && existingFileSize <= 0)
    {
        outputContext.file.close();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Cannot resume an empty output file."));
        return;
    }

    if (!resume)
        notifyFileCreated(outputFilename);

    outputContext.timer.start();
    outputContext.bytesWritten = existingFileSize;
    outputContext.virtualPosition = 0;
    outputContext.virtualSize = 0;

    // ------------------------------------------------------------------------
    // Output format.
    // ------------------------------------------------------------------------
    AVFormatContext* output = nullptr;

    ret = avformat_alloc_output_context2(
        &output,
        nullptr,
        "matroska",
        nullptr);

    if (ret < 0 || !output)
    {
        outputContext.file.close();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Could not create Matroska output context: %1")
                .arg(ffmpegErrorString(ret)));
        return;
    }

    const int ioBufferSize = 32 * 1024;
    unsigned char* ioBuffer =
        static_cast<unsigned char*>(av_malloc(ioBufferSize));

    if (!ioBuffer)
    {
        avformat_free_context(output);
        outputContext.file.close();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Could not allocate output IO buffer."));
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
        outputContext.file.close();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Could not create output AVIO context."));
        return;
    }

    output->pb = outputIo;
    output->flags |= AVFMT_FLAG_CUSTOM_IO;

    // ------------------------------------------------------------------------
    // Transfer state.
    // ------------------------------------------------------------------------
    struct AudioBinding
    {
        int inputIndex = -1;
        int activeInputIndex = -1;
        AVStream* inputStream = nullptr;
        AVStream* activeStream = nullptr;
        AVStream* outputStream = nullptr;
        AVPacket* pendingPacket = nullptr;
        bool eof = false;
    };

    struct QueuedAudioPacket
    {
        AVPacket* packet = nullptr;
        int streamIndex = -1;
    };

    std::vector<AudioBinding> audioBindings;
    std::vector<QueuedAudioPacket> audioQueue;

    const int networkVideoStreamIndex =
        av_find_best_stream(
            videoInput.get(),
            AVMEDIA_TYPE_VIDEO,
            -1,
            -1,
            nullptr,
            0);

    if (networkVideoStreamIndex < 0)
    {
        avio_context_free(&outputIo);
        avformat_free_context(output);
        outputContext.file.close();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Could not find a video stream: %1")
                .arg(ffmpegErrorString(networkVideoStreamIndex)));
        return;
    }

    AVStream* inputVideoStream =
        videoInput->streams[networkVideoStreamIndex];

    for (unsigned int i = 0; i < audioInput->nb_streams; ++i)
    {
        AVStream* stream = audioInput->streams[i];
        if (!isAudioStream(stream))
            continue;

        AudioBinding binding;
        binding.inputIndex = static_cast<int>(i);
        binding.activeInputIndex = static_cast<int>(i);
        binding.inputStream = stream;
        binding.activeStream = stream;
        binding.pendingPacket = av_packet_alloc();

        if (!binding.pendingPacket)
        {
            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);
            avio_context_free(&outputIo);
            avformat_free_context(output);
            outputContext.file.close();
            finishWorker();
            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Could not allocate audio packet."));
            return;
        }

        audioBindings.push_back(binding);
    }

    if (audioBindings.empty())
    {
        avio_context_free(&outputIo);
        avformat_free_context(output);
        outputContext.file.close();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("The audio input contains no audio streams."));
        return;
    }

    AVStream* outputVideoStream = avformat_new_stream(output, nullptr);
    if (!outputVideoStream)
    {
        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);
        avio_context_free(&outputIo);
        avformat_free_context(output);
        outputContext.file.close();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Could not create output video stream."));
        return;
    }

    ret = avcodec_parameters_copy(
        outputVideoStream->codecpar,
        inputVideoStream->codecpar);

    if (ret < 0)
    {
        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);
        avio_context_free(&outputIo);
        avformat_free_context(output);
        outputContext.file.close();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Could not copy video codec parameters: %1")
                .arg(ffmpegErrorString(ret)));
        return;
    }

    outputVideoStream->codecpar->codec_tag = 0;
    outputVideoStream->time_base = inputVideoStream->time_base;
    outputVideoStream->sample_aspect_ratio = inputVideoStream->sample_aspect_ratio;
    outputVideoStream->disposition = inputVideoStream->disposition;
    av_dict_copy(&outputVideoStream->metadata, inputVideoStream->metadata, 0);

    for (auto& binding : audioBindings)
    {
        AVStream* outStream = avformat_new_stream(output, nullptr);
        if (!outStream)
        {
            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);
            avio_context_free(&outputIo);
            avformat_free_context(output);
            outputContext.file.close();
            finishWorker();
            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Could not create output audio stream."));
            return;
        }

        ret = avcodec_parameters_copy(
            outStream->codecpar,
            binding.inputStream->codecpar);

        if (ret < 0)
        {
            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);
            avio_context_free(&outputIo);
            avformat_free_context(output);
            outputContext.file.close();
            finishWorker();
            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Could not copy audio codec parameters: %1")
                    .arg(ffmpegErrorString(ret)));
            return;
        }

        outStream->codecpar->codec_tag = 0;
        outStream->time_base = binding.inputStream->time_base;
        outStream->sample_aspect_ratio = binding.inputStream->sample_aspect_ratio;
        outStream->disposition = binding.inputStream->disposition;
        av_dict_copy(&outStream->metadata, binding.inputStream->metadata, 0);
        binding.outputStream = outStream;
    }

    av_dict_copy(&output->metadata, videoInput->metadata, 0);
    output->avoid_negative_ts = AVFMT_AVOID_NEG_TS_DISABLED;

    // ------------------------------------------------------------------------
    // Resume inputs: two independent readers of the same old MKV. Keeping the
    // video and audio readers separate allows the normal lambdas to be reused.
    // ------------------------------------------------------------------------
    InputFormatPtr resumeVideoInput;
    InputFormatPtr resumeAudioInput;
    int resumeVideoStreamIndex = -1;
    std::vector<int> resumeAudioStreamIndices;

    if (resume)
    {
        const QByteArray filenameBytes =
            QFileInfo(outputFilename).absoluteFilePath().toUtf8();

        auto openResumeInput =
            [&](InputFormatPtr& result) -> int
            {
                AVFormatContext* raw = avformat_alloc_context();
                if (!raw)
                    return AVERROR(ENOMEM);

                raw->interrupt_callback.opaque = &m_stopRequested;
                raw->interrupt_callback.callback = InterruptionRequested;

                int r = avformat_open_input(
                    &raw,
                    filenameBytes.constData(),
                    nullptr,
                    nullptr);

                if (r < 0)
                {
                    if (raw)
                        avformat_close_input(&raw);
                    return r;
                }

                result.reset(raw);
                return avformat_find_stream_info(result.get(), nullptr);
            };

        ret = openResumeInput(resumeVideoInput);
        if (ret < 0)
        {
            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);
            avio_context_free(&outputIo);
            avformat_free_context(output);
            outputContext.file.close();
            finishWorker();
            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Could not open existing output for resume: %1")
                    .arg(ffmpegErrorString(ret)));
            return;
        }

        ret = openResumeInput(resumeAudioInput);
        if (ret < 0)
        {
            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);
            avio_context_free(&outputIo);
            avformat_free_context(output);
            outputContext.file.close();
            finishWorker();
            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Could not open existing audio data for resume: %1")
                    .arg(ffmpegErrorString(ret)));
            return;
        }

        resumeVideoStreamIndex =
            av_find_best_stream(
                resumeVideoInput.get(),
                AVMEDIA_TYPE_VIDEO,
                -1,
                -1,
                nullptr,
                0);

        if (resumeVideoStreamIndex < 0)
        {
            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);
            avio_context_free(&outputIo);
            avformat_free_context(output);
            outputContext.file.close();
            finishWorker();
            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Could not find video stream in existing output."));
            return;
        }

        for (unsigned int i = 0; i < resumeAudioInput->nb_streams; ++i)
        {
            if (isAudioStream(resumeAudioInput->streams[i]))
                resumeAudioStreamIndices.push_back(static_cast<int>(i));
        }

        if (resumeAudioStreamIndices.size() < audioBindings.size())
        {
            for (auto& a : audioBindings)
                av_packet_free(&a.pendingPacket);
            avio_context_free(&outputIo);
            avformat_free_context(output);
            outputContext.file.close();
            finishWorker();
            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Existing output does not contain all required audio streams."));
            return;
        }
    }

    AVFormatContext* activeVideoInput = videoInput.get();
    AVFormatContext* activeAudioInput = audioInput.get();
    int activeVideoStreamIndex = networkVideoStreamIndex;
    AVStream* activeVideoStream = inputVideoStream;

    AVPacket* pendingVideoPacket = av_packet_alloc();
    if (!pendingVideoPacket)
    {
        for (auto& a : audioBindings)
            av_packet_free(&a.pendingPacket);
        avio_context_free(&outputIo);
        avformat_free_context(output);
        outputContext.file.close();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Could not allocate video packet."));
        return;
    }

    bool videoEof = false;
    bool audioEof = false;
    bool replayMode = resume;
    bool headerWritten = false;

    std::vector<qint64> lastAudioTimestampUs(
        audioBindings.size(),
        std::numeric_limits<qint64>::min());
    qint64 lastVideoTimestampUs =
        std::numeric_limits<qint64>::min();

    auto freeAudioQueue =
        [&]()
        {
            for (auto& item : audioQueue)
                av_packet_free(&item.packet);
            audioQueue.clear();
        };

    auto cleanup =
        [&]()
        {
            av_packet_free(&pendingVideoPacket);
            freeAudioQueue();
            for (auto& binding : audioBindings)
                av_packet_free(&binding.pendingPacket);
            if (outputIo)
                avio_context_free(&outputIo);
            if (output)
                avformat_free_context(output);
            outputContext.file.close();
        };

    auto selectedAudioStream =
        [&](int streamIndex) -> AudioBinding*
        {
            for (auto& binding : audioBindings)
            {
                if (binding.activeInputIndex == streamIndex)
                    return &binding;
            }
            return nullptr;
        };

    auto readNextVideoPacket =
        [&]() -> bool
        {
            av_packet_unref(pendingVideoPacket);

            while (!videoEof)
            {
                if (m_stopRequested.load())
                    return false;

                ret = av_read_frame(activeVideoInput, pendingVideoPacket);

                if (ret == AVERROR_EOF || ret < 0)
                {
                    // In resume mode an error after successfully readable data
                    // is intentionally treated as EOF: the old tail may be
                    // incomplete/corrupt.
                    videoEof = true;
                    return false;
                }

                if (pendingVideoPacket->stream_index == activeVideoStreamIndex)
                {
                    if (replayMode)
                        return true;

                    const qint64 videoTimestamp = packetTimestampUs(pendingVideoPacket, activeVideoStream);
                    if (videoTimestamp == std::numeric_limits<qint64>::max()
                            || videoTimestamp > lastVideoTimestampUs)
                        return true;
                }
                av_packet_unref(pendingVideoPacket);
            }

            return false;
        };

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
                    if (!binding.pendingPacket || binding.pendingPacket->size <= 0)
                    {
                        allHavePacket = false;
                        break;
                    }
                }

                if (allHavePacket)
                    return;

                AVPacket* packet = av_packet_alloc();
                if (!packet)
                {
                    audioEof = true;
                    return;
                }

                const int readRet = av_read_frame(activeAudioInput, packet);

                if (readRet == AVERROR_EOF || readRet < 0)
                {
                    av_packet_free(&packet);
                    audioEof = true;
                    for (auto& binding : audioBindings)
                    {
                        if (!binding.pendingPacket || binding.pendingPacket->size <= 0)
                            binding.eof = true;
                    }
                    return;
                }

                AudioBinding* binding = selectedAudioStream(packet->stream_index);
                if (!binding)
                {
                    av_packet_free(&packet);
                    continue;
                }

                if (!replayMode)
                {
                    const qint64 timestamp = packetTimestampUs(packet, binding->activeStream);
                    if (timestamp != std::numeric_limits<qint64>::max())
                    {
                        const size_t audioIndex =
                            static_cast<size_t>(binding - audioBindings.data());
                        if (timestamp <= lastAudioTimestampUs[audioIndex])
                        {
                            av_packet_free(&packet);
                            continue;
                        }
                    }
                }

                if (binding->pendingPacket && binding->pendingPacket->size > 0)
                {
                    audioQueue.push_back({ packet, packet->stream_index });
                    continue;
                }

                av_packet_ref(binding->pendingPacket, packet);
                av_packet_free(&packet);
            }
        };

    auto promoteQueuedAudioPacket =
        [&](AudioBinding& binding)
        {
            if (binding.pendingPacket && binding.pendingPacket->size > 0)
                return;

            for (auto it = audioQueue.begin(); it != audioQueue.end(); ++it)
            {
                if (it->streamIndex != binding.activeInputIndex)
                    continue;

                av_packet_ref(binding.pendingPacket, it->packet);
                av_packet_free(&it->packet);
                audioQueue.erase(it);
                return;
            }

            if (audioEof)
                binding.eof = true;
        };

    // ------------------------------------------------------------------------
    // Same merge loop for both phases. In replayMode it consumes old packets
    // and records their timestamps but does not write a single output byte.
    // ------------------------------------------------------------------------
    auto transfer = [&]() -> bool
    {
        readNextVideoPacket();
        fillAudioPending();
        for (auto& binding : audioBindings)
            promoteQueuedAudioPacket(binding);

        while (!m_stopRequested.load())
        {
            if (!videoEof &&
                (!pendingVideoPacket || pendingVideoPacket->size <= 0))
                readNextVideoPacket();

            fillAudioPending();
            for (auto& binding : audioBindings)
                promoteQueuedAudioPacket(binding);

            AudioBinding* selectedAudio = nullptr;
            qint64 selectedAudioTimestamp =
                std::numeric_limits<qint64>::max();

            for (auto& binding : audioBindings)
            {
                if (binding.eof ||
                    !binding.pendingPacket ||
                    binding.pendingPacket->size <= 0)
                    continue;

                const qint64 ts =
                    packetTimestampUs(
                        binding.pendingPacket,
                        binding.activeStream);

                if (ts < selectedAudioTimestamp)
                {
                    selectedAudioTimestamp = ts;
                    selectedAudio = &binding;
                }
            }

            const bool haveVideoPacket =
                !videoEof && pendingVideoPacket && pendingVideoPacket->size > 0;

            const qint64 videoTimestamp =
                haveVideoPacket
                ? packetTimestampUs(pendingVideoPacket, activeVideoStream)
                : std::numeric_limits<qint64>::max();

            if (!haveVideoPacket && !selectedAudio)
                break;

            const bool writeVideo =
                haveVideoPacket &&
                (!selectedAudio || videoTimestamp <= selectedAudioTimestamp);

            if (writeVideo)
            {
                AVPacket* packet = pendingVideoPacket;
                const qint64 ts = videoTimestamp;

                if (replayMode || ts > lastVideoTimestampUs)
                {
                    if (ts != std::numeric_limits<qint64>::max())
                        lastVideoTimestampUs = ts;

                    packet->stream_index = outputVideoStream->index;
                    av_packet_rescale_ts(
                        packet,
                        activeVideoStream->time_base,
                        outputVideoStream->time_base);

                    ret = av_interleaved_write_frame(output, packet);
                    if (ret < 0)
                    {
                        cleanup();
                        finishWorker();
                        notifyError(
                            utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Could not write video packet: %1")
                                .arg(ffmpegErrorString(ret)));
                        return false;
                    }

                    av_packet_unref(packet);
                }

                if (!readNextVideoPacket())
                    videoEof = true;
            }
            else
            {
                AudioBinding& binding = *selectedAudio;
                AVPacket* packet = binding.pendingPacket;
                const size_t audioIndex =
                    static_cast<size_t>(&binding - audioBindings.data());
                const qint64 ts = selectedAudioTimestamp;

                if (replayMode || ts > lastAudioTimestampUs[audioIndex])
                {
                    if (ts != std::numeric_limits<qint64>::max())
                        lastAudioTimestampUs[audioIndex] = ts;

                    packet->stream_index = binding.outputStream->index;
                    av_packet_rescale_ts(
                        packet,
                        binding.activeStream->time_base,
                        binding.outputStream->time_base);

                    ret = av_interleaved_write_frame(output, packet);
                    if (ret < 0)
                    {
                        cleanup();
                        finishWorker();
                        notifyError(
                            utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Could not write audio packet: %1")
                                .arg(ffmpegErrorString(ret)));
                        return false;
                    }

                    av_packet_unref(packet);
                    promoteQueuedAudioPacket(binding);
                }
            }

        }

        return true;
    };

    // ------------------------------------------------------------------------
    // Header. Resume suppresses the physical header bytes because the old
    // Matroska header is already in the existing file.
    // ------------------------------------------------------------------------
    outputContext.suppressWrites = resume;
    outputContext.virtualPosition = 0;
    outputContext.virtualSize = 0;

    ret = avformat_write_header(output, nullptr);
    if (ret < 0)
    {
        cleanup();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Could not initialize Matroska output: %1")
                .arg(ffmpegErrorString(ret)));
        return;
    }

    headerWritten = true;

    // ------------------------------------------------------------------------
    // Replay the old file before allowing any output write.
    // ------------------------------------------------------------------------
    if (resume)
    {
        activeVideoInput = resumeVideoInput.get();
        activeAudioInput = resumeAudioInput.get();
        activeVideoStreamIndex = resumeVideoStreamIndex;
        activeVideoStream = resumeVideoInput->streams[resumeVideoStreamIndex];

        for (size_t i = 0; i < audioBindings.size(); ++i)
        {
            audioBindings[i].activeInputIndex = resumeAudioStreamIndices[i];
            audioBindings[i].activeStream =
                resumeAudioInput->streams[resumeAudioStreamIndices[i]];
            audioBindings[i].eof = false;
            av_packet_unref(audioBindings[i].pendingPacket);
        }

        videoEof = false;
        audioEof = false;

        if (!transfer())
            return;

        av_packet_unref(pendingVideoPacket);
        freeAudioQueue();
        for (auto& binding : audioBindings)
        {
            av_packet_unref(binding.pendingPacket);
            binding.eof = false;
        }

        // --------------------------------------------------------------------
        // Remove the old trailer/incomplete tail only now, after the old data
        // has been completely replayed. Thus resume performs no physical
        // output write while it is replaying the old content.
        // --------------------------------------------------------------------
#if 0
        const qint64 appendPosition =
            findMatroskaAppendPosition(outputContext.file);

        if (appendPosition <= 0)
        {
            cleanup();
            finishWorker();
            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral(
                    "Could not find a complete Matroska Cluster in the existing output."));
            return;
        }

        if (!outputContext.file.resize(appendPosition))
        {
            cleanup();
            finishWorker();
            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral(
                    "Could not remove the incomplete Matroska tail before resume."));
            return;
        }
#endif

        // --------------------------------------------------------------------
        // Return to network inputs and seek back slightly. We intentionally
        // re-download this overlap and discard packets already present in the
        // old file. A two-second overlap is small but gives HTTP seeks room to
        // land on a useful video keyframe.
        // --------------------------------------------------------------------
        constexpr qint64 kResumeOverlapUs = 2 * 1000 * 1000;

        activeVideoInput = videoInput.get();
        activeAudioInput = audioInput.get();
        activeVideoStreamIndex = networkVideoStreamIndex;
        activeVideoStream = inputVideoStream;

        for (auto& binding : audioBindings)
        {
            binding.activeInputIndex = binding.inputIndex;
            binding.activeStream = binding.inputStream;
        }

        videoEof = false;
        audioEof = false;

        if (lastVideoTimestampUs != std::numeric_limits<qint64>::min())
        {
            const qint64 target =
                std::max<qint64>(0, lastVideoTimestampUs - kResumeOverlapUs);

            qDebug().noquote()
                << "Video resume seek:"
                << "last=" << lastVideoTimestampUs / 1000000.0 << "s"
                << "target=" << target / 1000000.0 << "s"
                << "overlap=" << kResumeOverlapUs / 1000000.0 << "s";

            const int seekRet = avformat_seek_file(
                videoInput.get(),
                -1,
                std::numeric_limits<int64_t>::min(),
                target,
                std::numeric_limits<int64_t>::max(),
                AVSEEK_FLAG_BACKWARD);

            qDebug().noquote()
                << "Video resume seek result:"
                << seekRet
                << (seekRet < 0
                    ? ffmpegErrorString(seekRet)
                    : QStringLiteral("OK"));
        }

        qint64 audioResumeTimestamp =
            std::numeric_limits<qint64>::max();

        for (const qint64 ts : lastAudioTimestampUs)
        {
            if (ts != std::numeric_limits<qint64>::min())
                audioResumeTimestamp =
                std::min(audioResumeTimestamp, ts);
        }

        if (audioResumeTimestamp != std::numeric_limits<qint64>::max())
        {
            const qint64 target =
                std::max<qint64>(0, audioResumeTimestamp - kResumeOverlapUs);

            qDebug().noquote()
                << "Audio resume seek:"
                << "last=" << audioResumeTimestamp / 1000000.0 << "s"
                << "target=" << target / 1000000.0 << "s"
                << "overlap=" << kResumeOverlapUs / 1000000.0 << "s";

            const int seekRet = avformat_seek_file(
                audioInput.get(),
                -1,
                std::numeric_limits<int64_t>::min(),
                target,
                std::numeric_limits<int64_t>::max(),
                AVSEEK_FLAG_BACKWARD);

            qDebug().noquote()
                << "Audio resume seek result:"
                << seekRet
                << (seekRet < 0
                    ? ffmpegErrorString(seekRet)
                    : QStringLiteral("OK"));
        }

        // Nothing has been physically written so far. Now move the QFile to
        // its existing end and switch the AVIO into real-write mode.
        //avio_flush(output->pb);
        outputContext.suppressWrites = false;
        //outputContext.virtualPosition = appendPosition;
        //outputContext.virtualSize = appendPosition;

        if (!outputContext.file.seek(outputContext.virtualPosition))//appendPosition))
        {
            cleanup();
            finishWorker();
            notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Could not seek output to the end for resume."));
            return;
        }

        outputIo->pos = outputContext.virtualPosition;//appendPosition;
        //outputIo->buf_ptr = outputIo->buffer;
        //outputIo->buf_end = outputIo->buffer;
        outputIo->eof_reached = 0;

        replayMode = false;

        if (!transfer())
            return;
    }
    else
    {
        outputContext.suppressWrites = false;
        replayMode = false;

        if (!transfer())
            return;
    }

    if (m_pauseRequested.load())
    {
        // Do not write a Matroska trailer: the partial file is deliberately
        // left in the same resumable form as a paused plain Downloader.
        avio_flush(output->pb);
        outputContext.file.flush();
        cleanup();
        finishWorker();
        return;
    }

    if (m_stopRequested.load())
    {
        const QString filename = outputContext.file.fileName();
        cleanup();
        if (QFile::exists(filename))
        {
            notifyFileToBeReleased(filename);
            QFile::remove(filename);
        }
        finishWorker();
        return;
    }

    ret = headerWritten ? av_write_trailer(output) : 0;
    if (ret < 0)
    {
        cleanup();
        finishWorker();
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Could not finalize Matroska file: %1")
                .arg(ffmpegErrorString(ret)));
        return;
    }

    avio_flush(output->pb);
    outputContext.file.flush();

    const qint64 finalSize = outputContext.file.size();
    m_totalFileSize.store(finalSize);
    notifyProgress(finalSize);

    if (outputContext.timer.elapsed() > 0)
    {
        const qint64 speed =
            static_cast<qint64>(
                (static_cast<double>(finalSize) * 1000.0) /
                static_cast<double>(outputContext.timer.elapsed()));
        notifySpeed(speed);
    }

    cleanup();
    finishWorker();
    notifyFinished();
}
