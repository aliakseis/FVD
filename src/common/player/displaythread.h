#pragma once
#ifndef DISPLAYTHREAD_H
#define DISPLAYTHREAD_H

#include "ffmpegdecoder.h"

class DisplayThread : public QThread, public ThreadControl
{
	Q_OBJECT
	friend class FFmpegDecoder;
public:
	explicit DisplayThread(VideoParseThread* parent) : m_parent(parent)
	{
		setObjectName("DisplayThread");
	}
	virtual void run();
private:
	VideoParseThread* m_parent;
};

#endif //DISPLAYTHREAD_H
