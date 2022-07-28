#include "displaythread.h"

#include "utilities/loggertag.h"
#include "videodisplay.h"
#include "videoparsethread.h"

extern "C"
{
#include <libavutil/time.h>
}

void DisplayThread::run()
{
    TAG("ffmpeg_threads") << "Displaying thread started";
    FFmpegDecoder* ff = m_parent->m_ffmpeg;

    while (true)
    {
        // Break thread
        if (isAbort())
        {
            TAG("ffmpeg_threads") << "Displaying thread broken";
            return;
        }

        {
            QMutexLocker locker(&ff->m_videoFramesMutex);
            ff->m_videoFramesCV.wait([ff]() { return ff->m_videoFramesQueue.m_busy != 0; }, &ff->m_videoFramesMutex);
        }
        // Break thread
        if (isAbort())
        {
            TAG("ffmpeg_threads") << "Displaying thread broken";
            return;
        }

        VideoFrame* current_frame = &ff->m_videoFramesQueue.m_frames[ff->m_videoFramesQueue.m_read_counter];

        // Frame skip
        if ((ff->m_videoFramesQueue.m_busy > 1 &&
            m_parent->m_videoStartClock + current_frame->m_pts < (av_gettime() / 1000000.))
            || ff->m_generation != current_frame->m_generation)
        {
            TAG("ffmpeg_threads") << __FUNCTION__ << "Framedrop: " << current_frame->m_pts;
            ff->finishedDisplayingFrame();
            continue;
        }

        // Give it time to render frame
        if (ff->m_frameListener != nullptr)
        {
            ff->m_frameListener->renderFrame(current_frame->m_image);
        }

        double delay;
        while (!isAbort() && (delay = m_parent->m_videoStartClock + current_frame->m_pts - av_gettime() / 1000000.) > 0
            && ff->m_generation == current_frame->m_generation)
        {
            // wait time must be not too short or too long. Bounds: 0.001 < target_waittime < 3.0 secs.
            preciseSleep(qBound<double>(0.001, delay / 2., 3.0));
        }
        // Break thread
        if (isAbort())
        {
            TAG("ffmpeg_threads") << "Displaying thread broken";
            return;
        }

        // It's time to display converted frame
        emit ff->changedFramePosition(current_frame->m_duration, ff->m_duration);

        if (ff->m_frameListener != nullptr)
        {
            ff->m_frameListener->displayFrame();
        }
        else
        {
            ff->finishedDisplayingFrame();
        }
    }
}
