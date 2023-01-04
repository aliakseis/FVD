#include "timelimiterthread.h"

extern "C"
{
#include <libavformat/avformat.h>
}

void TimeLimiterThread::run()
{
    if (isAbort())
    {
        return;
    }
    if (m_parent->m_openedFilePath.isEmpty())
    {
        return;
    }

    QScopedPointer<FFmpegDecoder> thread_data(new FFmpegDecoder());
    if (!thread_data->openFileDecoder(m_parent->m_openedFilePath))
    {
        return;
    }
    AVPacket packet;
    if (m_parent->m_fileProbablyNotFull)
    {
        m_parent->m_duration = 0;
    }
    while (m_parent->m_bytesLimiter >= 0)
    {
        if (isAbort())
        {
            return;
        }

        while (m_parent->m_bytesLimiter >= 0 &&
               m_parent->m_bytesLimiter - thread_data->m_bytesCurrent > PLAYBACK_AVPACKET_MAX)
        {
            if (isAbort())
            {
                return;
            }

            if (av_read_frame(thread_data->m_formatContext, &packet) >= 0)
            {
                if (packet.pos > 0) {
                    m_readerBytesCurrent = thread_data->m_bytesCurrent = packet.pos;
                }

                if (packet.pos > 0 && m_parent->m_headerSize == 0)
                {
                    m_parent->m_headerSize = packet.pos;
                }

                if (packet.stream_index == thread_data->m_videoStreamNumber)
                {
                    if (packet.pts != AV_NOPTS_VALUE)
                    {
                        m_parent->m_durationLimiter = packet.pts;
                    }
                    else if (packet.dts != AV_NOPTS_VALUE)
                    {
                        m_parent->m_durationLimiter = packet.dts;
                    }
                    if (m_parent->m_fileProbablyNotFull)
                    {
                        m_parent->m_duration = m_parent->m_durationLimiter;
                    }
                }
                av_packet_unref(&packet);
            }
            else
            {
                break;
            }
        }

        preciseSleep(0.1);
    }

    while (m_parent->m_fileProbablyNotFull && av_read_frame(thread_data->m_formatContext, &packet) >= 0)
    {
        if (packet.pos > 0) {
            m_readerBytesCurrent = thread_data->m_bytesCurrent = packet.pos;
        }
        if (packet.stream_index == thread_data->m_videoStreamNumber)
        {
            if (packet.pts != AV_NOPTS_VALUE)
            {
                m_parent->m_durationLimiter = packet.pts;
            }
            else if (packet.dts != AV_NOPTS_VALUE)
            {
                m_parent->m_durationLimiter = packet.dts;
            }
            m_parent->m_duration = m_parent->m_durationLimiter;
        }
    }

    m_parent->m_fileProbablyNotFull = false;
}
