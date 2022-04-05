#pragma once

#include "ffmpegdecoder.h"

class ParseThread : public QThread, public ThreadControl
{
    friend class FFmpegDecoder;
    friend class Test_FFmpegDecoder;

    std::atomic_int64_t m_seekDuration;
    bool reader_eof;
    bool readFrame(AVPacket* packet);
    void sendSeekPacket();
    void fixDuration();

public:
    explicit ParseThread(FFmpegDecoder* parent) : QThread((QObject*)parent), m_seekDuration(-1), reader_eof(false)
    {
        setObjectName("ParseThread");
    }
    void run() override;

    static void startAudioThread(FFmpegDecoder* parent);
    static void startVideoThread(FFmpegDecoder* parent);
};
