#pragma once
#ifndef AUDIOPARSETHREAD_H
#define AUDIOPARSETHREAD_H

#include "ffmpegdecoder.h"

#include <vector>

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

public:
    std::atomic<double> m_audioStartClock;

	explicit AudioParseThread(FFmpegDecoder* parent)
		: m_isSeekingWhilePaused(false)
		, m_ffmpeg(parent)
		, m_audioCurrentPref(m_ffmpeg->m_audioSettings)
	{
		setObjectName("AudioParseThread");
	};
	virtual void run() override;
	bool getAudioPacket(AVPacket* packet);
};


#endif //AUDIOPARSETHREAD_H
