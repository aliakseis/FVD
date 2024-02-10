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

    while (!isAbort())
    {
        {
            QMutexLocker locker(&ff->m_videoFramesMutex);
            ff->m_videoFramesCV.wait([ff]() { 
                    return !ff->m_frameDisplayingRequested && ff->m_videoFramesQueue.m_busy != 0;
                }, &ff->m_videoFramesMutex);
        }
        // Break thread
        if (isAbort())
        {
            return;
        }

        VideoFrame* current_frame = &ff->m_videoFramesQueue.m_frames[ff->m_videoFramesQueue.m_read_counter];
        ff->m_frameDisplayingRequested = true;

        // Frame skip
        if ((ff->m_videoFramesQueue.m_busy > 1 &&
            ff->m_videoStartClock + current_frame->m_pts < getCurrentTime())
            || ff->m_generation != current_frame->m_generation)
        {
            TAG("ffmpeg_threads") << __FUNCTION__ << "Framedrop: " << current_frame->m_pts;
            ff->finishedDisplayingFrame(ff->m_videoGeneration);
            continue;
        }

        const auto pts = current_frame->m_pts;
        const auto generation = current_frame->m_generation;
        const auto duration = current_frame->m_duration;

        // Give it time to render frame
        if (ff->m_frameListener != nullptr)
        {
            ff->m_frameListener->renderFrame(current_frame->m_image, ff->m_videoGeneration);
        }

        while (!isAbort() && ff->m_generation == generation)
        {
            const double delay = ff->m_videoStartClock + pts - getCurrentTime();
            if (delay < 0.005) {
                break;
            }

            if (delay > 0.1)
            {
                preciseSleep(0.1);
                continue;
            }

            preciseSleep(delay);
            break;
        }
        // Break thread
        if (isAbort())
        {
            return;
        }

        // It's time to display converted frame
        emit ff->changedFramePosition(duration, ff->m_duration);

        if (ff->m_frameListener != nullptr)
        {
            ff->m_frameListener->displayFrame(ff->m_videoGeneration);
        }
        else
        {
            ff->finishedDisplayingFrame(ff->m_videoGeneration);
        }
    }
}
