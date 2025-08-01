#pragma once

#include <vector>

#include "ffmpegdecoder.h"

class AudioParseThread : public QThread, public ThreadControl
{
    friend class FFmpegDecoder;

private:
    std::atomic_bool m_isSeekingWhilePaused;
    FFmpegDecoder* m_ffmpeg;
    FFmpegDecoder::AudioParams m_audioCurrentPref;

    std::vector<uint8_t> m_resampleBuffer;

private:
    void handlePacket(const AVPacket& packet);
    void seek(const AVPacket& packet);

public:
    explicit AudioParseThread(FFmpegDecoder* parent)
        : m_isSeekingWhilePaused(false), m_ffmpeg(parent), m_audioCurrentPref(m_ffmpeg->m_audioSettings)
    {
        setObjectName("AudioParseThread");
    }
    void run() override;
    bool getAudioPacket(AVPacket* packet);
};
