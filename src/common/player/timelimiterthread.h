#pragma once

#include "ffmpegdecoder.h"

class TimeLimiterThread : public QThread, public ThreadControl
{
	friend class FFmpegDecoder;
private:
	FFmpegDecoder* m_parent;
	int64_t m_readerBytesCurrent;
public:
	explicit TimeLimiterThread(FFmpegDecoder* parent) : m_parent(parent), m_readerBytesCurrent(0)
	{
		setObjectName("TimeLimiterThread");
	};
	virtual void run() override;
};
