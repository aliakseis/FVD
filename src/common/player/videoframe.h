#pragma once

#include "fpicture.h"

struct VideoFrame
{
    unsigned int m_generation;
    double m_pts;
    int64_t m_duration;
    FPicture m_image;
};
