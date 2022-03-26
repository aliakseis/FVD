#pragma once

#include <QDebug>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <QEventLoop>
#include <QFile>
#include <QMutex>
#include <QPixmap>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

#include "threadcontrol.h"

#ifndef VERIFY
#ifdef QT_NO_DEBUG
#define VERIFY(x) (x)
#else
#define VERIFY(x) Q_ASSERT(x)
#endif
#endif

enum
{
    MAX_QUEUE_SIZE = (15 * 1024 * 1024),
    MAX_VIDEO_FRAMES = 8,
    MAX_AUDIO_FRAMES = 1000,
    VIDEO_PICTURE_QUEUE_SIZE = 2,   // enough for displaying one frame.
    PLAYBACK_AVPACKET_MAX = 524288  // AVPacket maximum size for downloading items.
};

#include <atomic>

#include "fpicture.h"
#include "fqueue.h"
#include "videoframe.h"
#include "vqueue.h"

void preciseSleep(double sec);

// Threads-helpers
class ParseThread;
class VideoParseThread;
class AudioParseThread;
class TimeLimiterThread;
class DisplayThread;

class VideoDisplay;
class Test_FFmpegDecoder;  // test class

class FFmpegDecoder : public QObject
{
    friend class Test_FFmpegDecoder;
    Q_OBJECT
public:
    FFmpegDecoder();
    ~FFmpegDecoder();

    void openFile(QString file);
    bool seekMs(int64_t ts);
    bool seekFrame(int64_t frame);
    bool seekDuration(int64_t duration);
    bool seekByPercent(float percent, int64_t totalDuration = -1);

    int64_t seekPostion() const;
    int64_t duration() const;
    int64_t framesCount() const;
    int64_t headerSize() const;

    double volume() const;

    void setTargetSize(int width, int height, bool is_ceil = false);
    float aspectRatio() const;

    QRect setPreferredSize(const QSize& size, int scr_xleft = 0, int scr_ytop = 0);
    QRect setPreferredSize(int scr_width, int scr_height, int scr_xleft = 0, int scr_ytop = 0);
    QRect getPreferredSize(int scr_width, int scr_height, int scr_xleft = 0, int scr_ytop = 0) const;
    inline bool isPlaying() const { return m_isPlaying; }
    inline bool isPaused() const { return m_isPaused; }

    inline bool isFileLoaded() const { return m_formatContext != NULL; };

    void setFrameListener(VideoDisplay* listener);

    QByteArray getRandomFrame(const QString& file, double startPercent = 0, double endPercent = 1.);

    void setDefaultImageOnClose(bool set) { m_setDefaultImageOnClose = set; }

    void setDownloadBufferingAwait(double seconds)
    {
        Q_ASSERT(seconds >= 0);
        m_waitSleperTime = seconds;
    };

    double getDurationSecs(int64_t duration) const
    {
        return (m_videoStream != 0) ? av_q2d(m_videoStream->time_base) * duration : 0;
    }

    // Limiter functions
    bool isRunningLimitation() const;
    int64_t limiterDuration() const;
    int64_t limiterBytes() const;
    bool isBrokenDuration() const;

    void finishedDisplayingFrame();

private:
    // Frame display listener
    VideoDisplay* m_frameListener;

    // Indicators
    std::atomic_bool m_isReadReady;
    std::atomic_bool m_isPlaying;

    // Limit playback by bytes
    bool m_fileProbablyNotFull;
    bool m_durationRecheckIsRun;
    std::atomic_int64_t m_bytesLimiter;
    std::atomic_int64_t m_bytesLimiterTotal;
    std::atomic_int64_t m_bytesCurrent;
    std::atomic<double> m_waitSleperTime;
    std::atomic_int64_t m_durationLimiter;

    std::atomic_int64_t m_headerSize;

    QMutex m_downloadLockerMutex;
    InterruptibleWaitCondition m_downloadLockerWait;
    bool m_downloading;

    // Threads
    friend class ParseThread;
    friend class AudioParseThread;
    friend class VideoParseThread;
    friend class TimeLimiterThread;
    friend class DisplayThread;
    VideoParseThread* m_mainVideoThread;
    AudioParseThread* m_mainAudioThread;
    ParseThread* m_mainParseThread;
    DisplayThread* m_mainDisplayThread;
    TimeLimiterThread* m_mainTimeLimiterThread;
    QMutex m_mainTimeLimiterStarter;

    // Syncronization
    std::atomic<double> m_audioPTS;
    std::atomic<double> m_videoPTS;

    // Real frame number and duration from video stream
    int64_t m_duration;
    int64_t m_frameTotalCount;
    bool m_streamDurationDetection;

    // Closing loop
    QEventLoop m_closingEventLoop;
    QEventLoop m_loadedEventLoop;

    // Basic file path
    QString m_openedFilePath;

    // Output video image
    bool m_stableDisplay;

    int m_targetWidth;
    int m_targetHeight;

    // Basic stuff
    AVFormatContext* m_formatContext;

    // Flush packet
    AVPacket m_seekPacket;
    unsigned int m_seekFlags;

    std::atomic_int64_t m_seekFrame;

    QMutex m_seekFlagsMtx;
    InterruptibleWaitCondition m_seekFlagsCV;

    // Downloading packet
    AVPacket m_downloadingPacket;

    // Video Stuff
    AVCodec* m_videoCodec;
    AVCodecContext* m_videoCodecContext;
    AVStream* m_videoStream;
    int m_videoStreamNumber;

    // Audio Stuff
    AVCodec* m_audioCodec;
    AVCodecContext* m_audioCodecContext;
    AVStream* m_audioStream;
    int m_audioStreamNumber;
    SwrContext* m_audioSwrContext;
    struct AudioParams
    {
        int frequency;
        int channels;
        int64_t channel_layout;
        AVSampleFormat format;
    } m_audioSettings;

    // Audio stuff
    AVFrame* m_audioFrame;

    // Stuff for converting image
    AVFrame* m_videoFrame;
    SwsContext* m_imageCovertContext;
    QMutex m_resizeMutex;
    bool m_resizeWithDecoder;
    FPicture m_videoFrameData;
    AVPixelFormat m_pixelFormat;

    // Video and audio threads stuff
    FQueue m_videoPacketsQueue;
    FQueue m_audioPacketsQueue;

    QMutex m_packetsQueueMutex;
    InterruptibleWaitCondition m_packetsQueueCV;

    VQueue m_videoFramesQueue;

    QMutex m_videoFramesMutex;
    InterruptibleWaitCondition m_videoFramesCV;

    std::atomic_bool m_isPaused;
    std::atomic<double> m_pauseTimer;

    bool m_setDefaultImageOnClose;

    // Audio
    void* m_stream{};
    std::atomic<double> m_volume = 1.;

    void resetVariables();
    void closeProcessing();
    FPicture* frameToImage();
    double audioClock() const;
    double videoClock() const;

private:
    void setPixelFormat(AVPixelFormat format);
    void setResizeWithDecoder(bool policy);
    void correctDisplay(bool is_ceil = false);
    bool openFileDecoder(const QString& file);
    void allocateFrames();
    void startLimiterThread();

    void seekWhilePaused();

public slots:
    void close(bool isBlocking = true);
    void play(bool isPaused = false);
    bool pauseResume();
    void setVolume(double volume);
    void setVolume(int volume, int steep = 100);
    void limitPlayback(qint64 limit, qint64 total);
    void resetLimitPlayback();

protected slots:
    void openFileProcessing();

signals:
    void changedFramePosition(qint64 frame, qint64 total);
    void decoderClosed();
    void fileReleased(bool setDefaultImage);
    void fileLoaded();
    void processOpenning();
    void volumeChanged(double volume);

    void downloadPendingStarted();
    void downloadPendingFinished();

    void playingFinished();
    void fileProbablyNotFull();
};
