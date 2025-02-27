#pragma once

// Video frame struct for RGB24 frame (used by displays)
struct VQueue
{
    VideoFrame m_frames[VIDEO_PICTURE_QUEUE_SIZE]{};
    int m_write_counter{0};
    int m_read_counter{0};
    int m_busy{0};

    void clear()
    {
        for (uint i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; ++i)
        {
            m_frames[i].m_image.free();
        }

        // Reset readers
        m_write_counter = 0;
        m_read_counter = 0;
        m_busy = 0;
    }
};
