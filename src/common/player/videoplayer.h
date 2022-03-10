#pragma once

#include "videodisplay.h"

#include <memory>

class FFmpegDecoder;

class VideoPlayer
{
public:
	enum VideoState
	{
		InitialState,
		Playing,
		Paused,
		PendingHeader,
		PendingHeaderPaused,
	};

	VideoPlayer();
	virtual ~VideoPlayer();

	FFmpegDecoder* getDecoder();
	VideoDisplay* getCurrentDisplay();
	virtual void setDisplay(VideoDisplay* display);
	VideoState state() const { return m_state; }

protected:
	virtual void setDefaultDisplay();
	void setState(VideoState newState);

private:
	std::unique_ptr<FFmpegDecoder> m_decoder;
	VideoDisplay* m_display;
	VideoState m_state;
};
