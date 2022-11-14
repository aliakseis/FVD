#include "ffmpegdecoder.h"

#include <portaudio.h>

#include <QBuffer>
#include <climits>
#include <cstdint>
#include <random>
#include <utility>
#include <cmath>

#include "audioparsethread.h"
#include "displaythread.h"
#include "interlockedadd.h"
#include "parsethread.h"
#include "timelimiterthread.h"
#include "utilities/logger.h"
#include "utilities/loggertag.h"
#include "videodisplay.h"
#include "videoparsethread.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}


void preciseSleep(double sec) { QThread::usleep(sec * 1000000UL); }

FFmpegDecoder::FFmpegDecoder()
{
    // no detection using
    m_streamDurationDetection = false;

    // default format
    m_audioSettings.channels = 2;
    m_audioSettings.frequency = 44100;
    m_audioSettings.format = AV_SAMPLE_FMT_S16;
    m_audioSettings.channel_layout = av_get_default_channel_layout(m_audioSettings.channels);

    // Prepare audio device params
    Pa_Initialize();

    m_frameListener = nullptr;

    // Set startup sizes
    m_targetWidth = 0;
    m_targetHeight = 0;

    // time for sleeper
    m_waitSleperTime = 0;

    // Choose default pixel format
    m_pixelFormat = AV_PIX_FMT_YUV420P;
    m_resizeWithDecoder = false;

    // Init control packets
    av_init_packet(&m_seekPacket);
    m_seekPacket.data = (uint8_t*)(intptr_t) "SEEKING";
    m_seekPacket.size = (int)strlen("SEEKING");
    av_init_packet(&m_downloadingPacket);
    m_downloadingPacket.data = (uint8_t*)(intptr_t) "DOWNLOADING";
    m_downloadingPacket.size = (int)strlen("DOWNLOADING");

    VERIFY(connect(this, SIGNAL(fileLoaded()), &m_loadedEventLoop, SLOT(quit())));
    resetVariables();

    qDebug() << "[FFMPEG_DECODER] Initialized";
}

FFmpegDecoder::~FFmpegDecoder()
{
    close(true);
    Pa_Terminate();
    qDebug() << "[FFMPEG_DECODER] Closed";
}

void FFmpegDecoder::resetVariables()
{
    m_videoCodec = nullptr;
    m_formatContext = nullptr;
    m_videoCodecContext = nullptr;
    m_audioCodecContext = nullptr;
    m_videoFrame = nullptr;
    m_audioFrame = nullptr;
    m_audioSwrContext = nullptr;
    m_videoStream = nullptr;
    m_audioStream = nullptr;
    m_duration = 0;

    m_imageCovertContext = nullptr;

    m_audioPTS = 0;
    m_videoPTS = 0;

    m_mainParseThread = nullptr;
    m_mainVideoThread = nullptr;
    m_mainAudioThread = nullptr;
    m_mainDisplayThread = nullptr;
    m_mainTimeLimiterThread = nullptr;

    m_isReadReady = false;

    m_isPaused = false;

    m_seekFlags = 0x0;

    m_seekFrame = 0;

    m_bytesLimiter = -1;
    m_bytesLimiterTotal = -1;
    m_bytesCurrent = 0;
    m_fileProbablyNotFull = false;
    m_durationRecheckIsRun = false;

    m_durationLimiter = 0;

    m_headerSize = 0;

    m_isPlaying = false;

    m_setDefaultImageOnClose = true;

    m_downloading = false;

    TAG("ffmpeg_closing") << "Variables reset";
}

void FFmpegDecoder::close(bool isBlocking)
{
    TAG("ffmpeg_closing") << "Start file closing";

    QThread* current_thread = QThread::currentThread();
    if (current_thread == m_mainVideoThread || current_thread == m_mainAudioThread ||
        current_thread == m_mainParseThread || current_thread == m_mainDisplayThread ||
        current_thread == m_mainTimeLimiterThread)
    {
        QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
        return;
    }

    // Now we can't read from file
    m_isReadReady = false;

    TAG("ffmpeg_closing") << "Aborting threads";
    if (m_mainVideoThread != nullptr)
    {
        m_mainVideoThread->setAbort();
    }
    if (m_mainAudioThread != nullptr)
    {
        m_mainAudioThread->setAbort();
    }
    if (m_mainParseThread != nullptr)
    {
        m_mainParseThread->setAbort();
    }
    if (m_mainDisplayThread != nullptr)
    {
        m_mainDisplayThread->setAbort();
    }
    if (m_mainTimeLimiterThread != nullptr)
    {
        m_mainTimeLimiterThread->setAbort();
    }

    QThread::yieldCurrentThread();

    TAG("ffmpeg_closing") << "Closing threads";
    if (isBlocking)
    {
        if (m_mainVideoThread != nullptr)
        {
            m_mainVideoThread->wait();
        }
        if (m_mainAudioThread != nullptr)
        {
            m_mainAudioThread->wait();
        }
        if (m_mainParseThread != nullptr)
        {
            m_mainParseThread->wait();
        }
        if (m_mainDisplayThread != nullptr)
        {
            if (m_mainDisplayThread->isRunning())
            {
                VERIFY(connect(m_mainDisplayThread, SIGNAL(finished()), &m_closingEventLoop, SLOT(quit())));
                m_closingEventLoop.exec();
            }
        }
        if (m_mainTimeLimiterThread != nullptr)
        {
            m_mainTimeLimiterThread->wait();
        }
    }

    closeProcessing();
    emit playingFinished();
}

void FFmpegDecoder::closeProcessing()
{
    m_audioPacketsQueue.clear();
    m_videoPacketsQueue.clear();

    TAG("ffmpeg_closing") << "Closing old vars";

    delete m_mainVideoThread;
    delete m_mainAudioThread;
    delete m_mainParseThread;
    delete m_mainDisplayThread;
    delete m_mainTimeLimiterThread;

    if (m_stream != nullptr)
    {
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
    }

    // Free videoFrames
    m_videoFramesQueue.clear();

    if (m_imageCovertContext != nullptr)
    {
        av_free(m_imageCovertContext);
    }

    if (m_audioFrame != nullptr)
    {
        av_free(m_audioFrame);
    }

    if (m_audioSwrContext != nullptr)
    {
        swr_free(&m_audioSwrContext);
    }

    // Free the YUV frame
    if (m_videoFrame != nullptr)
    {
        av_free(m_videoFrame);
    }

    // Close the codec
    avcodec_free_context(&m_videoCodecContext);

    // Close the audio codec
    avcodec_free_context(&m_audioCodecContext);

    bool isFileReallyClosed = false;

    // Close video file
    if (m_formatContext != nullptr)
    {
        avformat_close_input(&m_formatContext);
        isFileReallyClosed = true;
    }

    TAG("ffmpeg_closing") << "Old file closed";

    const bool setDefaultImageOnClose = m_setDefaultImageOnClose;
    resetVariables();

    if (isFileReallyClosed)
    {
        TAG("ffmpeg_closing") << "File was opened. Emit file closing signal";
        emit fileReleased(setDefaultImageOnClose);
    }

    emit decoderClosed();
}

void FFmpegDecoder::openFile(QString filename)
{
    m_openedFilePath = std::move(filename);
    VERIFY(connect(this, SIGNAL(decoderClosed()), SLOT(openFileProcessing())));
    close();
}

void FFmpegDecoder::openFileProcessing()
{
    VERIFY(disconnect(this, SIGNAL(decoderClosed()), this, SLOT(openFileProcessing())));

    if (!openFileDecoder(m_openedFilePath))
    {
        return;
    }

    allocateFrames();

    // TODO: make it header size from AVFormatContext
    m_bytesCurrent = 0;

    if (m_audioStreamNumber >= 0)
    {
        PaStreamParameters params{};
        params.device = Pa_GetDefaultOutputDevice();
        params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultHighOutputLatency;
        params.channelCount = m_audioSettings.channels;

        switch (av_get_bytes_per_sample(m_audioSettings.format))
        {
        case 1:
            params.sampleFormat = paInt8;
            break;
        case 2:
            params.sampleFormat = paInt16;
            break;
        case 4:
            params.sampleFormat = paInt32;
            break;
        }

        auto err{Pa_OpenStream(&m_stream, nullptr, &params, m_audioSettings.frequency, paFramesPerBufferUnspecified,
                               paNoFlag, nullptr, nullptr)};

        if (err != paNoError)
        {
            qCritical() << "[FFMPEG] unable to open audio stream";
        }
        else
        {
            err = Pa_StartStream(m_stream);
            if (err != paNoError)
            {
                qCritical() << "[FFMPEG] unable to start audio stream";
            }
            m_audioSettings.frequency = Pa_GetStreamInfo(m_stream)->sampleRate;
        }
    }

    if (m_videoStreamNumber >= 0)
    {
        if (m_targetWidth == 0 || m_targetHeight == 0)
        {
            setTargetSize(m_videoCodecContext->width, m_videoCodecContext->height);
        }
    }

    m_isReadReady = true;
    emit fileLoaded();
}

bool FFmpegDecoder::openFileDecoder(const QString& file)
{
    m_openedFilePath = file;
    // Open video file
    const int error = avformat_open_input(&m_formatContext,
#ifdef Q_OS_WIN
        file.toUtf8().constData(),
#else
        QFile::encodeName(file).constData(),
#endif  // Q_OS_WIN
        nullptr, nullptr);
    if (error != 0)
    {
        qWarning() << "Couldn't open '" + file + "'"
                   << "error:" << error;
        return false;
    }
    TAG("ffmpeg_opening") << "Opening '" + file + "' video/audio file...";

    // Retrieve stream information
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0)
    {
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
        TAG("ffmpeg_opening") << "Couldn't find stream information";
        return false;
    }

    // Dump information about file onto standard error
    // av_dump_format(m_formatContext, 0, m_openedFilePath.toStdString().c_str(), false);

    // Find the first video stream
    m_videoStreamNumber = -1;
    m_audioStreamNumber = -1;
    for (int i = m_formatContext->nb_streams; --i >= 0;)
    {
        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStream = m_formatContext->streams[i];
            m_videoStreamNumber = i;
        }
        else if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_audioStream = m_formatContext->streams[i];
            m_audioStreamNumber = i;
        }
    }
    if (m_videoStreamNumber == -1)
    {
        TAG("ffmpeg_opening") << "Can't find video stream";
    }
    else if (m_videoStreamNumber >= 0)
    {
        if (m_streamDurationDetection)
        {
            m_duration = (m_videoStream->duration > 0) ? m_videoStream->duration : -1;
        }
        else
        {
            m_duration = (m_videoStream->duration > 0) ? m_videoStream->duration
                : (m_formatContext->duration / av_q2d(m_videoStream->time_base)) / 1000000LL;
        }
    }

    if (m_audioStreamNumber == -1)
    {
        TAG("ffmpeg_opening") << "No audio stream";
    }
    else if (m_audioStreamNumber >= 0 && m_videoStreamNumber == -1)
    {
        // Changing video -> audio duration
        const int64_t format_duration = (m_formatContext->duration / av_q2d(m_audioStream->time_base)) / 1000000LL;
        if (m_streamDurationDetection)
        {
            m_duration = (m_audioStream->duration > 0) ? m_audioStream->duration : -1;
        }
        else
        {
            m_duration = (m_audioStream->duration > 0) ? m_audioStream->duration : format_duration;
        }
    }

    // Get a pointer to the codec context for the video stream
    if (m_videoStreamNumber >= 0)
    {
        TAG("ffmpeg_opening") << "Video steam number: " << m_videoStreamNumber;
        m_videoCodecContext = avcodec_alloc_context3(nullptr);
        if (m_videoCodecContext == nullptr)
        {
            return false;
        }
        if (avcodec_parameters_to_context(m_videoCodecContext, m_videoStream->codecpar) < 0)
        {
            return false;
        }
    }
    if (m_audioStreamNumber >= 0)
    {
        TAG("ffmpeg_opening") << "Audio steam number: " << m_audioStreamNumber;
        m_audioCodecContext = avcodec_alloc_context3(nullptr);
        if (m_audioCodecContext == nullptr)
        {
            return false;
        }
        if (avcodec_parameters_to_context(m_audioCodecContext, m_audioStream->codecpar) < 0)
        {
            return false;
        }
    }

    // Multithread decoding
    int perfect_threads = QThread::idealThreadCount();
    if (m_videoStreamNumber >= 0)
    {
        m_videoCodecContext->thread_count = perfect_threads > 0 ? perfect_threads : 1;
    }

    // Find the decoder for the video stream
    if (m_videoStreamNumber >= 0)
    {
        m_videoCodec = avcodec_find_decoder(m_videoCodecContext->codec_id);
        if (m_videoCodec == nullptr)
        {
            avcodec_free_context(&m_videoCodecContext);
            m_videoCodecContext = nullptr;
            avformat_free_context(m_formatContext);
            m_formatContext = nullptr;
            // Q_ASSERT(false && "[FFMPEG] No such codec found");
            return false;  // Codec not found
        }
    }

    // Find audio codec
    if (m_audioStreamNumber >= 0)
    {
        m_audioCodec = avcodec_find_decoder(m_audioCodecContext->codec_id);
        if (m_audioCodec == nullptr)
        {
            avcodec_free_context(&m_audioCodecContext);
            m_audioCodecContext = nullptr;
            avcodec_free_context(&m_videoCodecContext);
            m_videoCodecContext = nullptr;
            if (m_formatContext != nullptr)
            {
                avformat_free_context(m_formatContext);
            }
            m_formatContext = nullptr;
            Q_ASSERT(false && "[FFMPEG] No such codec found");
            return false;  // Codec not found
        }
    }

    // Open codec
    if (m_videoStreamNumber >= 0)
    {
        if (avcodec_open2(m_videoCodecContext, m_videoCodec, nullptr) < 0)
        {
            avcodec_free_context(&m_videoCodecContext);
            m_videoCodecContext = nullptr;
            avformat_free_context(m_formatContext);
            m_formatContext = nullptr;
            Q_ASSERT(false && "[FFMPEG] Error on codec opening");
            return false;  // Could not open codec
        }

        // Some broken files can pass codec check but don't have width x height
        if (m_videoCodecContext->width <= 0 || m_videoCodecContext->height <= 0)
        {
            if (m_audioCodecContext != nullptr)
            {
                m_audioCodecContext = nullptr;
            }
            avcodec_free_context(&m_videoCodecContext);
            m_videoCodecContext = nullptr;
            avformat_close_input(&m_formatContext);
            m_formatContext = nullptr;

            Q_ASSERT(false && "[FFMPEG] This file haven't resolution");
            return false;  // Could not open codec
        }
    }

    // Open audio codec
    if (m_audioStreamNumber >= 0)
    {
        if (avcodec_open2(m_audioCodecContext, m_audioCodec, nullptr) < 0)
        {
            avcodec_free_context(&m_audioCodecContext);
            m_audioCodecContext = nullptr;
            if (m_videoStreamNumber >= 0)
            {
                avcodec_free_context(&m_videoCodecContext);
                m_videoCodecContext = nullptr;
            }
            if (m_formatContext != nullptr)
            {
                avformat_free_context(m_formatContext);
            }
            m_formatContext = nullptr;
            Q_ASSERT(false && "[FFMPEG] Error on codec opening");
            return false;  // Could not open codec
        }
    }

    return true;
}

void FFmpegDecoder::allocateFrames()
{
    if (m_videoStreamNumber >= 0)
    {
        m_videoFrame = av_frame_alloc();
    }
    if (m_audioStreamNumber >= 0)
    {
        m_audioFrame = av_frame_alloc();
    }
}

/**
 * Starts play video.
 *
 * @brief FFmpegDecoder::play
 */
void FFmpegDecoder::play(bool isPaused)
{
    TAG("ffmpeg_opening") << "Starting playing";

    m_isPaused = isPaused;

    if (isPaused)
    {
        m_pauseTimer = av_gettime() / 1000000.;
    }

    // Sync threads
    if (!m_isReadReady)
    {
        TAG("ffmpeg_opening") << "Waiting for the file to be opened";
        m_loadedEventLoop.exec();
    }
    // Starting parse thread
    if (m_isReadReady && (m_mainParseThread == nullptr))
    {
        m_isPlaying = true;
        m_mainParseThread = new ParseThread(this);
        TAG("ffmpeg_opening") << "Playing";
        m_mainParseThread->start();
    }
}

void FFmpegDecoder::setVolume(double volume)
{
    if (volume < 0 || volume > 1.)
    {
        return;
    }

    m_volume = volume;

    emit volumeChanged(volume);
}

void FFmpegDecoder::setVolume(int volume, int steep) { setVolume((double)volume / steep); }

double FFmpegDecoder::volume() const { return m_volume; }

bool FFmpegDecoder::frameToImage(FPicture& videoFrameData)
{
    int width = m_videoFrame->width;
    int height = m_videoFrame->height;

    if (m_resizeWithDecoder)
    {
        QMutexLocker locker(&m_resizeMutex);
        if (m_targetWidth > 0 && m_targetHeight > 0)
        {
            width = m_targetWidth;
            height = m_targetHeight;
        }
    }

    videoFrameData.reallocForSure(m_pixelFormat, width, height);

    // Prepare image conversion
    m_imageCovertContext = sws_getCachedContext(m_imageCovertContext, m_videoFrame->width, m_videoFrame->height,
                                                (AVPixelFormat)m_videoFrame->format, width, height, m_pixelFormat,
                                                m_resizeWithDecoder? SWS_BICUBIC : SWS_POINT,
                                                nullptr, nullptr, nullptr);

    Q_ASSERT(m_imageCovertContext != nullptr);

    if (m_imageCovertContext == nullptr)
    {
        return false;
    }

    // Doing conversion
    VERIFY(sws_scale(m_imageCovertContext, m_videoFrame->data, m_videoFrame->linesize, 0, m_videoFrame->height,
                     videoFrameData.data, videoFrameData.linesize) > 0);

    return true;
}

void FFmpegDecoder::setTargetSize(int w, int h, bool is_ceil)
{
    // Lock resizing
    QMutexLocker locker(&m_resizeMutex);
    Q_ASSERT(w > 0);
    Q_ASSERT(h > 0);
    m_targetWidth = w;
    m_targetHeight = h;
    correctDisplay(is_ceil);
}

void FFmpegDecoder::correctDisplay(bool is_ceil)
{
    // Calculate sizes
    if (m_targetWidth % 16 != 0)
    {
        double aspect_revert = 1. / aspectRatio();
        if (!is_ceil)
        {
            m_targetWidth = m_targetWidth - (m_targetWidth % 16);  // m_targetWidth must be divisible by 16
        }
        else
        {
            m_targetWidth = (m_targetWidth - (m_targetWidth % 16)) << 4;  // m_targetWidth must be divisible by 16
        }
        m_targetHeight = aspect_revert * m_targetWidth;
    }
}

QRect FFmpegDecoder::setPreferredSize(const QSize& size, int scr_xleft, int scr_ytop)
{
    return setPreferredSize(size.width(), size.height(), scr_xleft, scr_ytop);
}

QRect FFmpegDecoder::setPreferredSize(int scr_width, int scr_height, int scr_xleft, int scr_ytop)
{
    QRect targetRect = getPreferredSize(scr_width, scr_height, scr_xleft, scr_ytop);
    /* Correct display */
    if (scr_width > 0 && scr_height > 0)
    {
        QMutexLocker locker(&m_resizeMutex);
        m_targetWidth = targetRect.width();
        m_targetHeight = targetRect.height();
    }
    else
    {
        return {0, 0, 0, 0};
    }
    return targetRect;
}

QRect FFmpegDecoder::getPreferredSize(int scr_width, int scr_height, int scr_xleft /* = 0*/,
                                      int scr_ytop /* = 0*/) const
{
    TAG("ffmpeg_resizeframe") << "Target frame size: " << scr_width << "x" << scr_height;

    const double aspect_ratio = aspectRatio();

    int width = int(scr_height * aspect_ratio) & ~0xf;
    int height = std::lround(width / aspect_ratio);

    if (width > scr_width)
    {
        width = scr_width & ~0xf;
        height = std::lround(width / aspect_ratio);
    }
    int x = ((scr_width - width) >> 1);
    int y = ((scr_height - height) >> 1);

    return {scr_xleft + x, scr_ytop + y, FFMAX(width, 1), FFMAX(height, 1)};
}

double FFmpegDecoder::videoClock() const { return m_videoPTS; }

double FFmpegDecoder::audioClock() const { return m_audioPTS; }

void FFmpegDecoder::finishedDisplayingFrame()
{
    m_videoPTS = m_videoFramesQueue.m_frames[m_videoFramesQueue.m_read_counter].m_pts;
    QMutexLocker locker(&m_videoFramesMutex);
    m_videoFramesQueue.m_busy--;
    Q_ASSERT(m_videoFramesQueue.m_busy >= 0);
    // avoiding assert in VideoParseThread::run()
    m_videoFramesQueue.m_read_counter =
        (m_videoFramesQueue.m_read_counter + 1) % std::size(m_videoFramesQueue.m_frames);
    m_videoFramesCV.wakeAll();
}

void FFmpegDecoder::startLimiterThread()
{
    QMutexLocker locker(&m_mainTimeLimiterStarter);
    if (m_mainTimeLimiterThread == nullptr)
    {
        m_mainTimeLimiterThread = new TimeLimiterThread(this);

        // Duration fix was starting and now file duration probably not corrent because of partitional file
        if (m_durationRecheckIsRun)
        {
            m_fileProbablyNotFull = true;
            emit fileProbablyNotFull();
        }

        m_mainTimeLimiterThread->start();
    }
}


bool FFmpegDecoder::seekDuration(int64_t duration)
{
    if (m_bytesLimiter >= 0 &&
        ((m_durationLimiter >= 0 && duration > m_durationLimiter) ||
         ((double)duration / m_duration >= (double)(m_bytesLimiter - PLAYBACK_AVPACKET_MAX) / m_bytesLimiterTotal)))
    {
        Q_ASSERT(m_bytesLimiterTotal >= 0);
        return false;
    }

    if ((m_mainParseThread != nullptr) && m_mainParseThread->m_seekDuration == -1 && m_mainAudioThread != nullptr &&
        !m_mainAudioThread->m_isSeekingWhilePaused && m_mainVideoThread != nullptr &&
        !m_mainVideoThread->m_isSeekingWhilePaused)
    {
        m_mainParseThread->m_seekDuration = duration;
        m_packetsQueueCV.wakeAll();
    }

    return true;
}

void FFmpegDecoder::seekWhilePaused()
{
    if (m_isPaused)
    {
        InterlockedAdd(m_mainVideoThread->m_videoStartClock, av_gettime() / 1000000. - m_pauseTimer);
        m_pauseTimer = av_gettime() / 1000000.;

        m_mainAudioThread->m_isSeekingWhilePaused = true;
        m_mainVideoThread->m_isSeekingWhilePaused = true;
    }
}

bool FFmpegDecoder::seekByPercent(float percent, int64_t totalDuration)
{
    if (totalDuration < 0)
    {
        totalDuration = m_duration;
    }

    return seekDuration(totalDuration * percent);
}

void FFmpegDecoder::setFrameListener(VideoDisplay* listener)
{
    Q_ASSERT(listener);

    m_frameListener = listener;
    m_frameListener->setDecoderObject(this);
    setPixelFormat(m_frameListener->preferablePixelFormat());
    setResizeWithDecoder(m_frameListener->resizeWithDecoder());
}

bool FFmpegDecoder::pauseResume()
{
    if (m_mainAudioThread == nullptr || m_mainVideoThread == nullptr || m_mainParseThread == nullptr)
    {
        return false;
    }

    if (m_isPaused)
    {
        TAG("ffmpeg_pause") << "Unpause";
        TAG("ffmpeg_pause") << "Move >> " << av_gettime() / 1000000. - m_pauseTimer;
        InterlockedAdd(m_mainVideoThread->m_videoStartClock, av_gettime() / 1000000. - m_pauseTimer);
        m_isPaused = false;
    }
    else
    {
        TAG("ffmpeg_pause") << "Pause";
        m_isPaused = true;
        m_videoFramesCV.wakeAll();
        m_pauseTimer = av_gettime() / 1000000.;
    }

    return true;
}

void FFmpegDecoder::limitPlayback(qint64 limit, qint64 total)
{
    m_bytesLimiter = limit;
    m_bytesLimiterTotal = total;

    if (m_formatContext != nullptr && !m_openedFilePath.isEmpty())
    {
        startLimiterThread();
    }
}

void FFmpegDecoder::resetLimitPlayback()
{
    TAG("ffmpeg_readpacket_limitation") << "File full now, allowance disabled";
    m_bytesLimiter = -1;
    m_bytesLimiterTotal = -1;
    m_durationLimiter = -1;
}

int64_t FFmpegDecoder::duration() const { return m_duration; }

QByteArray FFmpegDecoder::getRandomFrame(const QString& file, double startPercent, double endPercent)
{
    QByteArray result;
    Q_ASSERT(startPercent <= endPercent);

    std::default_random_engine re(qHash(file));
    std::uniform_real_distribution<double> dis(startPercent, endPercent);

    double percents[32];

    for (auto& percent : percents)
    {
        percent = dis(re);
    }

    std::sort(std::begin(percents), std::end(percents));

    if (openFileDecoder(file) && m_videoStreamNumber >= 0)
    {
        allocateFrames();
        setPixelFormat(AV_PIX_FMT_RGB24);
        setResizeWithDecoder(true);
        setTargetSize(m_videoCodecContext->width, m_videoCodecContext->height);

        FPicture pic;
        for (const auto percent : percents)
        {
            int frame = m_duration * percent;
            if (frame < 0)
            {
                frame = 0;
                TAG("ffmpeg_getframe") << "Fault duration: " << file;
            }
            if (av_seek_frame(m_formatContext, m_videoStreamNumber, frame, 0) >= 0)
            {
                for (;;)
                {
                    AVPacket packet;

                    if (av_read_frame(m_formatContext, &packet) < 0)
                    {
                        if (0 == frame || (avcodec_flush_buffers(m_videoCodecContext),
                                           av_seek_frame(m_formatContext, m_videoStreamNumber, 0, 0) < 0))
                        {
                            break;
                        }
                        frame = 0;

                        continue;
                    }

                    if (packet.stream_index != m_videoStreamNumber)
                    {
                        av_packet_unref(&packet);
                        continue;
                    }

                    const int ret = avcodec_send_packet(m_videoCodecContext, &packet);

                    av_packet_unref(&packet);

                    if (ret < 0)
                    {
                        break;
                    }

                    if (avcodec_receive_frame(m_videoCodecContext, m_videoFrame) == 0
                        && frameToImage(pic))
                    {
                        Q_ASSERT(pic.format == AV_PIX_FMT_RGB24);
                        auto image = QImage(pic.width, pic.height, QImage::Format_RGB888);
                        memcpy(image.bits(), pic.data[0], pic.width * pic.height * 3);
                        Q_ASSERT(!image.isNull());

                        QByteArray ba;
                        {
                            QBuffer buffer(&ba);
                            buffer.open(QIODevice::WriteOnly);
                            image.save(&buffer, "jpg");
                        }
                        if (ba.size() > result.size())
                        {
                            result = std::move(ba);
                        }

                        break;
                    }
                }
            }
        }
    }
    close();
    return result;
}


double FFmpegDecoder::getDuration(const QString& file)
{
    double result = 0;
    if (openFileDecoder(file))
    {
        result = getDurationSecs(m_duration);
    }
    close();
    return result;
}

float FFmpegDecoder::aspectRatio() const
{
    float aspect_ratio = 0.0F;
    if (m_videoCodecContext != nullptr)
    {
        aspect_ratio = av_q2d(m_videoCodecContext->sample_aspect_ratio);
        if (aspect_ratio <= 0.0)
        {
            aspect_ratio = 1.0;
        }
        aspect_ratio *= (float)m_videoCodecContext->width / (float)m_videoCodecContext->height;
    }
    return aspect_ratio;
}

int64_t FFmpegDecoder::headerSize() const { return FFMAX(0, m_headerSize - 8); }

double FFmpegDecoder::getDurationSecs(int64_t duration) const
{
    return (m_videoStream != nullptr) ? av_q2d(m_videoStream->time_base) * duration : 0;
}


int64_t FFmpegDecoder::limiterBytes() const
{
    if (!isRunningLimitation())
    {
        return -1;
    }

    return m_mainTimeLimiterThread->m_readerBytesCurrent;
}

int64_t FFmpegDecoder::limiterDuration() const
{
    if (!isRunningLimitation())
    {
        return -1;
    }

    return m_durationLimiter;
}

bool FFmpegDecoder::isRunningLimitation() const
{
    return m_mainTimeLimiterThread != nullptr && m_mainTimeLimiterThread->isRunning();
}

bool FFmpegDecoder::isBrokenDuration() const { return m_fileProbablyNotFull; }

void FFmpegDecoder::setPixelFormat(AVPixelFormat format) { m_pixelFormat = format; }

void FFmpegDecoder::setResizeWithDecoder(bool policy) { m_resizeWithDecoder = policy; }
