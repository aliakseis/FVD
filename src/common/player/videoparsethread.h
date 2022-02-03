#pragma once
#ifndef VIDEOPARSETHREAD_H
#define VIDEOPARSETHREAD_H

#include "ffmpegdecoder.h"

class VideoParseThread : public QThread, public ThreadControl
{
	friend class FFmpegDecoder;
	friend class DisplayThread;
private:
    std::atomic_bool m_isSeekingWhilePaused;
	double  m_videoClock;                          ///< pts of last decoded frame / predicted pts of next decoded frame
	double  m_frameDelay;
	FFmpegDecoder* m_ffmpeg;
	int64_t m_nb_frame;
public:
    std::atomic<double> m_videoStartClock;

	explicit VideoParseThread(FFmpegDecoder* parent)
		: m_isSeekingWhilePaused(false)
		, m_ffmpeg(parent)
	{
		setObjectName("VideoParseThread");
	};
	virtual void run() override;
	bool getVideoPacket(AVPacket* packet);
};

#endif //VIDEOPARSETHREAD_H