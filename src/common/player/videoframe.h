#pragma once

#include "fpicture.h"

struct VideoFrame
{
    VideoFrame() = default;
    VideoFrame(const VideoFrame&) = delete;
    VideoFrame& operator=(const VideoFrame&) = delete;

    unsigned int m_generation;
    double m_pts;
    int64_t m_duration;
    FPicture m_image;
};
