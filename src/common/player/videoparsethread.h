#pragma once

#include "ffmpegdecoder.h"

class VideoParseThread : public QThread, public ThreadControl
{
    friend class FFmpegDecoder;
    friend class DisplayThread;

private:
    std::atomic_bool m_isSeekingWhilePaused;
    double m_videoClock{};  ///< pts of last decoded frame / predicted pts of next decoded frame
    FFmpegDecoder* m_ffmpeg;

public:
    explicit VideoParseThread(FFmpegDecoder* parent) : m_isSeekingWhilePaused(false), m_ffmpeg(parent)
    {
        setObjectName("VideoParseThread");
    }
    void run() override;
private:
    bool handlePacket(AVPacket& packet);
    bool getVideoPacket(AVPacket* packet);
};
