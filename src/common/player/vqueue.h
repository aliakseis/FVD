#pragma once

// Video frame struct for RGB24 frame (used by displays)
struct VQueue
{
	VideoFrame m_frame[VIDEO_PICTURE_QUEUE_SIZE];
	int m_write_counter;
	int m_read_counter;
	int m_busy;

	VQueue() :
		m_write_counter(0),
		m_read_counter(0),
		m_busy(0)
	{}

	void clear()
	{
		for (uint i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; ++i)
		{
			m_frame[i].m_image.free();
		}

		memset(&m_frame, 0, sizeof(m_frame));

		// Reset readers
		m_write_counter = 0;
		m_read_counter = 0;
		m_busy = 0;
	}

	void setBasePts(double base, double pts)
	{
		for (uint i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; ++i)
		{
			m_frame[i].m_base = base;
			m_frame[i].m_pts = pts;
		}
	}
};
