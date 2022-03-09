#pragma once
#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include "fpicture.h"

struct VideoFrame
{
	double m_base;
	double m_pts;
	int64_t m_duration;
	FPicture m_image;
};

#endif