#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}


struct FPicture : public AVFrame
{
	FPicture()
	{
		reset();
	}

	~FPicture()
	{
		free();
	}

	void free()
	{
        av_frame_unref(this);
		reset();
	}

	void alloc(AVPixelFormat pix_fmt, int width, int height)
	{
		//avpicture_alloc(this, pix_fmt, width, height);
		this->width = width;
		this->height = height;
		this->format = pix_fmt;
        av_frame_get_buffer(this, 1); // ?
	}

	void realloc(AVPixelFormat pix_fmt, int width, int height)
	{
		free();
		alloc(pix_fmt, width, height);
	}

	void reallocForSure(AVPixelFormat pix_fmt, int width, int height)
	{
		if (pix_fmt != this->format || width != this->width || height != this->height)
		{
			realloc(pix_fmt, width, height);
		}
	}

	void copyToForSure(FPicture* dest)
	{
        dest->reallocForSure(static_cast<AVPixelFormat>(format), width, height);
        av_frame_copy(dest, this);
	}
private:
	void reset()
	{
		memset(this, 0, sizeof(AVFrame));
		width = 0;
		height = 0;
        format = AV_PIX_FMT_NONE;
	}
};
