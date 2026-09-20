#include "FFmpegMergeDownloader.h"

#include <QDebug>
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

// #pragma optimize( "", off )

namespace
{

static QString ffmpegErrorString(int error)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(error, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

static qint64 packetTimestampUs(const AVPacket* packet, const AVStream* stream)
{
    if (!packet || !stream)
        return std::numeric_limits<qint64>::max();

    int64_t ts = packet->dts;

    if (ts == AV_NOPTS_VALUE)
        ts = packet->pts;

    if (ts == AV_NOPTS_VALUE)
        return std::numeric_limits<qint64>::max();

    return static_cast<qint64>(av_rescale_q(ts, stream->time_base, AVRational{1, 1000000}));
}

static bool isAudioStream(const AVStream* stream)
{
    return stream && stream->codecpar && stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO;
}

static bool isVideoStream(const AVStream* stream)
{
    return stream && stream->codecpar && stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
}

int InterruptionRequested(void* ptr) { return ptr && static_cast<std::atomic<bool>*>(ptr)->load(); }

}  // namespace

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

    static int writePacket(void* opaque, FFMPEG_AVIO_WRITE_BUFFER buffer, int size)
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
                qWarning() << "writePacket: MISMATCH at offset" << ctx->virtualPosition << "size" << size;
            }
            // else
            //{
            //     qDebug() << "writePacket: match at offset"
            //         << ctx->virtualPosition
            //         << "size" << size;
            // }

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
            ctx->owner->notifyStart(QByteArray(reinterpret_cast<const char*>(buffer), notifySize));
        }

        ctx->reportProgress();
        return size;
    }

    static int64_t seek(void* opaque, int64_t offset, int whence)
    {
        auto* ctx = static_cast<OutputContext*>(opaque);

        if (!ctx)
            return AVERROR(EINVAL);

        if (whence == AVSEEK_SIZE)
            return ctx->suppressWrites ? ctx->virtualSize : ctx->file.size();

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

        const qint64 now = timer.isValid() ? timer.elapsed() : 0;

        // Avoid flooding the Qt event queue.
        if (bytesWritten == lastReportedBytes && now - lastReportMs < 100)
        {
            return;
        }

        if (now - lastReportMs < 100 && bytesWritten - lastReportedBytes < 64 * 1024)
        {
            return;
        }

        lastReportedBytes = bytesWritten;
        lastReportMs = now;

        owner->notifyProgress(bytesWritten);

        if (now > 0)
        {
            const qint64 speed =
                static_cast<qint64>((static_cast<double>(bytesWritten) * 1000.0) / static_cast<double>(now));

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

using InputFormatPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

struct PacketDeleter
{
    void operator()(AVPacket* packet) const
    {
        if (packet)
            av_packet_free(&packet);
    }
};

using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

}  // namespace

// ============================================================================
// Construction / destruction
// ============================================================================

FFmpegMergeDownloader::FFmpegMergeDownloader(QObject* parent) : QObject(parent) {}

FFmpegMergeDownloader::~FFmpegMergeDownloader()
{
    Stop();

    if (m_worker.joinable())
        m_worker.join();
}

// ============================================================================
// IDownloader
// ============================================================================

const QString& FFmpegMergeDownloader::destinationPath() const { return m_destinationPath; }

bool FFmpegMergeDownloader::setDestinationPath(const QString& destination_path)
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

qint64 FFmpegMergeDownloader::totalFileSize() const { return m_totalFileSize.load(); }

void FFmpegMergeDownloader::setTotalFileSize(qint64 value) { m_totalFileSize.store(value); }

void FFmpegMergeDownloader::setExpectedFileSize(qint64 expected_size)
{
    m_expectedFileSize.store(expected_size);
    if (expected_size > 0)
        m_totalFileSize.store(expected_size);
}

int FFmpegMergeDownloader::speedLimit() const { return m_speedLimit.load(); }

void FFmpegMergeDownloader::setSpeedLimit(int value) { m_speedLimit.store(value); }

void FFmpegMergeDownloader::setDownloadNamePolicy(DuplicateDownloadNamePolicy policy) { m_downloadNamePolicy = policy; }

void FFmpegMergeDownloader::setObserver(DownloaderObserverInterface* observer) { m_observer.store(observer); }

// ============================================================================
// Filename
// ============================================================================

QString FFmpegMergeDownloader::makeOutputFilename(const QList<QUrl>& urls, const QString& filename, bool resume) const
{
    QString result = filename;

    if (result.isEmpty())
    {
        if (!urls.isEmpty())
        {
            QString base = QFileInfo(urls.first().path()).completeBaseName();

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
        result = QDir(m_destinationPath).filePath(result);
    }

    if (resume || (m_downloadNamePolicy == kReplaceFile))
        return result;

    QFileInfo original(result);

    if (!original.exists())
        return result;

    const QString directory = original.absolutePath();

    const QString base = original.completeBaseName();

    const QString suffix = original.suffix().isEmpty() ? QString() : QStringLiteral(".") + original.suffix();

    for (qint64 n = 1; n <= std::numeric_limits<int>::max(); ++n)
    {
        const QString candidate = QDir(directory).filePath(QStringLiteral("%1(%2)%3").arg(base).arg(n).arg(suffix));

        if (!QFileInfo::exists(candidate))
            return candidate;
    }

    return QString();
}

// ============================================================================
// Observer notifications
// ============================================================================

void FFmpegMergeDownloader::notifyStart(const QByteArray& data)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(this, [observer, data]() { observer->onStart(data); }, Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyProgress(qint64 bytes)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(this, [observer, bytes]() { observer->onProgress(bytes); }, Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifySpeed(qint64 bytesPerSecond)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this, [observer, bytesPerSecond]() { observer->onSpeed(bytesPerSecond); }, Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyFileCreated(const QString& filename)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this, [observer, filename]() { observer->onFileCreated(filename); }, Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyFileToBeReleased(const QString& filename)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this, [observer, filename]() { observer->onFileToBeReleased(filename); }, Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyFinished()
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(this, [observer]() { observer->onFinished(); }, Qt::QueuedConnection);
}

void FFmpegMergeDownloader::notifyError(utilities::ErrorCode::ERROR_CODES code, const QString& description)
{
    DownloaderObserverInterface* const observer = m_observer.load();

    if (!observer)
        return;

    QMetaObject::invokeMethod(
        this, [observer, code, description]() { observer->onError(code, description); }, Qt::QueuedConnection);
}

// ============================================================================
// Start / Resume / Pause / Stop
// ============================================================================

void FFmpegMergeDownloader::Start(const QList<QUrl>& urls, QNetworkAccessManager* network_manager,
                                  const QString& filename, const QStringList& httpHeaders)
{
    run(urls, network_manager, filename, httpHeaders, false);
}

void FFmpegMergeDownloader::Resume(const QList<QUrl>& urls, QNetworkAccessManager* network_manager,
                                   const QString& filename, const QStringList& httpHeaders)
{
    // Resume support is intentionally not implemented yet.
    run(urls, network_manager, filename, httpHeaders, true);
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

// ============================================================================
// Internal merge worker
// ============================================================================

class FFmpegMergeDownloader::MergeWorker
{
public:
    MergeWorker(FFmpegMergeDownloader& owner, QList<QUrl> urls, QString outputFilename, bool resume,
                QStringList httpHeaders)
        : m_owner(owner),
          m_urls(std::move(urls)),
          m_outputFilename(std::move(outputFilename)),
          m_resume(resume),
          m_httpHeaders(std::move(httpHeaders))
    {
        m_outputContext.owner = &m_owner;
    }

    void run();

private:
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

    void finishWorker();
    int openNetworkInput(const QUrl& url, InputFormatPtr& result);
    qint64 inputContentLength(const InputFormatPtr& input) const;
    int openResumeInput(InputFormatPtr& result);
    void freeAudioQueue();
    void cleanup();
    AudioBinding* selectedAudioStream(int streamIndex);
    bool readNextVideoPacket();
    bool fillAudioPending();
    bool waitForRetry(int attempt, const char* kind, int error);
    void promoteQueuedAudioPacket(AudioBinding& binding);
    bool transfer();

    FFmpegMergeDownloader& m_owner;
    QList<QUrl> m_urls;
    QString m_outputFilename;
    bool m_resume = false;
    QStringList m_httpHeaders;

    InputFormatPtr m_videoInput;
    InputFormatPtr m_audioInput;
    InputFormatPtr m_resumeVideoInput;
    InputFormatPtr m_resumeAudioInput;

    AVFormatContext* m_output = nullptr;
    AVIOContext* m_outputIo = nullptr;
    FFmpegMergeDownloader::OutputContext m_outputContext;

    std::vector<AudioBinding> m_audioBindings;
    std::vector<QueuedAudioPacket> m_audioQueue;

    int m_networkVideoStreamIndex = -1;
    AVStream* m_inputVideoStream = nullptr;
    AVStream* m_outputVideoStream = nullptr;

    int m_resumeVideoStreamIndex = -1;
    std::vector<int> m_resumeAudioStreamIndices;

    AVFormatContext* m_activeVideoInput = nullptr;
    AVFormatContext* m_activeAudioInput = nullptr;
    int m_activeVideoStreamIndex = -1;
    AVStream* m_activeVideoStream = nullptr;

    AVPacket* m_pendingVideoPacket = nullptr;

    bool m_videoEof = false;
    bool m_audioEof = false;
    bool m_replayMode = false;
    bool m_headerWritten = false;

    std::vector<qint64> m_lastAudioTimestampUs;
    qint64 m_lastVideoTimestampUs = std::numeric_limits<qint64>::min();

    int m_readError = 0;
    QString m_readErrorText;
};

void FFmpegMergeDownloader::MergeWorker::finishWorker() { m_owner.m_running.store(false); }

int FFmpegMergeDownloader::MergeWorker::openNetworkInput(const QUrl& url, InputFormatPtr& result)
{
    AVFormatContext* raw = avformat_alloc_context();
    if (!raw)
        return AVERROR(ENOMEM);

    raw->interrupt_callback.opaque = &m_owner.m_stopRequested;
    raw->interrupt_callback.callback = InterruptionRequested;

    AVDictionary* opts = nullptr;
    // Let FFmpeg itself recover ordinary HTTP disconnects first.
    // The worker-level retry below handles errors that still escape
    // from av_read_frame().
    av_dict_set(&opts, "reconnect", "1", 0);
    av_dict_set(&opts, "reconnect_streamed", "1", 0);
    av_dict_set(&opts, "reconnect_delay_max", "10", 0);
    av_dict_set(&opts, "respect_retry_after", "1", 0);
    av_dict_set(&opts, "reconnect_on_network_error", "1", 0);
    av_dict_set(&opts, "reconnect_on_http_error", "404,429,500,503", 0);
    av_dict_set(&opts, "rw_timeout", "30000000", 0);

    QByteArray headerBlock;
    if (m_httpHeaders.isEmpty())
    {
        headerBlock =
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
            "AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/126.0.0.0 Safari/537.36\r\n";
    }
    else
    {
        for (int i = 0; i + 1 < m_httpHeaders.size(); i += 2)
        {
            headerBlock += m_httpHeaders[i].toUtf8();
            headerBlock += ": ";
            headerBlock += m_httpHeaders[i + 1].toUtf8();
            headerBlock += "\r\n";
        }
    }
    if (!headerBlock.isEmpty())
        av_dict_set(&opts, "headers", headerBlock.constData(), 0);

    const QByteArray urlBytes = url.toString().toUtf8();
    const int r = avformat_open_input(&raw, urlBytes.constData(), nullptr, &opts);
    av_dict_free(&opts);

    if (r < 0)
    {
        if (raw)
            avformat_close_input(&raw);
        return r;
    }

    result.reset(raw);
    return avformat_find_stream_info(result.get(), nullptr);
}

qint64 FFmpegMergeDownloader::MergeWorker::inputContentLength(const InputFormatPtr& input) const
{
    if (!input || !input->pb)
        return -1;

    const int64_t size = avio_size(input->pb);
    return size > 0 ? static_cast<qint64>(size) : -1;
}

int FFmpegMergeDownloader::MergeWorker::openResumeInput(InputFormatPtr& result)
{
    AVFormatContext* raw = avformat_alloc_context();
    if (!raw)
        return AVERROR(ENOMEM);

    raw->interrupt_callback.opaque = &m_owner.m_stopRequested;
    raw->interrupt_callback.callback = InterruptionRequested;

    const QByteArray filenameBytes = QFileInfo(m_outputFilename).absoluteFilePath().toUtf8();

    int r = avformat_open_input(&raw, filenameBytes.constData(), nullptr, nullptr);

    if (r < 0)
    {
        if (raw)
            avformat_close_input(&raw);
        return r;
    }

    result.reset(raw);
    return avformat_find_stream_info(result.get(), nullptr);
}

void FFmpegMergeDownloader::MergeWorker::freeAudioQueue()
{
    for (auto& item : m_audioQueue) av_packet_free(&item.packet);
    m_audioQueue.clear();
}

void FFmpegMergeDownloader::MergeWorker::cleanup()
{
    av_packet_free(&m_pendingVideoPacket);
    freeAudioQueue();
    for (auto& binding : m_audioBindings) av_packet_free(&binding.pendingPacket);
    if (m_outputIo)
        avio_context_free(&m_outputIo);
    if (m_output)
    {
        avformat_free_context(m_output);
        m_output = nullptr;
    }
    m_outputContext.file.close();
}

FFmpegMergeDownloader::MergeWorker::AudioBinding* FFmpegMergeDownloader::MergeWorker::selectedAudioStream(
    int streamIndex)
{
    for (auto& binding : m_audioBindings)
    {
        if (binding.activeInputIndex == streamIndex)
            return &binding;
    }
    return nullptr;
}

bool FFmpegMergeDownloader::MergeWorker::waitForRetry(int attempt, const char* kind, int error)
{
    constexpr int kMaxRetries = 8;
    constexpr int kRetryDelaysMs[] = {1000, 2000, 4000, 5000, 5000, 5000, 5000, 5000};

    if (attempt >= kMaxRetries || m_owner.m_stopRequested.load())
        return false;

    const int delayMs = kRetryDelaysMs[attempt];

    qDebug().noquote() << "FFmpegMergeDownloader:" << kind << "read failed (attempt" << (attempt + 1) << "of"
                       << kMaxRetries << ")"
                       << ":" << ffmpegErrorString(error) << "; retrying in" << delayMs << "ms";

    int remaining = delayMs;
    while (remaining > 0 && !m_owner.m_stopRequested.load())
    {
        const int slice = std::min(remaining, 100);
        QThread::msleep(static_cast<unsigned long>(slice));
        remaining -= slice;
    }

    return !m_owner.m_stopRequested.load();
}

bool FFmpegMergeDownloader::MergeWorker::readNextVideoPacket()
{
    av_packet_unref(m_pendingVideoPacket);

    while (!m_videoEof)
    {
        if (m_owner.m_stopRequested.load())
            return false;

        const int ret = av_read_frame(m_activeVideoInput, m_pendingVideoPacket);

        if (ret == 0)
        {
            if (m_pendingVideoPacket->stream_index == m_activeVideoStreamIndex)
            {
                if (m_replayMode)
                    return true;

                const qint64 videoTimestamp = packetTimestampUs(m_pendingVideoPacket, m_activeVideoStream);

                if (videoTimestamp == std::numeric_limits<qint64>::max() || videoTimestamp > m_lastVideoTimestampUs)
                    return true;
            }

            av_packet_unref(m_pendingVideoPacket);
            continue;
        }

        if (ret == AVERROR_EOF)
        {
            m_videoEof = true;
            av_packet_unref(m_pendingVideoPacket);
            return false;
        }

        // A negative return other than AVERROR_EOF is an actual I/O/protocol
        // error, not end-of-file. FFmpeg's reconnect options normally recover
        // this internally; if the error escapes, give the same input a few
        // more chances before declaring the download failed.
        int error = ret;
        bool recovered = false;

        for (int attempt = 0; attempt < 8; ++attempt)
        {
            if (!waitForRetry(attempt, "video", error))
                return false;

            av_packet_unref(m_pendingVideoPacket);
            const int retryRet = av_read_frame(m_activeVideoInput, m_pendingVideoPacket);

            if (retryRet == 0)
            {
                recovered = true;
                break;
            }

            if (retryRet == AVERROR_EOF)
            {
                m_videoEof = true;
                av_packet_unref(m_pendingVideoPacket);
                return false;
            }

            error = retryRet;
        }

        if (!recovered)
        {
            m_readError = error;
            m_readErrorText = QStringLiteral("Video input read failed: %1").arg(ffmpegErrorString(error));
            av_packet_unref(m_pendingVideoPacket);
            return false;
        }

        if (m_pendingVideoPacket->stream_index == m_activeVideoStreamIndex)
        {
            if (m_replayMode)
                return true;

            const qint64 videoTimestamp = packetTimestampUs(m_pendingVideoPacket, m_activeVideoStream);

            if (videoTimestamp == std::numeric_limits<qint64>::max() || videoTimestamp > m_lastVideoTimestampUs)
                return true;
        }

        av_packet_unref(m_pendingVideoPacket);
    }

    return false;
}

bool FFmpegMergeDownloader::MergeWorker::fillAudioPending()
{
    if (m_audioEof)
        return true;

    for (;;)
    {
        bool allHavePacket = true;
        for (const auto& binding : m_audioBindings)
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
            return true;

        AVPacket* packet = av_packet_alloc();
        if (!packet)
        {
            m_readError = AVERROR(ENOMEM);
            m_readErrorText = QStringLiteral("Could not allocate audio packet.");
            return false;
        }

        int readRet = av_read_frame(m_activeAudioInput, packet);

        if (readRet < 0 && readRet != AVERROR_EOF)
        {
            int error = readRet;
            bool recovered = false;

            for (int attempt = 0; attempt < 8; ++attempt)
            {
                if (!waitForRetry(attempt, "audio", error))
                {
                    av_packet_free(&packet);
                    return false;
                }

                av_packet_unref(packet);
                readRet = av_read_frame(m_activeAudioInput, packet);

                if (readRet == 0)
                {
                    recovered = true;
                    break;
                }

                if (readRet == AVERROR_EOF)
                {
                    av_packet_free(&packet);
                    m_audioEof = true;
                    for (auto& binding : m_audioBindings)
                    {
                        if (!binding.pendingPacket || binding.pendingPacket->size <= 0)
                            binding.eof = true;
                    }
                    return true;
                }

                error = readRet;
            }

            if (!recovered)
            {
                m_readError = error;
                m_readErrorText = QStringLiteral("Audio input read failed: %1").arg(ffmpegErrorString(error));
                av_packet_free(&packet);
                return false;
            }
        }

        if (readRet == AVERROR_EOF)
        {
            av_packet_free(&packet);
            m_audioEof = true;
            for (auto& binding : m_audioBindings)
            {
                if (!binding.pendingPacket || binding.pendingPacket->size <= 0)
                    binding.eof = true;
            }
            return true;
        }

        AudioBinding* binding = selectedAudioStream(packet->stream_index);
        if (!binding)
        {
            av_packet_free(&packet);
            continue;
        }

        if (!m_replayMode)
        {
            const qint64 timestamp = packetTimestampUs(packet, binding->activeStream);
            if (timestamp != std::numeric_limits<qint64>::max())
            {
                const size_t audioIndex = static_cast<size_t>(binding - m_audioBindings.data());
                if (timestamp <= m_lastAudioTimestampUs[audioIndex])
                {
                    av_packet_free(&packet);
                    continue;
                }
            }
        }

        if (binding->pendingPacket && binding->pendingPacket->size > 0)
        {
            m_audioQueue.push_back({packet, packet->stream_index});
            continue;
        }

        av_packet_ref(binding->pendingPacket, packet);
        av_packet_free(&packet);
    }
}

void FFmpegMergeDownloader::MergeWorker::promoteQueuedAudioPacket(AudioBinding& binding)
{
    if (binding.pendingPacket && binding.pendingPacket->size > 0)
        return;

    for (auto it = m_audioQueue.begin(); it != m_audioQueue.end(); ++it)
    {
        if (it->streamIndex != binding.activeInputIndex)
            continue;

        av_packet_ref(binding.pendingPacket, it->packet);
        av_packet_free(&it->packet);
        m_audioQueue.erase(it);
        return;
    }

    if (m_audioEof)
        binding.eof = true;
}

bool FFmpegMergeDownloader::MergeWorker::transfer()
{
    int ret = 0;
    readNextVideoPacket();
    if (m_readError != 0)
        return false;

    if (!fillAudioPending())
        return false;
    for (auto& binding : m_audioBindings) promoteQueuedAudioPacket(binding);

    while (!m_owner.m_stopRequested.load())
    {
        if (!m_videoEof && (!m_pendingVideoPacket || m_pendingVideoPacket->size <= 0))
            readNextVideoPacket();

        if (m_readError != 0)
            return false;

        if (!fillAudioPending())
            return false;
        for (auto& binding : m_audioBindings) promoteQueuedAudioPacket(binding);

        AudioBinding* selectedAudio = nullptr;
        qint64 selectedAudioTimestamp = std::numeric_limits<qint64>::max();

        for (auto& binding : m_audioBindings)
        {
            if (binding.eof || !binding.pendingPacket || binding.pendingPacket->size <= 0)
                continue;

            const qint64 ts = packetTimestampUs(binding.pendingPacket, binding.activeStream);

            if (ts < selectedAudioTimestamp)
            {
                selectedAudioTimestamp = ts;
                selectedAudio = &binding;
            }
        }

        const bool haveVideoPacket = !m_videoEof && m_pendingVideoPacket && m_pendingVideoPacket->size > 0;

        const qint64 videoTimestamp = haveVideoPacket ? packetTimestampUs(m_pendingVideoPacket, m_activeVideoStream)
                                                      : std::numeric_limits<qint64>::max();

        if (!haveVideoPacket && !selectedAudio)
            break;

        const bool writeVideo = haveVideoPacket && (!selectedAudio || videoTimestamp <= selectedAudioTimestamp);

        if (writeVideo)
        {
            AVPacket* packet = m_pendingVideoPacket;
            const qint64 ts = videoTimestamp;

            if (m_replayMode || ts > m_lastVideoTimestampUs)
            {
                if (ts != std::numeric_limits<qint64>::max())
                    m_lastVideoTimestampUs = ts;

                packet->stream_index = m_outputVideoStream->index;
                av_packet_rescale_ts(packet, m_activeVideoStream->time_base, m_outputVideoStream->time_base);

                ret = av_interleaved_write_frame(m_output, packet);
                if (ret < 0)
                {
                    cleanup();
                    finishWorker();
                    m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                                        QStringLiteral("Could not write video packet: %1").arg(ffmpegErrorString(ret)));
                    return false;
                }

                av_packet_unref(packet);
            }

            if (!readNextVideoPacket())
            {
                if (m_readError != 0)
                    return false;
                m_videoEof = true;
            }
        }
        else
        {
            AudioBinding& binding = *selectedAudio;
            AVPacket* packet = binding.pendingPacket;
            const size_t audioIndex = static_cast<size_t>(&binding - m_audioBindings.data());
            const qint64 ts = selectedAudioTimestamp;

            if (m_replayMode || ts > m_lastAudioTimestampUs[audioIndex])
            {
                if (ts != std::numeric_limits<qint64>::max())
                    m_lastAudioTimestampUs[audioIndex] = ts;

                packet->stream_index = binding.outputStream->index;
                av_packet_rescale_ts(packet, binding.activeStream->time_base, binding.outputStream->time_base);

                ret = av_interleaved_write_frame(m_output, packet);
                if (ret < 0)
                {
                    cleanup();
                    finishWorker();
                    m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                                        QStringLiteral("Could not write audio packet: %1").arg(ffmpegErrorString(ret)));
                    return false;
                }

                av_packet_unref(packet);
                promoteQueuedAudioPacket(binding);
            }
        }
    }

    return true;
}

void FFmpegMergeDownloader::MergeWorker::run()
{
    int ret = openNetworkInput(m_urls[0], m_videoInput);
    if (ret < 0)
    {
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDNETWORKERR,
                            QStringLiteral("Could not open video input: %1").arg(ffmpegErrorString(ret)));
        return;
    }

    ret = openNetworkInput(m_urls[1], m_audioInput);
    if (ret < 0)
    {
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDNETWORKERR,
                            QStringLiteral("Could not open audio input: %1").arg(ffmpegErrorString(ret)));
        return;
    }

    // The progress numerator is the actual number of bytes written to the
    // output file. Keep the denominator independent from it: for a merged
    // download it is the sum of the sizes of the video and audio inputs.
    // avio_size() uses the protocol's AVSEEK_SIZE support and does not consume
    // the input stream. If a server does not provide a size, leave an already
    // supplied expected size untouched.

    const qint64 videoSize = inputContentLength(m_videoInput);
    const qint64 audioSize = inputContentLength(m_audioInput);

    if (videoSize > 0 && audioSize > 0)
    {
        m_owner.m_totalFileSize.store(videoSize + audioSize);
    }

    // ------------------------------------------------------------------------
    // Output file. Resume deliberately opens the existing file read/write;
    // nothing is written to it until the old content has been replayed.
    // ------------------------------------------------------------------------
    m_outputContext.owner = &m_owner;

    const auto openMode =
        m_resume ? QIODevice::ReadWrite
                 : ((m_owner.m_downloadNamePolicy == kReplaceFile) ? QIODevice::ReadWrite | QIODevice::Truncate
                                                                   : QIODevice::ReadWrite | QIODevice::NewOnly);

    m_outputContext.file.setFileName(m_outputFilename);

    if (!m_outputContext.file.open(openMode))
    {
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDOPENFILERR,
                            QStringLiteral("Could not create output file '%1': %2")
                                .arg(m_outputFilename, m_outputContext.file.errorString()));
        return;
    }

    const qint64 existingFileSize = m_resume ? m_outputContext.file.size() : 0;

    if (m_resume && existingFileSize <= 0)
    {
        m_outputContext.file.close();
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Cannot resume an empty output file."));
        return;
    }

    if (!m_resume)
        m_owner.notifyFileCreated(m_outputFilename);

    m_outputContext.timer.start();
    m_outputContext.bytesWritten = existingFileSize;
    m_outputContext.virtualPosition = 0;
    m_outputContext.virtualSize = 0;

    // ------------------------------------------------------------------------
    // Output format.
    // ------------------------------------------------------------------------

    ret = avformat_alloc_output_context2(&m_output, nullptr, "matroska", nullptr);

    if (ret < 0 || !m_output)
    {
        m_outputContext.file.close();
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Could not create Matroska output context: %1").arg(ffmpegErrorString(ret)));
        return;
    }

    const int ioBufferSize = 32 * 1024;
    unsigned char* ioBuffer = static_cast<unsigned char*>(av_malloc(ioBufferSize));

    if (!ioBuffer)
    {
        avformat_free_context(m_output);
        m_outputContext.file.close();
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Could not allocate output IO buffer."));
        return;
    }

    m_outputIo = avio_alloc_context(ioBuffer, ioBufferSize, 1, &m_outputContext, nullptr, &OutputContext::writePacket,
                                    &OutputContext::seek);

    if (!m_outputIo)
    {
        av_free(ioBuffer);
        avformat_free_context(m_output);
        m_outputContext.file.close();
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Could not create output AVIO context."));
        return;
    }

    m_output->pb = m_outputIo;
    m_output->flags |= AVFMT_FLAG_CUSTOM_IO;

    // ------------------------------------------------------------------------
    // Transfer state.
    // ------------------------------------------------------------------------

    m_networkVideoStreamIndex = av_find_best_stream(m_videoInput.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (m_networkVideoStreamIndex < 0)
    {
        avio_context_free(&m_outputIo);
        avformat_free_context(m_output);
        m_outputContext.file.close();
        finishWorker();
        m_owner.notifyError(
            utilities::ErrorCode::eDOWLDUNKWNFILERR,
            QStringLiteral("Could not find a video stream: %1").arg(ffmpegErrorString(m_networkVideoStreamIndex)));
        return;
    }

    m_inputVideoStream = m_videoInput->streams[m_networkVideoStreamIndex];

    for (unsigned int i = 0; i < m_audioInput->nb_streams; ++i)
    {
        AVStream* stream = m_audioInput->streams[i];
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
            for (auto& a : m_audioBindings) av_packet_free(&a.pendingPacket);
            avio_context_free(&m_outputIo);
            avformat_free_context(m_output);
            m_outputContext.file.close();
            finishWorker();
            m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                                QStringLiteral("Could not allocate audio packet."));
            return;
        }

        m_audioBindings.push_back(binding);
    }

    if (m_audioBindings.empty())
    {
        avio_context_free(&m_outputIo);
        avformat_free_context(m_output);
        m_outputContext.file.close();
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("The audio input contains no audio streams."));
        return;
    }

    m_outputVideoStream = avformat_new_stream(m_output, nullptr);
    if (!m_outputVideoStream)
    {
        for (auto& a : m_audioBindings) av_packet_free(&a.pendingPacket);
        avio_context_free(&m_outputIo);
        avformat_free_context(m_output);
        m_outputContext.file.close();
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Could not create m_output video stream."));
        return;
    }

    ret = avcodec_parameters_copy(m_outputVideoStream->codecpar, m_inputVideoStream->codecpar);

    if (ret < 0)
    {
        for (auto& a : m_audioBindings) av_packet_free(&a.pendingPacket);
        avio_context_free(&m_outputIo);
        avformat_free_context(m_output);
        m_outputContext.file.close();
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Could not copy video codec parameters: %1").arg(ffmpegErrorString(ret)));
        return;
    }

    m_outputVideoStream->codecpar->codec_tag = 0;
    m_outputVideoStream->time_base = m_inputVideoStream->time_base;
    m_outputVideoStream->sample_aspect_ratio = m_inputVideoStream->sample_aspect_ratio;
    m_outputVideoStream->disposition = m_inputVideoStream->disposition;
    av_dict_copy(&m_outputVideoStream->metadata, m_inputVideoStream->metadata, 0);

    for (auto& binding : m_audioBindings)
    {
        AVStream* outStream = avformat_new_stream(m_output, nullptr);
        if (!outStream)
        {
            for (auto& a : m_audioBindings) av_packet_free(&a.pendingPacket);
            avio_context_free(&m_outputIo);
            avformat_free_context(m_output);
            m_outputContext.file.close();
            finishWorker();
            m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                                QStringLiteral("Could not create m_output audio stream."));
            return;
        }

        ret = avcodec_parameters_copy(outStream->codecpar, binding.inputStream->codecpar);

        if (ret < 0)
        {
            for (auto& a : m_audioBindings) av_packet_free(&a.pendingPacket);
            avio_context_free(&m_outputIo);
            avformat_free_context(m_output);
            m_outputContext.file.close();
            finishWorker();
            m_owner.notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Could not copy audio codec parameters: %1").arg(ffmpegErrorString(ret)));
            return;
        }

        outStream->codecpar->codec_tag = 0;
        outStream->time_base = binding.inputStream->time_base;
        outStream->sample_aspect_ratio = binding.inputStream->sample_aspect_ratio;
        outStream->disposition = binding.inputStream->disposition;
        av_dict_copy(&outStream->metadata, binding.inputStream->metadata, 0);
        binding.outputStream = outStream;
    }

    av_dict_copy(&m_output->metadata, m_videoInput->metadata, 0);
    m_output->avoid_negative_ts = AVFMT_AVOID_NEG_TS_DISABLED;

    // ------------------------------------------------------------------------
    // Resume inputs: two independent readers of the same old MKV. Keeping the
    // video and audio readers separate allows the same transfer logic to be reused.
    // ------------------------------------------------------------------------

    if (m_resume)
    {
        ret = openResumeInput(m_resumeVideoInput);
        if (ret < 0)
        {
            for (auto& a : m_audioBindings) av_packet_free(&a.pendingPacket);
            avio_context_free(&m_outputIo);
            avformat_free_context(m_output);
            m_outputContext.file.close();
            finishWorker();
            m_owner.notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Could not open existing output for resume: %1").arg(ffmpegErrorString(ret)));
            return;
        }

        ret = openResumeInput(m_resumeAudioInput);
        if (ret < 0)
        {
            for (auto& a : m_audioBindings) av_packet_free(&a.pendingPacket);
            avio_context_free(&m_outputIo);
            avformat_free_context(m_output);
            m_outputContext.file.close();
            finishWorker();
            m_owner.notifyError(
                utilities::ErrorCode::eDOWLDUNKWNFILERR,
                QStringLiteral("Could not open existing audio data for resume: %1").arg(ffmpegErrorString(ret)));
            return;
        }

        m_resumeVideoStreamIndex =
            av_find_best_stream(m_resumeVideoInput.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

        if (m_resumeVideoStreamIndex < 0)
        {
            for (auto& a : m_audioBindings) av_packet_free(&a.pendingPacket);
            avio_context_free(&m_outputIo);
            avformat_free_context(m_output);
            m_outputContext.file.close();
            finishWorker();
            m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                                QStringLiteral("Could not find video stream in existing output."));
            return;
        }

        for (unsigned int i = 0; i < m_resumeAudioInput->nb_streams; ++i)
        {
            if (isAudioStream(m_resumeAudioInput->streams[i]))
                m_resumeAudioStreamIndices.push_back(static_cast<int>(i));
        }

        if (m_resumeAudioStreamIndices.size() < m_audioBindings.size())
        {
            for (auto& a : m_audioBindings) av_packet_free(&a.pendingPacket);
            avio_context_free(&m_outputIo);
            avformat_free_context(m_output);
            m_outputContext.file.close();
            finishWorker();
            m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                                QStringLiteral("Existing output does not contain all required audio streams."));
            return;
        }
    }

    m_activeVideoInput = m_videoInput.get();
    m_activeAudioInput = m_audioInput.get();
    m_activeVideoStreamIndex = m_networkVideoStreamIndex;
    m_activeVideoStream = m_inputVideoStream;

    m_pendingVideoPacket = av_packet_alloc();
    if (!m_pendingVideoPacket)
    {
        for (auto& a : m_audioBindings) av_packet_free(&a.pendingPacket);
        avio_context_free(&m_outputIo);
        avformat_free_context(m_output);
        m_outputContext.file.close();
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Could not allocate video packet."));
        return;
    }

    m_videoEof = false;
    m_audioEof = false;
    m_replayMode = m_resume;
    m_headerWritten = false;

    m_lastAudioTimestampUs.assign(m_audioBindings.size(), std::numeric_limits<qint64>::min());
    m_lastVideoTimestampUs = std::numeric_limits<qint64>::min();

    // ------------------------------------------------------------------------
    // Same merge loop for both phases. In replay mode it consumes old packets
    // and records their timestamps but does not write a single output byte.
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // Header. Resume suppresses the physical header bytes because the old
    // Matroska header is already in the existing file.
    // ------------------------------------------------------------------------
    m_outputContext.suppressWrites = m_resume;
    m_outputContext.virtualPosition = 0;
    m_outputContext.virtualSize = 0;

    ret = avformat_write_header(m_output, nullptr);
    if (ret < 0)
    {
        cleanup();
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Could not initialize Matroska output: %1").arg(ffmpegErrorString(ret)));
        return;
    }

    m_headerWritten = true;

    // ------------------------------------------------------------------------
    // Replay the old file before allowing any output write.
    // ------------------------------------------------------------------------
    if (m_resume)
    {
        m_activeVideoInput = m_resumeVideoInput.get();
        m_activeAudioInput = m_resumeAudioInput.get();
        m_activeVideoStreamIndex = m_resumeVideoStreamIndex;
        m_activeVideoStream = m_resumeVideoInput->streams[m_resumeVideoStreamIndex];

        for (size_t i = 0; i < m_audioBindings.size(); ++i)
        {
            m_audioBindings[i].activeInputIndex = m_resumeAudioStreamIndices[i];
            m_audioBindings[i].activeStream = m_resumeAudioInput->streams[m_resumeAudioStreamIndices[i]];
            m_audioBindings[i].eof = false;
            av_packet_unref(m_audioBindings[i].pendingPacket);
        }

        m_videoEof = false;
        m_audioEof = false;

        if (!transfer())
        {
            if (m_readError != 0)
            {
                cleanup();
                finishWorker();
                m_owner.notifyError(utilities::ErrorCode::eDOWLDNETWORKERR, m_readErrorText);
            }
            return;
        }

        av_packet_unref(m_pendingVideoPacket);
        freeAudioQueue();
        for (auto& binding : m_audioBindings)
        {
            av_packet_unref(binding.pendingPacket);
            binding.eof = false;
        }

        // --------------------------------------------------------------------
        // Remove the old trailer/incomplete tail only now, after the old data
        // has been completely replayed. Thus resume performs no physical
        // output write while it is replaying the old content.
        // --------------------------------------------------------------------

        // --------------------------------------------------------------------
        // Return to network inputs and seek back slightly. We intentionally
        // re-download this overlap and discard packets already present in the
        // old file. A two-second overlap is small but gives HTTP seeks room to
        // land on a useful video keyframe.
        // --------------------------------------------------------------------
        constexpr qint64 kResumeOverlapUs = 2 * 1000 * 1000;

        m_activeVideoInput = m_videoInput.get();
        m_activeAudioInput = m_audioInput.get();
        m_activeVideoStreamIndex = m_networkVideoStreamIndex;
        m_activeVideoStream = m_inputVideoStream;

        for (auto& binding : m_audioBindings)
        {
            binding.activeInputIndex = binding.inputIndex;
            binding.activeStream = binding.inputStream;
        }

        m_videoEof = false;
        m_audioEof = false;

        if (m_lastVideoTimestampUs != std::numeric_limits<qint64>::min())
        {
            const qint64 target = std::max<qint64>(0, m_lastVideoTimestampUs - kResumeOverlapUs);

            qDebug().noquote() << "Video resume seek:"
                               << "last=" << m_lastVideoTimestampUs / 1000000.0 << "s"
                               << "target=" << target / 1000000.0 << "s"
                               << "overlap=" << kResumeOverlapUs / 1000000.0 << "s";

            const int seekRet = avformat_seek_file(m_videoInput.get(), -1, std::numeric_limits<int64_t>::min(), target,
                                                   std::numeric_limits<int64_t>::max(), AVSEEK_FLAG_BACKWARD);

            qDebug().noquote() << "Video resume seek result:" << seekRet
                               << (seekRet < 0 ? ffmpegErrorString(seekRet) : QStringLiteral("OK"));
        }

        qint64 audioResumeTimestamp = std::numeric_limits<qint64>::max();

        for (const qint64 ts : m_lastAudioTimestampUs)
        {
            if (ts != std::numeric_limits<qint64>::min())
                audioResumeTimestamp = std::min(audioResumeTimestamp, ts);
        }

        if (audioResumeTimestamp != std::numeric_limits<qint64>::max())
        {
            const qint64 target = std::max<qint64>(0, audioResumeTimestamp - kResumeOverlapUs);

            qDebug().noquote() << "Audio resume seek:"
                               << "last=" << audioResumeTimestamp / 1000000.0 << "s"
                               << "target=" << target / 1000000.0 << "s"
                               << "overlap=" << kResumeOverlapUs / 1000000.0 << "s";

            const int seekRet = avformat_seek_file(m_audioInput.get(), -1, std::numeric_limits<int64_t>::min(), target,
                                                   std::numeric_limits<int64_t>::max(), AVSEEK_FLAG_BACKWARD);

            qDebug().noquote() << "Audio resume seek result:" << seekRet
                               << (seekRet < 0 ? ffmpegErrorString(seekRet) : QStringLiteral("OK"));
        }

        // Nothing has been physically written so far. Now move the QFile to
        // its existing end and switch the AVIO into real-write mode.
        // avio_flush(m_output->pb);
        m_outputContext.suppressWrites = false;
        // m_outputContext.virtualPosition = appendPosition;
        // m_outputContext.virtualSize = appendPosition;

        if (!m_outputContext.file.seek(m_outputContext.virtualPosition))  // appendPosition))
        {
            cleanup();
            finishWorker();
            m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                                QStringLiteral("Could not seek output to the end for resume."));
            return;
        }

        m_outputIo->pos = m_outputContext.virtualPosition;  // appendPosition;
        // m_outputIo->buf_ptr = m_outputIo->buffer;
        // m_outputIo->buf_end = m_outputIo->buffer;
        m_outputIo->eof_reached = 0;

        m_replayMode = false;

        if (!transfer())
        {
            if (m_readError != 0)
            {
                cleanup();
                finishWorker();
                m_owner.notifyError(utilities::ErrorCode::eDOWLDNETWORKERR, m_readErrorText);
            }
            return;
        }
    }
    else
    {
        m_outputContext.suppressWrites = false;
        m_replayMode = false;

        if (!transfer())
        {
            if (m_readError != 0)
            {
                cleanup();
                finishWorker();
                m_owner.notifyError(utilities::ErrorCode::eDOWLDNETWORKERR, m_readErrorText);
            }
            return;
        }
    }

    if (m_owner.m_pauseRequested.load())
    {
        // Do not write a Matroska trailer: the partial file is deliberately
        // left in the same resumable form as a paused plain Downloader.
        avio_flush(m_output->pb);
        m_outputContext.file.flush();
        cleanup();
        finishWorker();
        return;
    }

    if (m_owner.m_stopRequested.load())
    {
        const QString filename = m_outputContext.file.fileName();
        cleanup();
        if (QFile::exists(filename))
        {
            m_owner.notifyFileToBeReleased(filename);
            QFile::remove(filename);
        }
        finishWorker();
        return;
    }

    ret = m_headerWritten ? av_write_trailer(m_output) : 0;
    if (ret < 0)
    {
        cleanup();
        finishWorker();
        m_owner.notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                            QStringLiteral("Could not finalize Matroska file: %1").arg(ffmpegErrorString(ret)));
        return;
    }

    avio_flush(m_output->pb);
    m_outputContext.file.flush();

    const qint64 finalSize = m_outputContext.file.size();
    m_owner.m_totalFileSize.store(finalSize);
    m_owner.notifyProgress(finalSize);

    if (m_outputContext.timer.elapsed() > 0)
    {
        const qint64 speed = static_cast<qint64>((static_cast<double>(finalSize) * 1000.0) /
                                                 static_cast<double>(m_outputContext.timer.elapsed()));
        m_owner.notifySpeed(speed);
    }

    cleanup();
    finishWorker();
    m_owner.notifyFinished();
}

void FFmpegMergeDownloader::run(const QList<QUrl>& urls, QNetworkAccessManager* network_manager,
                                const QString& filename, const QStringList& httpHeaders, bool resume)
{
    Q_UNUSED(network_manager);

    if (m_running)
        return;

    if (urls.size() != 2)
    {
        notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR,
                    QStringLiteral("Exactly two URLs are required: video and audio."));

        return;
    }

    if (!urls[0].isValid() || !urls[1].isValid())
    {
        notifyError(utilities::ErrorCode::eDOWLDUNKWNFILERR, QStringLiteral("Invalid media URL."));

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

        notifyError(utilities::ErrorCode::eDOWLDOPENFILERR, QStringLiteral("Could not generate output filename."));

        return;
    }

    m_worker = std::thread(
        [this, urls, outputFilename, resume, httpHeaders]()
        {
            MergeWorker worker(*this, urls, outputFilename, resume, httpHeaders);
            worker.run();
        });
}
