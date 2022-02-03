#include "videoplayer.h"
#include "widgetdisplay.h"

#include "ffmpegdecoder.h"

#include <QDebug>

VideoPlayer::VideoPlayer()
	: m_decoder(std::make_unique<FFmpegDecoder>())
    , m_display(nullptr)
	, m_state(InitialState)
{

}

VideoPlayer::~VideoPlayer()
{
	delete m_display;
}

void VideoPlayer::setDefaultDisplay()
{
	m_display = new WidgetDisplay();
}

FFmpegDecoder* VideoPlayer::getDecoder()
{
	return m_decoder.get();
}

VideoDisplay* VideoPlayer::getCurrentDisplay()
{
	return m_display;
}

void VideoPlayer::setDisplay(VideoDisplay* display)
{
	Q_ASSERT(display);

	
	
		delete m_display;
	

	m_display = display;
	m_decoder->setFrameListener(m_display);
}

void VideoPlayer::setState(VideoState newState)
{
	qDebug() << __FUNCTION__ << "newState:" << newState;
	m_state = newState;
}
