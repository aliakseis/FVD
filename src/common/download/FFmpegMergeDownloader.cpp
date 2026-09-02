#include "FFmpegMergeDownloader.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QByteArray>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/dict.h>
#include <libavutil/mem.h>
}

#include <algorithm>
#include <limits>
#include <vector>
#include <cstring>

// Helper functions

namespace
{

constexpr int kIoBufferSize = 64 * 1024;

static QString toQString(const char* s)
{
    if (!s)
        return QString();

    return QString::fromUtf8(s);
}

static QString avErrorToQString(int error)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};

    av_strerror(
        error,
        buffer,
        sizeof(buffer));

    return QString::fromUtf8(buffer);
}

static qint64 packetTimestampUs(
    const AVPacket* packet,
    const AVStream* stream)
{
    int64_t ts = packet->dts;

    if (ts == AV_NOPTS_VALUE)
        ts = packet->pts;

    if (ts == AV_NOPTS_VALUE)
        return std::numeric_limits<qint64>::max();

    return av_rescale_q(
        ts,
        stream->time_base,
        AVRational{1, 1000000});
}

}

// Output AVIO

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

    explicit OutputContext(
        FFmpegMergeDownloader* downloader)
        : owner(downloader)
    {
        timer.start();
    }

    static int writePacket(
        void* opaque,
        uint8_t* buffer,
        int size)
    {
        auto* ctx =
            static_cast<OutputContext*>(opaque);

        if (!ctx ||
            !buffer ||
            size <= 0)
        {
            return 0;
        }

        const qint64 written =
            ctx->file.write(
                reinterpret_cast<const char*>(buffer),
                size);

        if (written != size)
        {
            ctx->writeError = true;
            return AVERROR(EIO);
        }

        ctx->bytesWritten += written;

        ctx->reportProgress();

        return static_cast<int>(written);
    }

static int64_t seek(
    void* opaque,
    int64_t offset,
    int whence)
{
    auto* ctx =
        static_cast<OutputContext*>(opaque);

    if (!ctx)
        return AVERROR(EINVAL);

    if (whence == AVSEEK_SIZE)
        return ctx->file.size();

    switch (whence & ~AVSEEK_FORCE)
    {
    case SEEK_SET:
        break;

    case SEEK_CUR:
        offset += ctx->file.pos();
        break;

    case SEEK_END:
        offset += ctx->file.size();
        break;

    default:
        return AVERROR(EINVAL);
    }

    if (offset < 0)
        return AVERROR(EINVAL);

    if (!ctx->file.seek(offset))
        return AVERROR(EIO);

    return offset;
}

    void reportProgress()
    {
        if (!owner)
            return;

        const qint64 now =
            timer.elapsed();

        // Avoid flooding the Qt event queue.
        if (now - lastReportMs < 100)
            return;

        lastReportMs = now;

        owner->notifyProgress(bytesWritten);

        if (now > 0)
        {
            const qint64 speed =
                (bytesWritten * 1000) / now;

            owner->notifySpeed(speed);
        }

        lastReportedBytes = bytesWritten;
    }
};

// Constructor/destructor

FFmpegMergeDownloader::FFmpegMergeDownloader(
    QObject* parent)
    : QObject(parent)
{
}

FFmpegMergeDownloader::~FFmpegMergeDownloader()
{
    Stop();

    if (m_worker.joinable())
        m_worker.join();
}

// Basic properties

const QString&
FFmpegMergeDownloader::destinationPath() const
{
    return m_destinationPath;
}

bool FFmpegMergeDownloader::setDestinationPath(
    const QString& destination_path)
{
    m_destinationPath = destination_path;
    if (m_destinationPath.isEmpty())
    {
        return false;
    }
    QDir path(m_destinationPath);
    if (!path.exists(m_destinationPath) && !path.mkpath(m_destinationPath))
    {
        m_destinationPath.clear();
        return false;
    }
    return true;
}

qint64 FFmpegMergeDownloader::totalFileSize() const
{
    QMutexLocker locker(&m_mutex);
    return m_totalFileSize;
}

void FFmpegMergeDownloader::setTotalFileSize(
    qint64 value)
{
    QMutexLocker locker(&m_mutex);
    m_totalFileSize = value;
}

void FFmpegMergeDownloader::setExpectedFileSize(
    qint64 expected_size)
{
    QMutexLocker locker(&m_mutex);
    m_expectedFileSize = expected_size;
}

int FFmpegMergeDownloader::speedLimit() const
{
    return m_speedLimit;
}

void FFmpegMergeDownloader::setSpeedLimit(
    int value)
{
    m_speedLimit = value;
}

void FFmpegMergeDownloader::setDownloadNamePolicy(
    DuplicateDownloadNamePolicy policy)
{
    m_namePolicy = policy;
}

void FFmpegMergeDownloader::setObserver(
    DownloaderObserverInterface* observer)
{
    QMutexLocker locker(&m_mutex);
    m_observer = observer;
}

// Output filename handling

QString FFmpegMergeDownloader::makeUniqueFilename(
    const QString& filename) const
{
    if (m_namePolicy == kReplaceFile)
        return filename;

    QFileInfo info(filename);

    if (!info.exists())
        return filename;

    const QString directory =
        info.absolutePath();

    const QString base =
        info.completeBaseName();

    const QString suffix =
        info.completeSuffix();

    for (int i = 1;
         i < std::numeric_limits<int>::max();
         ++i)
    {
        QString candidate =
            directory +
            QDir::separator() +
            base +
            QStringLiteral("(") +
            QString::number(i) +
            QStringLiteral(")");

        if (!suffix.isEmpty())
            candidate +=
                QStringLiteral(".") + suffix;

        if (!QFileInfo::exists(candidate))
            return candidate;
    }

    return QString();
}

QString FFmpegMergeDownloader::makeOutputFilename(
    const QString& requestedFilename,
    const QUrl& videoUrl) const
{
    QString filename =
        requestedFilename.trimmed();

    if (filename.isEmpty())
    {
        filename =
            QFileInfo(
                videoUrl.path()).fileName();

        if (filename.isEmpty())
            filename = QStringLiteral("output.mkv");

        QFileInfo info(filename);

        filename =
            info.completeBaseName() +
            QStringLiteral(".mkv");
    }
    else
    {
        QFileInfo info(filename);

        if (info.suffix().isEmpty())
        {
            filename +=
                QStringLiteral(".mkv");
        }
        else if (info.suffix().compare(
                     QStringLiteral("mkv"),
                     Qt::CaseInsensitive) != 0)
        {
            filename =
                info.completeBaseName() +
                QStringLiteral(".mkv");
        }
    }

    QString fullPath;

    if (QFileInfo(filename).isAbsolute())
        fullPath = filename;
    else
        fullPath =
            QDir(m_destinationPath).filePath(filename);

    return makeUniqueFilename(
        QDir::cleanPath(fullPath));
}

// FFmpeg error handling

QString FFmpegMergeDownloader::ffmpegErrorString(
    int error)
{
    return avErrorToQString(error);
}

utilities::ErrorCode::ERROR_CODES
FFmpegMergeDownloader::mapFfmpegError(
    int error)
{
    switch (error)
    {
    case AVERROR_HTTP_BAD_REQUEST:
    case AVERROR_HTTP_UNAUTHORIZED:
    case AVERROR_HTTP_FORBIDDEN:
    case AVERROR_HTTP_NOT_FOUND:
    //case AVERROR_HTTP_TOO_MANY_REQUESTS:
    case AVERROR_HTTP_OTHER_4XX:
    case AVERROR_HTTP_SERVER_ERROR:
        return utilities::ErrorCode::eDOWLDHTTPCODERR;

    case AVERROR(EIO):
    case AVERROR(ETIMEDOUT):
    case AVERROR(ECONNRESET):
    case AVERROR(ECONNREFUSED):
    case AVERROR(ENETDOWN):
    case AVERROR(ENETUNREACH):
    case AVERROR(ENETRESET):
    case AVERROR(EHOSTUNREACH):
        return utilities::ErrorCode::eDOWLDNETWORKERR;

    case AVERROR(ENOENT):
    case AVERROR(EACCES):
        return utilities::ErrorCode::eDOWLDOPENFILERR;

    default:
        return utilities::ErrorCode::eDOWLDUNKWNFILERR;
    }
}

// Opening an input

bool FFmpegMergeDownloader::openInput(
    const QUrl& url,
    AVFormatContext** context,
    QString& errorDescription,
    utilities::ErrorCode::ERROR_CODES& errorCode)
{
    *context = nullptr;

    const QByteArray encodedUrl =
        url.toEncoded();

    int ret =
        avformat_open_input(
            context,
            encodedUrl.constData(),
            nullptr,
            nullptr);

    if (ret < 0)
    {
        errorDescription =
            QStringLiteral(
                "Could not open input '%1': %2")
                .arg(
                    url.toString(),
                    ffmpegErrorString(ret));

        errorCode = mapFfmpegError(ret);

        return false;
    }

    ret =
        avformat_find_stream_info(
            *context,
            nullptr);

    if (ret < 0)
    {
        errorDescription =
            QStringLiteral(
                "Could not read stream information from '%1': %2")
                .arg(
                    url.toString(),
                    ffmpegErrorString(ret));

        errorCode = mapFfmpegError(ret);

        avformat_close_input(context);

        return false;
    }

    return true;
}

// Starting the worker

void FFmpegMergeDownloader::Start(
    const QList<QUrl>& urls,
    QNetworkAccessManager* network_manager,
    const QString& filename,
    const QStringList& httpHeaders)
{
    Q_UNUSED(network_manager);
    Q_UNUSED(httpHeaders);

    if (urls.size() != 2)
    {
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "FFmpeg merger requires exactly "
                "two URLs: video and audio."));

        return;
    }

    if (m_running.exchange(true))
    {
        notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral(
                "A merge operation is already running."));

        return;
    }

    m_stopRequested = false;
    m_pauseRequested = false;

    if (m_worker.joinable())
        m_worker.join();

    const QString outputFilename =
        makeOutputFilename(
            filename,
            urls[0]);

    if (outputFilename.isEmpty())
    {
        m_running = false;

        notifyError(
            utilities::ErrorCode::eDOWLDOPENFILERR,
            QStringLiteral(
                "Could not determine output filename."));

        return;
    }

    m_worker =
        std::thread(
            [this, urls, outputFilename]()
            {
                workerMain(
                    urls,
                    outputFilename);
            });
}

// Worker

void FFmpegMergeDownloader::workerMain(
    QList<QUrl> urls,
    QString outputFilename)
{
    QString errorDescription;

    utilities::ErrorCode::ERROR_CODES errorCode =
        utilities::ErrorCode::eNOTERROR;

    const bool ok =
        merge(
            urls[0],
            urls[1],
            outputFilename,
            errorDescription,
            errorCode);

    if (ok)
        notifyFinished();
    else
        notifyError(
            errorCode,
            errorDescription);

    m_running = false;
}

// The actual FFmpeg merge

bool FFmpegMergeDownloader::merge(
    const QUrl& videoUrl,
    const QUrl& audioUrl,
    const QString& outputFilename,
    QString& errorDescription,
    utilities::ErrorCode::ERROR_CODES& errorCode)
{
    AVFormatContext* videoInput = nullptr;
    AVFormatContext* audioInput = nullptr;
    AVFormatContext* output = nullptr;

    AVIOContext* outputIo = nullptr;

    OutputContext outputContext(this);

    bool success = false;

    do
    {
        //
        // Open video.
        //
        if (!openInput(
                videoUrl,
                &videoInput,
                errorDescription,
                errorCode))
        {
            break;
        }

        //
        // Open audio.
        //
        if (!openInput(
                audioUrl,
                &audioInput,
                errorDescription,
                errorCode))
        {
            break;
        }

        //
        // Find best video.
        //
        const int videoStreamIndex =
            av_find_best_stream(
                videoInput,
                AVMEDIA_TYPE_VIDEO,
                -1,
                -1,
                nullptr,
                0);

        if (videoStreamIndex < 0)
        {
            errorDescription =
                QStringLiteral(
                    "No video stream was found in '%1': %2")
                    .arg(
                        videoUrl.toString(),
                        ffmpegErrorString(
                            videoStreamIndex));

            errorCode =
                utilities::ErrorCode::eDOWLDUNKWNFILERR;

            break;
        }

        //
        // Verify audio.
        //
        bool haveAudio = false;

        for (unsigned i = 0;
             i < audioInput->nb_streams;
             ++i)
        {
            if (audioInput->streams[i]->codecpar->codec_type ==
                AVMEDIA_TYPE_AUDIO)
            {
                haveAudio = true;
                break;
            }
        }

        if (!haveAudio)
        {
            errorDescription =
                QStringLiteral(
                    "No audio stream was found in '%1'.")
                    .arg(audioUrl.toString());

            errorCode =
                utilities::ErrorCode::eDOWLDUNKWNFILERR;

            break;
        }

        //
        // Estimate total size.
        //
        qint64 estimatedSize = 0;
        bool haveEstimate = false;

        auto estimateInputSize =
            [](AVFormatContext* input) -> qint64
        {
            if (!input || !input->pb)
                return -1;

            const int64_t size =
                avio_size(input->pb);

            return size > 0 ? size : -1;
        };

        const qint64 videoSize =
            estimateInputSize(videoInput);

        const qint64 audioSize =
            estimateInputSize(audioInput);

        if (videoSize >= 0)
        {
            estimatedSize += videoSize;
            haveEstimate = true;
        }

        if (audioSize >= 0)
        {
            estimatedSize += audioSize;
            haveEstimate = true;
        }

        if (haveEstimate)
        {
            // MKV overhead means the estimate should not
            // be treated as an exact final size.
            estimatedSize +=
                std::max<qint64>(
                    1024 * 1024,
                    estimatedSize / 100);

            outputContext.estimatedSize =
                estimatedSize;

            setTotalFileSize(
                estimatedSize);
        }
        else
        {
            setTotalFileSize(-1);
        }

        //
        // Create Matroska output context.
        //
        const int ret =
            avformat_alloc_output_context2(
                &output,
                nullptr,
                "matroska",
                nullptr);

        if (ret < 0 || !output)
        {
            errorDescription =
                QStringLiteral(
                    "Could not create Matroska output: %1")
                    .arg(ffmpegErrorString(ret));

            errorCode =
                utilities::ErrorCode::eDOWLDOPENFILERR;

            break;
        }

        //
        // Preserve timestamps rather than asking the muxer
        // to normalize them.
        //
        output->avoid_negative_ts =
            AVFMT_AVOID_NEG_TS_DISABLED;

        //
        // Create output video stream.
        //
        AVStream* outputVideo =
            avformat_new_stream(
                output,
                nullptr);

        if (!outputVideo)
        {
            errorDescription =
                QStringLiteral(
                    "Could not create output video stream.");

            errorCode =
                utilities::ErrorCode::eDOWLDUNKWNFILERR;

            break;
        }

        AVStream* inputVideo =
            videoInput->streams[
                videoStreamIndex];

        int retCopy =
            avcodec_parameters_copy(
                outputVideo->codecpar,
                inputVideo->codecpar);

        if (retCopy < 0)
        {
            errorDescription =
                QStringLiteral(
                    "Could not copy video codec parameters: %1")
                    .arg(ffmpegErrorString(retCopy));

            errorCode =
                utilities::ErrorCode::eDOWLDUNKWNFILERR;

            break;
        }

        outputVideo->time_base =
            inputVideo->time_base;

        outputVideo->disposition =
            inputVideo->disposition;

        av_dict_copy(
            &outputVideo->metadata,
            inputVideo->metadata,
            0);

        //
        // Map audio streams.
        //
        std::vector<int> audioInputToOutput(
            audioInput->nb_streams,
            -1);

        for (unsigned i = 0;
             i < audioInput->nb_streams;
             ++i)
        {
            AVStream* inputStream =
                audioInput->streams[i];

            if (inputStream->codecpar->codec_type !=
                AVMEDIA_TYPE_AUDIO)
            {
                continue;
            }

            AVStream* outputStream =
                avformat_new_stream(
                    output,
                    nullptr);

            if (!outputStream)
            {
                errorDescription =
                    QStringLiteral(
                        "Could not create output audio stream.");

                errorCode =
                    utilities::ErrorCode::eDOWLDUNKWNFILERR;

                break;
            }

            retCopy =
                avcodec_parameters_copy(
                    outputStream->codecpar,
                    inputStream->codecpar);

            if (retCopy < 0)
            {
                errorDescription =
                    QStringLiteral(
                        "Could not copy audio codec parameters: %1")
                        .arg(
                            ffmpegErrorString(retCopy));

                errorCode =
                    utilities::ErrorCode::eDOWLDUNKWNFILERR;

                break;
            }

            outputStream->time_base =
                inputStream->time_base;

            outputStream->disposition =
                inputStream->disposition;

            av_dict_copy(
                &outputStream->metadata,
                inputStream->metadata,
                0);

            audioInputToOutput[i] =
                outputStream->index;
        }

        if (!errorDescription.isEmpty())
            break;

        //
        // Open QFile.
        //
        outputContext.file.setFileName(
            outputFilename);

        if (!outputContext.file.open(
                QIODevice::ReadWrite |
                QIODevice::Truncate))
        {
            errorDescription =
                QStringLiteral(
                    "Could not create output file '%1': %2")
                    .arg(
                        outputFilename,
                        outputContext.file.errorString());

            errorCode =
                utilities::ErrorCode::eDOWLDOPENFILERR;

            break;
        }

        //
        // Notify immediately after the QFile has been created.
        //
        notifyFileCreated(outputFilename);

        //
        // Create custom AVIO buffer.
        //
        unsigned char* ioBuffer =
            static_cast<unsigned char*>(
                av_malloc(kIoBufferSize));

        if (!ioBuffer)
        {
            errorDescription =
                QStringLiteral(
                    "Could not allocate FFmpeg I/O buffer.");

            errorCode =
                utilities::ErrorCode::eDOWLDUNKWNFILERR;

            break;
        }

        outputIo =
            avio_alloc_context(
                ioBuffer,
                kIoBufferSize,
                1,
                &outputContext,
                nullptr,
                &OutputContext::writePacket,
                &OutputContext::seek);

        if (!outputIo)
        {
            av_free(ioBuffer);

            errorDescription =
                QStringLiteral(
                    "Could not create FFmpeg AVIO context.");

            errorCode =
                utilities::ErrorCode::eDOWLDUNKWNFILERR;

            break;
        }

        output->pb = outputIo;

        //
        // The output is already supplied with AVIO.
        //
        output->flags |= AVFMT_FLAG_CUSTOM_IO;

        //
        // Write Matroska header.
        //
        int retHeader =
            avformat_write_header(
                output,
                nullptr);

        if (retHeader < 0)
        {
            errorDescription =
                QStringLiteral(
                    "Could not write Matroska header: %1")
                    .arg(
                        ffmpegErrorString(retHeader));

            errorCode =
                mapFfmpegError(retHeader);

            break;
        }

        //
        // Dummy onStart for now.
        //
        {
            QMutexLocker locker(&m_mutex);

            if (m_observer)
            {
                auto* observer = m_observer;

                QMetaObject::invokeMethod(
                    this,
                    [observer]()
                    {
                        observer->onStart(QByteArray());
                    },
                    Qt::QueuedConnection);
            }
        }

        //
        // Packet state.
        //
        AVPacket* videoPacket =
            av_packet_alloc();

        AVPacket* audioPacket =
            av_packet_alloc();

        if (!videoPacket || !audioPacket)
        {
            if (videoPacket)
                av_packet_free(&videoPacket);

            if (audioPacket)
                av_packet_free(&audioPacket);

            errorDescription =
                QStringLiteral(
                    "Could not allocate FFmpeg packet.");

            errorCode =
                utilities::ErrorCode::eDOWLDUNKWNFILERR;

            break;
        }

        bool videoAvailable = false;
        bool audioAvailable = false;

        bool videoFinished = false;
        bool audioFinished = false;

        //
        // Helper: read next relevant video packet.
        //
        auto readVideoPacket =
            [&]() -> int
        {
            av_packet_unref(videoPacket);

            while (true)
            {
                const int r =
                    av_read_frame(
                        videoInput,
                        videoPacket);

                if (r < 0)
                {
                    videoFinished = true;
                    return r;
                }

                if (videoPacket->stream_index ==
                    videoStreamIndex)
                {
                    videoAvailable = true;
                    return 0;
                }

                av_packet_unref(videoPacket);
            }
        };

        //
        // Helper: read next relevant audio packet.
        //
        auto readAudioPacket =
            [&]() -> int
        {
            av_packet_unref(audioPacket);

            while (true)
            {
                const int r =
                    av_read_frame(
                        audioInput,
                        audioPacket);

                if (r < 0)
                {
                    audioFinished = true;
                    return r;
                }

                const int inputIndex =
                    audioPacket->stream_index;

                if (inputIndex >= 0 &&
                    inputIndex <
                        static_cast<int>(
                            audioInputToOutput.size()) &&
                    audioInputToOutput[inputIndex] >= 0)
                {
                    audioAvailable = true;
                    return 0;
                }

                av_packet_unref(audioPacket);
            }
        };

        //
        // Prime both sources.
        //
        readVideoPacket();
        readAudioPacket();

        //
        // Merge in timestamp order.
        //
        while (videoAvailable ||
               audioAvailable)
        {
            if (m_stopRequested)
            {
                errorDescription =
                    QStringLiteral(
                        "Merge stopped.");

                errorCode =
                    utilities::ErrorCode::eDOWLDUNKWNFILERR;

                break;
            }

            AVPacket* packet = nullptr;
            AVStream* inputStream = nullptr;
            AVStream* outputStream = nullptr;

            bool fromVideo = false;

            if (!audioAvailable)
            {
                packet = videoPacket;
                inputStream = inputVideo;
                outputStream = outputVideo;
                fromVideo = true;
            }
            else if (!videoAvailable)
            {
                packet = audioPacket;

                inputStream =
                    audioInput->streams[
                        packet->stream_index];

                outputStream =
                    output->streams[
                        audioInputToOutput[
                            packet->stream_index]];

                fromVideo = false;
            }
            else
            {
                const qint64 videoTs =
                    packetTimestampUs(
                        videoPacket,
                        inputVideo);

                const AVStream* audioInputStream =
                    audioInput->streams[
                        audioPacket->stream_index];

                const qint64 audioTs =
                    packetTimestampUs(
                        audioPacket,
                        audioInputStream);

                if (videoTs <= audioTs)
                {
                    packet = videoPacket;
                    inputStream = inputVideo;
                    outputStream = outputVideo;
                    fromVideo = true;
                }
                else
                {
                    packet = audioPacket;

                    inputStream =
                        const_cast<AVStream*>(
                            audioInputStream);

                    outputStream =
                        output->streams[
                            audioInputToOutput[
                                packet->stream_index]];

                    fromVideo = false;
                }
            }

            av_packet_rescale_ts(
                packet,
                inputStream->time_base,
                outputStream->time_base);

            packet->stream_index =
                outputStream->index;

            const int writeResult =
                av_interleaved_write_frame(
                    output,
                    packet);

            if (writeResult < 0)
            {
                errorDescription =
                    QStringLiteral(
                        "Could not write packet to Matroska: %1")
                        .arg(
                            ffmpegErrorString(
                                writeResult));

                errorCode =
                    mapFfmpegError(writeResult);

                break;
            }

            if (fromVideo)
            {
                videoAvailable = false;

                if (!videoFinished)
                    readVideoPacket();
            }
            else
            {
                audioAvailable = false;

                if (!audioFinished)
                    readAudioPacket();
            }
        }

        av_packet_free(&videoPacket);
        av_packet_free(&audioPacket);

        if (!errorDescription.isEmpty())
            break;

        //
        // Finalize Matroska.
        //
        const int trailerResult =
            av_write_trailer(output);

        if (trailerResult < 0)
        {
            errorDescription =
                QStringLiteral(
                    "Could not finalize Matroska file: %1")
                    .arg(
                        ffmpegErrorString(
                            trailerResult));

            errorCode =
                mapFfmpegError(trailerResult);

            break;
        }

        //
        // Flush QFile and obtain the actual final size.
        //
        if (!outputContext.file.flush())
        {
            errorDescription =
                QStringLiteral(
                    "Could not flush output file: %1")
                    .arg(
                        outputContext.file.errorString());

            errorCode =
                utilities::ErrorCode::eDOWLDOPENFILERR;

            break;
        }

        const qint64 finalSize =
            outputContext.file.size();

        setTotalFileSize(finalSize);

        notifyProgress(finalSize);

        if (outputContext.timer.elapsed() > 0)
        {
            const qint64 speed =
                (finalSize * 1000) /
                outputContext.timer.elapsed();

            notifySpeed(speed);
        }

        success = true;

    } while (false);

    //
    // Close output.
    //
    if (outputIo)
    {
        //
        // The buffer belongs to AVIO and is freed
        // by avio_context_free().
        //
        avio_context_free(&outputIo);

        if (output)
            output->pb = nullptr;
    }

    if (outputContext.file.isOpen())
        outputContext.file.close();

    if (videoInput)
        avformat_close_input(&videoInput);

    if (audioInput)
        avformat_close_input(&audioInput);

    if (output)
        avformat_free_context(output);

    //
    // Remove incomplete output.
    //
    if (!success)
        QFile::remove(outputFilename);

    return success;
}

// Observer notifications

void FFmpegMergeDownloader::notifyFinished()
{
    QMutexLocker locker(&m_mutex);

    if (!m_observer)
        return;

    auto* observer = m_observer;

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
    QMutexLocker locker(&m_mutex);

    if (!m_observer)
        return;

    auto* observer = m_observer;

    QMetaObject::invokeMethod(
        this,
        [observer, code, description]()
        {
            observer->onError(
                code,
                description);
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyProgress(
    qint64 bytes)
{
    QMutexLocker locker(&m_mutex);

    if (!m_observer)
        return;

    auto* observer = m_observer;

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
    QMutexLocker locker(&m_mutex);

    if (!m_observer)
        return;

    auto* observer = m_observer;

    QMetaObject::invokeMethod(
        this,
        [observer, bytesPerSecond]()
        {
            observer->onSpeed(
                bytesPerSecond);
        },
        Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyFileCreated(
    const QString& filename)
{
    QMutexLocker locker(&m_mutex);

    if (!m_observer)
        return;

    auto* observer = m_observer;

    QMetaObject::invokeMethod(
        this,
        [observer, filename]()
        {
            observer->onFileCreated(filename);
        },
        Qt::QueuedConnection);
}

// Pause / Resume / Stop

void FFmpegMergeDownloader::Pause()
{
    m_pauseRequested = true;
}

void FFmpegMergeDownloader::Resume(
    const QList<QUrl>& urls,
    QNetworkAccessManager* network_manager,
    const QString& filename,
    const QStringList& httpHeaders)
{
    Q_UNUSED(urls);
    Q_UNUSED(network_manager);
    Q_UNUSED(filename);
    Q_UNUSED(httpHeaders);

    // Resume is intentionally not implemented yet.
}

void FFmpegMergeDownloader::Stop()
{
    m_stopRequested = true;
}

