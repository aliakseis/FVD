#include "videoparsethread.h"
#include "displaythread.h"
#include "utilities/loggertag.h"
#include "utilities/logger.h"
#include "interlockedadd.h"

bool VideoParseThread::getVideoPacket(AVPacket* packet)
{
	QMutexLocker locker(&m_ffmpeg->m_packetsQueueMutex);
	m_ffmpeg->m_packetsQueueCV.wait([this]() { return !m_ffmpeg->m_videoPacketsQueue.empty(); }, &m_ffmpeg->m_packetsQueueMutex);
	if (!isAbort())
	{
		*packet = m_ffmpeg->m_videoPacketsQueue.dequeue();
		m_ffmpeg->m_packetsQueueCV.wakeAll();
		return true;
	}
	return false;
}

void VideoParseThread::run()
{
	TAG("ffmpeg_threads") << "Video thread started";
	AVPacket packet;

	m_ffmpeg->m_videoPTS = (double)AV_NOPTS_VALUE;
	m_videoStartClock = av_gettime() / 1000000.;
	m_videoClock = 0;

	// Help displaying thread
	m_ffmpeg->m_mainDisplayThread = new DisplayThread(this);
	m_ffmpeg->m_mainDisplayThread->start();

    bool frameFinished = false;
    while (true)
	{
		if (isAbort())
		{
			TAG("ffmpeg_threads") << "Video thread broken";
			return;
		}

		if (m_ffmpeg->m_isPaused && !m_isSeekingWhilePaused)
		{
			preciseSleep(0.1);
			continue;
		}

        bool seekDone = false;

        if (!frameFinished && getVideoPacket(&packet))
        {
            if (isAbort())
            {
                av_packet_unref(&packet);
                TAG("ffmpeg_threads") << "Video thread broken";
                return;
            }

            // Downloading section
            if (packet.data == m_ffmpeg->m_downloadingPacket.data)
            {
                if (m_ffmpeg->m_mainAudioThread != nullptr)
                {
                    emit m_ffmpeg->downloadPendingStarted();
                }
                {
                    QMutexLocker locker(&m_ffmpeg->m_downloadLockerMutex);
                    m_ffmpeg->m_downloadLockerWait.wait([this]() { return !m_ffmpeg->m_downloading; }, &m_ffmpeg->m_downloadLockerMutex);
                }

                while (true)
                {
                    if (isAbort())
                    {
                        return;
                    }

                    if (!getVideoPacket(&packet))
                    {
                        return;
                    }

                    if (packet.data == m_ffmpeg->m_downloadingPacket.data)
                    {
                        continue;
                    }

                    if (packet.data == m_ffmpeg->m_seekPacket.data)
                    {
                        break;
                    }

                    //qDebug() << "decoding video packet";

                    if (avcodec_send_packet(m_ffmpeg->m_videoCodecContext, &packet) < 0) {
                        return;
                    }
                    frameFinished = avcodec_receive_frame(m_ffmpeg->m_videoCodecContext, m_ffmpeg->m_videoFrame) == 0;
                    if (frameFinished)
                    {
                        int64_t pts = m_ffmpeg->m_videoFrame->best_effort_timestamp;
                        if (pts == AV_NOPTS_VALUE)
                        {
                            pts = 0;
                        }
                        const double stamp = av_q2d(m_ffmpeg->m_videoStream->time_base) * (double)pts;
                        const double curTime = av_gettime() / 1000000.;
                        m_videoStartClock = curTime - stamp;
                        m_ffmpeg->m_videoPTS = stamp;

                        m_ffmpeg->m_pauseTimer = curTime;

                        break;
                    }
                }
                if (m_ffmpeg->m_mainAudioThread != nullptr)
                {
                    emit m_ffmpeg->downloadPendingFinished();
                }
            }

            Q_ASSERT(packet.data != m_ffmpeg->m_downloadingPacket.data);

            // Seeking part
            if (packet.data == m_ffmpeg->m_seekPacket.data)
            {
                avcodec_flush_buffers(m_ffmpeg->m_videoCodecContext);

                while (true)
                {
                    if (isAbort())
                    {
                        TAG("ffmpeg_threads") << "Video thread broken";
                        return;
                    }

                    if (getVideoPacket(&packet))
                    {
                        Q_ASSERT(packet.data != m_ffmpeg->m_seekPacket.data);

                        if (packet.data == m_ffmpeg->m_downloadingPacket.data)
                        {
                            //qDebug() << "video downloading mode started [" << &packet << "]";
                            if (m_ffmpeg->m_mainAudioThread != nullptr)
                            {
                                emit m_ffmpeg->downloadPendingStarted();
                            }
                            {
                                QMutexLocker locker(&m_ffmpeg->m_downloadLockerMutex);
                                m_ffmpeg->m_downloadLockerWait.wait([this]() { return !m_ffmpeg->m_downloading; }, &m_ffmpeg->m_downloadLockerMutex);
                            }
                            if (m_ffmpeg->m_mainAudioThread != nullptr)
                            {
                                emit m_ffmpeg->downloadPendingFinished();
                            }

                            continue;
                        }

                        if (avcodec_send_packet(m_ffmpeg->m_videoCodecContext, &packet) < 0) {
                            return;
                        }
                        frameFinished = avcodec_receive_frame(m_ffmpeg->m_videoCodecContext, m_ffmpeg->m_videoFrame) == 0;
                        if (frameFinished)
                        {
                            int64_t pts = m_ffmpeg->m_videoFrame->best_effort_timestamp;
                            if (pts == AV_NOPTS_VALUE)
                            {
                                pts = 0;
                            }
                            double stamp = av_q2d(m_ffmpeg->m_videoStream->time_base) * (double)pts;

                            // Before sync we must sure that audio thread also begining sync
                            const bool isAE = (m_ffmpeg->m_mainAudioThread != nullptr);

                            QMutexLocker ml(&m_ffmpeg->m_seekFlagsMtx);
                            //qDebug() << "VSIN: " << seekflags << " = " << *seekflags;
                            m_ffmpeg->m_seekFlags |= (isAE) ? 0x4 : 0xC;
                            m_ffmpeg->m_seekFlagsCV.wakeAll();
                            if (isAE)
                            {
                                m_ffmpeg->m_seekFlagsCV.wait(
                                    [this]() { return (m_ffmpeg->m_seekFlags & 0x8) != 0; },
                                    &m_ffmpeg->m_seekFlagsMtx);
                            }

                            m_videoStartClock = av_gettime() / 1000000. - stamp;
                            m_ffmpeg->m_videoPTS = stamp;
                            m_ffmpeg->m_videoFramesQueue.setBasePts(m_videoStartClock, stamp);

                            m_ffmpeg->m_seekFlags &= (isAE) ? ~0x1 : ~0x3;
                            m_ffmpeg->m_seekFlagsCV.wakeAll();
                            if (isAE)
                            {
                                m_ffmpeg->m_seekFlagsCV.wait(
                                    [this]() { return (m_ffmpeg->m_seekFlags & 0x2) == 0; },
                                    &m_ffmpeg->m_seekFlagsMtx);
                            }

                            //qDebug() << "VSOUT: " << seekflags << " = " << *seekflags;
                            m_ffmpeg->m_seekFlags |= (isAE) ? 0x10 : 0x30;
                            m_ffmpeg->m_seekFlagsCV.wakeAll();

                            seekDone = true;

                            break;
                        }
                    }
                } // while
            }
            else
            {
                if (avcodec_send_packet(m_ffmpeg->m_videoCodecContext, &packet) < 0) {
                    return;
                }
                frameFinished = avcodec_receive_frame(m_ffmpeg->m_videoCodecContext, m_ffmpeg->m_videoFrame) == 0;
            }

            av_packet_unref(&packet);
        } // if (getVideoPacket(&packet))

		// Did we get a video frame?
		if (frameFinished)
		{
			int64_t duration_stamp =  m_ffmpeg->m_videoFrame->best_effort_timestamp;
			double pts = duration_stamp;

			/* compute the exact PTS for the picture if it is omitted in the stream
				* pts1 is the dts of the pkt / pts of the frame */
			if (pts == AV_NOPTS_VALUE)
			{
				pts = m_videoClock;
			}
			else
			{
				pts *= av_q2d(m_ffmpeg->m_videoStream->time_base);
				m_videoClock = pts;
			}

			/* update video clock for next frame */
			m_frameDelay = av_q2d(m_ffmpeg->m_videoCodecContext->time_base);
			/* for MPEG2, the frame can be repeated, so we update the
				clock accordingly */
			m_frameDelay += m_ffmpeg->m_videoFrame->repeat_pict * (m_frameDelay * 0.5);
			m_videoClock += m_frameDelay;

			// Skipping frames
            double curTime;
            if (!seekDone && m_videoStartClock + pts <= (curTime = av_gettime() / 1000000.))
			{
				if (m_videoStartClock + pts < curTime - 1.)
				{
                    InterlockedAdd(m_videoStartClock, 1.); // adjust clock
				}

				TAG("ffmpeg_sync") << "Hard skip frame";

				// pause
				if (m_ffmpeg->m_isPaused && !m_isSeekingWhilePaused)
				{
					continue;
				}
			}
            else
            {
                {
                    QMutexLocker locker(&m_ffmpeg->m_videoFramesMutex);

                    m_ffmpeg->m_videoFramesCV.wait(
                        [this]()
                    {
                        return m_ffmpeg->m_isPaused && !m_isSeekingWhilePaused || m_ffmpeg->m_videoFramesQueue.m_busy < VIDEO_PICTURE_QUEUE_SIZE;
                    },
                        &m_ffmpeg->m_videoFramesMutex);

                    Q_ASSERT(isAbort() || m_ffmpeg->m_isPaused && !m_isSeekingWhilePaused ||
                        !(m_ffmpeg->m_videoFramesQueue.m_write_counter == m_ffmpeg->m_videoFramesQueue.m_read_counter && m_ffmpeg->m_videoFramesQueue.m_busy > 0)); // It can crash on memcpy if that
                }

                if (isAbort())
                {
                    TAG("ffmpeg_threads") << "Video thread broken";
                    return;
                }

                if (m_ffmpeg->m_isPaused && !m_isSeekingWhilePaused)
                {
                    continue;
                }

                if (seekDone)
                {
                    m_isSeekingWhilePaused = false;
                }

                int wrcount = m_ffmpeg->m_videoFramesQueue.m_write_counter;
                VideoFrame* current_frame = &m_ffmpeg->m_videoFramesQueue.m_frames[wrcount];
                m_ffmpeg->frameToImage();

                // If target frame not good, it will be reallocated
                m_ffmpeg->m_videoFrameData.copyToForSure(&current_frame->m_image);

                current_frame->m_base = m_videoStartClock;
                current_frame->m_pts = pts;
                current_frame->m_duration = duration_stamp;

                m_ffmpeg->m_videoFramesQueue.m_write_counter = (m_ffmpeg->m_videoFramesQueue.m_write_counter + 1) % std::size(m_ffmpeg->m_videoFramesQueue.m_frames);

                QMutexLocker locker(&m_ffmpeg->m_videoFramesMutex);
                m_ffmpeg->m_videoFramesQueue.m_busy++;
                Q_ASSERT(m_ffmpeg->m_videoFramesQueue.m_busy <= VIDEO_PICTURE_QUEUE_SIZE);
                m_ffmpeg->m_videoFramesCV.wakeAll();
            }
            frameFinished = avcodec_receive_frame(m_ffmpeg->m_videoCodecContext, m_ffmpeg->m_videoFrame) == 0;
		}
	}
}
