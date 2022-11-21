#include "videodisplay.h"

#include "ffmpegdecoder.h"

VideoDisplay::VideoDisplay() : m_decoder(nullptr) {}

VideoDisplay::~VideoDisplay() = default;

void VideoDisplay::setDecoderObject(FFmpegDecoder* decoder)
{
    Q_ASSERT(decoder != nullptr);
    m_decoder = decoder;
}

void VideoDisplay::displayFrameFinished(unsigned int videoGeneration) 
{ 
    m_decoder->finishedDisplayingFrame(videoGeneration); 
}

void VideoDisplay::showPicture(const QImage& picture)
{
    // Override this
}

void VideoDisplay::showPicture(const QPixmap& picture)
{
    // Override this
}
