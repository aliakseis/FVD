#include "parsethread.h"

#include "audioparsethread.h"
#include "utilities/loggertag.h"
#include "videoparsethread.h"

extern "C"
{
#include <libavformat/avformat.h>
}


bool ParseThread::readFrame(AVPacket* packet)
{
    auto* parent = static_cast<FFmpegDecoder*>(this->parent());
    if (parent->m_bytesLimiter >= 0 && parent->m_bytesLimiter - parent->m_bytesCurrent < PLAYBACK_AVPACKET_MAX)
    {
        parent->m_downloading = true;

        parent->m_videoPacketsQueue.enqueue(parent->m_downloadingPacket, &parent->m_packetsQueueMutex,
                                            &parent->m_packetsQueueCV);
        parent->m_audioPacketsQueue.enqueue(parent->m_downloadingPacket, &parent->m_packetsQueueMutex,
                                            &parent->m_packetsQueueCV);

        double longSleepTime = parent->m_waitSleperTime;
        while (
            (parent->m_bytesLimiter >= 0 && parent->m_bytesLimiter - parent->m_bytesCurrent < PLAYBACK_AVPACKET_MAX) ||
            (parent->m_bytesLimiter >= 0 && longSleepTime > 0 && m_seekDuration < 0))  // 512kb move
        {
            TAG("ffmpeg_readpacket_limitation")
                << "Wait downloading limitation = "
                << PLAYBACK_AVPACKET_MAX - (parent->m_bytesLimiter - parent->m_bytesCurrent);

            if (isAbort())
            {
                emit parent->downloadPendingFinished();
                return false;  // Closing reading thread without packet
            }

            longSleepTime -= 0.05;
            preciseSleep(0.05);
        }

        QMutexLocker locker(&parent->m_downloadLockerMutex);
        parent->m_downloading = false;
        parent->m_downloadLockerWait.wakeAll();
    }

    int ret = av_read_frame(parent->m_formatContext, packet);
    if (ret >= 0)
    {
        parent->m_bytesCurrent = packet->pos;

        static int packet_max = 0;
        if (packet->size > packet_max)
        {
            packet_max = packet->size;
            TAG("ffmpeg_readpacket") << "Maximum packet size = " << packet_max;
        }

        reader_eof = false;
    }
    else
    {
        reader_eof = ret == AVERROR_EOF;
    }
    return ret >= 0;
}

void ParseThread::run()
{
    TAG("ffmpeg_threads") << "Parse thread started";
    auto* parent = static_cast<FFmpegDecoder*>(this->parent());
    AVPacket packet;
    bool eof = false;

    // detecting real framesize
    fixDuration();

    if (parent->m_audioStreamNumber >= 0)
    {
        parent->m_mainAudioThread = new AudioParseThread(parent);
        parent->m_mainAudioThread->start();
    }

    if (parent->m_videoStreamNumber >= 0)
    {
        parent->m_mainVideoThread = new VideoParseThread(parent);
        parent->m_mainVideoThread->start();
    }

    while (true)
    {
        if (isAbort())
        {
            parent->m_downloadLockerWait.wakeAll();
            TAG("ffmpeg_threads") << "Parse thread broken";
            return;
        }

        // seeking
        sendSeekPacket();

        if (readFrame(&packet))
        {
            if (packet.stream_index == parent->m_videoStreamNumber)
            {
                QMutexLocker locker(&parent->m_packetsQueueMutex);
                parent->m_packetsQueueCV.wait(
                    [this, parent]()
                    {
                        return parent->m_audioPacketsQueue.empty() ||
                               parent->m_videoPacketsQueue.packetsSize() < MAX_QUEUE_SIZE &&
                                   parent->m_videoPacketsQueue.size() < MAX_VIDEO_FRAMES ||
                               m_seekDuration >= 0;
                    },
                    &parent->m_packetsQueueMutex);
                parent->m_videoPacketsQueue.enqueue(packet);
                parent->m_packetsQueueCV.wakeAll();
            }
            else if (packet.stream_index == parent->m_audioStreamNumber)
            {
                QMutexLocker locker(&parent->m_packetsQueueMutex);
                parent->m_packetsQueueCV.wait(
                    [this, parent]()
                    {
                        return parent->m_videoPacketsQueue.empty() ||
                               parent->m_audioPacketsQueue.packetsSize() < MAX_QUEUE_SIZE &&
                                   parent->m_audioPacketsQueue.size() < MAX_AUDIO_FRAMES ||
                               m_seekDuration >= 0;
                    },
                    &parent->m_packetsQueueMutex);
                parent->m_audioPacketsQueue.enqueue(packet);
                parent->m_packetsQueueCV.wakeAll();
            }
            else
            {
                av_packet_unref(&packet);
            }

            eof = false;
        }
        else
        {
            if (eof)
            {
                const bool videoPacketsQueueIsEmpty = parent->m_videoPacketsQueue.empty();
                const bool audioPacketsQueueIsEmpty = parent->m_audioPacketsQueue.empty();
                if (videoPacketsQueueIsEmpty && audioPacketsQueueIsEmpty && parent->m_videoFramesQueue.m_busy == 0 ||
                    videoPacketsQueueIsEmpty && ((parent->m_seekFlags & 0x9) == 0x9) ||
                    audioPacketsQueueIsEmpty && ((parent->m_seekFlags & 0x6) == 0x6))
                {
                    parent->close();
                }

                preciseSleep(0.01);
            }
            eof = reader_eof;
        }

        // Continue packet reading
    }

    TAG("ffmpeg_threads") << "Decoding ended";
}

void ParseThread::sendSeekPacket()
{
    auto* parent = static_cast<FFmpegDecoder*>(this->parent());

    QMutexLocker ml(&parent->m_seekFlagsMtx);

    if (m_seekDuration >= 0 && ((((parent->m_seekFlags & 0x10) != 0) && ((parent->m_seekFlags & 0x20) != 0)) ||
                                parent->m_seekFlags == 0x00))  // seeking flags reset
    {
        TAG("ffmpeg_seek") << "Begin seek";

        if (avformat_seek_file(parent->m_formatContext, parent->m_videoStreamNumber, 0, m_seekDuration, m_seekDuration,
                               AVSEEK_FLAG_FRAME) < 0)
        {
            TAG("ffmpeg_seek") << "Seek failed";
            return;
        }

        parent->m_seekFrame = m_seekDuration.load();

        parent->m_seekFlags = 0x03;
        parent->m_seekFlagsCV.wakeAll();

        ml.unlock();

        parent->m_packetsQueueMutex.lock();

        // Update video packet
        parent->m_videoPacketsQueue.clear();
        parent->m_videoPacketsQueue.enqueue(parent->m_seekPacket);

        // Update audio packet
        parent->m_audioPacketsQueue.clear();
        parent->m_audioPacketsQueue.enqueue(parent->m_seekPacket);

        parent->m_packetsQueueCV.wakeAll();
        parent->m_packetsQueueMutex.unlock();

        m_seekDuration = -1;

        parent->seekWhilePaused();
    }
}

void ParseThread::fixDuration()
{
    auto* parent = static_cast<FFmpegDecoder*>(this->parent());
    AVPacket packet;
    if (parent->m_duration <= 0)
    {
        if (parent->m_bytesLimiter >= 0)
        {
            parent->m_fileProbablyNotFull = true;
            emit parent->fileProbablyNotFull();
            // If file not full the check code will not work correctly
            return;
        }

        parent->m_durationRecheckIsRun = true;

        // Reset rechecking vars
        parent->m_duration = 0;
        while (av_read_frame(parent->m_formatContext, &packet) >= 0)
        {
            if (packet.stream_index == parent->m_videoStreamNumber)
            {
                if (packet.pts != AV_NOPTS_VALUE)
                {
                    parent->m_duration = packet.pts;
                }
                else if (packet.dts != AV_NOPTS_VALUE)
                {
                    parent->m_duration = packet.dts;
                }
            }
            av_packet_unref(&packet);

            if (isAbort())
            {
                TAG("ffmpeg_threads") << "Parse thread broken";
                return;
            }
        }

        if (avformat_seek_file(parent->m_formatContext, parent->m_videoStreamNumber, 0, 0, 0, AVSEEK_FLAG_FRAME) < 0)
        {
            TAG("ffmpeg_seek") << "Seek failed";
            return;
        }
    }
}
