#pragma once

extern "C"
{
#include <libavcodec/avcodec.h>
}

struct FPicture
{
    FPicture()
        : m_frame(av_frame_alloc())
    {}
    FPicture(const FPicture&) = delete;
    FPicture& operator=(const FPicture&) = delete;

    ~FPicture() { av_frame_free(&m_frame); }

    void free()
    {
        av_frame_unref(m_frame);
    }

    void realloc(AVPixelFormat pix_fmt, int width, int height)
    {
        if (pix_fmt != m_frame->format || width != m_frame->width || height != m_frame->height)
        {
            free();
            m_frame->width = width;
            m_frame->height = height;
            m_frame->format = pix_fmt;
            av_frame_get_buffer(m_frame, 1);  // ?
        }
    }

    int width() const { return m_frame->width; }
    int height() const { return m_frame->height; }
    int* linesize() const { return m_frame->linesize; }
    uint8_t ** data() const { return m_frame->data; }
    auto format() const { return m_frame->format; }

    AVFrame* m_frame;
};
