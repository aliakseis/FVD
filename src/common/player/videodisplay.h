#pragma once

#include <QImage>
#include <QPixmap>

extern "C"
{
#include <libavutil/pixfmt.h>
}

class FFmpegDecoder;

struct FPicture;

class VideoDisplay
{
public:
    VideoDisplay();
    virtual ~VideoDisplay();
    virtual void renderFrame(const FPicture& frame, unsigned int videoGeneration) = 0;
    virtual void displayFrame(unsigned int videoGeneration) = 0;
    void setDecoderObject(FFmpegDecoder* decoder);

    virtual void showPicture(const QImage& picture);
    virtual void showPicture(const QPixmap& picture);

    virtual AVPixelFormat preferablePixelFormat() const = 0;
    virtual bool resizeWithDecoder() const = 0;

protected:
    void displayFrameFinished(unsigned int videoGeneration);

    FFmpegDecoder* m_decoder;
};
