#include "audioparsethread.h"

#include <portaudio.h>

#include "interlockedadd.h"
#include "utilities/logger.h"
#include "utilities/loggertag.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
}


namespace {

#if LIBAVUTIL_VERSION_MAJOR < 57

int64_t getChannelLayout(AVFrame* audioFrame)
{
	const int audioFrameChannels = audioFrame->channels;
    return ((audioFrame->channel_layout != 0u) &&
        audioFrameChannels == av_get_channel_layout_nb_channels(audioFrame->channel_layout))
        ? audioFrame->channel_layout
        : av_get_default_channel_layout(audioFrameChannels);
}

#else

auto getChannelLayout(AVFrame* audioFrame)
{
    return audioFrame->ch_layout;
}

bool operator == (const AVChannelLayout& left, const AVChannelLayout& right)
{
    return av_channel_layout_compare(&left, &right) == 0;
}

bool operator != (const AVChannelLayout& left, const AVChannelLayout& right)
{
    return av_channel_layout_compare(&left, &right) != 0;
}

#endif

} // namespace


FFmpegDecoder::AudioParams::AudioParams(int freq, int chans, AVSampleFormat fmt)
    : frequency(freq), format(fmt)
{
#if LIBAVUTIL_VERSION_MAJOR < 57
    channels = chans;
    channel_layout = av_get_default_channel_layout(chans);
#else
    av_channel_layout_default(&channel_layout, chans);
#endif
}

FFmpegDecoder::AudioParams::~AudioParams()
{
#if LIBAVUTIL_VERSION_MAJOR >= 57
    av_channel_layout_uninit(&channel_layout);
#endif
}


bool AudioParseThread::getAudioPacket(AVPacket* packet)
{
    QMutexLocker locker(&m_ffmpeg->m_packetsQueueMutex);
    m_ffmpeg->m_packetsQueueCV.wait([this]() { return !m_ffmpeg->m_audioPacketsQueue.empty(); },
                                    &m_ffmpeg->m_packetsQueueMutex);
    if (!isAbort())
    {
        *packet = m_ffmpeg->m_audioPacketsQueue.dequeue();
        m_ffmpeg->m_packetsQueueCV.wakeAll();
        return true;
    }
    return false;
}

void AudioParseThread::run()
{
    TAG("ffmpeg_threads") << "Audio thread started";

    setPriority(QThread::TimeCriticalPriority);

    AVPacket packet;

    bool handlePacketPostponed = false;

    while (!isAbort())
    {
        if (m_ffmpeg->m_isPaused && !m_isSeekingWhilePaused)
        {
            if (!Pa_IsStreamStopped(m_ffmpeg->m_stream))
            {
                Pa_StopStream(m_ffmpeg->m_stream);
            }

            if (isAbort())
            {
                if (handlePacketPostponed)
                {
                    av_packet_unref(&packet);
                }
                return;
            }

            preciseSleep(0.1);
            continue;
        }

        if (handlePacketPostponed)
        {
            handlePacketPostponed = false;
            if (m_isSeekingWhilePaused)
            {
                av_packet_unref(&packet);
            }
            else
            {
                handlePacket(packet);
            }
        }

        while (getAudioPacket(&packet))
        {
            bool flush_packet_found = false;

            // Downloading section
            if (packet.data == m_ffmpeg->m_downloadingPacket.data)
            {
                emit m_ffmpeg->downloadPendingStarted();
                {
                    QMutexLocker locker(&m_ffmpeg->m_downloadLockerMutex);
                    m_ffmpeg->m_downloadLockerWait.wait([this]() { return !m_ffmpeg->m_downloading; },
                                                        &m_ffmpeg->m_downloadLockerMutex);
                }

                while (true)
                {
                    if (!getAudioPacket(&packet))
                    {
                        return;
                    }

                    if (packet.data == m_ffmpeg->m_downloadingPacket.data)
                    {
                        continue;
                    }

                    if (packet.data == m_ffmpeg->m_seekPacket.data)
                    {
                        flush_packet_found = true;
                        break;
                    }

                    if (packet.pts != AV_NOPTS_VALUE)
                    {
                        m_ffmpeg->m_audioPTS = av_q2d(m_ffmpeg->m_audioStream->time_base) * packet.pts;
                    }
                    else
                    {
                        Q_ASSERT(false && "No audio pts found");
                        return;
                    }

                    break;
                }
                emit m_ffmpeg->downloadPendingFinished();
            }

            Q_ASSERT(packet.data != m_ffmpeg->m_downloadingPacket.data);

            // Seeking audio
            if (packet.data == m_ffmpeg->m_seekPacket.data)
            {
                ++m_ffmpeg->m_generation;
                avcodec_flush_buffers(m_ffmpeg->m_audioCodecContext);
                Pa_AbortStream(m_ffmpeg->m_stream);
                while (true)
                {
                    if (!getAudioPacket(&packet))
                    {
                        return;
                    }
                    Q_ASSERT(packet.data != m_ffmpeg->m_seekPacket.data);

                    if (packet.data != m_ffmpeg->m_downloadingPacket.data)
                    {
                        const bool isVE = (m_ffmpeg->m_mainVideoThread != nullptr);

                        QMutexLocker ml(&m_ffmpeg->m_seekFlagsMtx);
                        m_ffmpeg->m_seekFlags |= (isVE) ? 0x8 : 0xC;
                        m_ffmpeg->m_seekFlagsCV.wakeAll();
                        if (isVE)
                        {
                            m_ffmpeg->m_seekFlagsCV.wait([this]() { return (m_ffmpeg->m_seekFlags & 0x4) != 0; },
                                &m_ffmpeg->m_seekFlagsMtx);
                        }

                        if (packet.pts != AV_NOPTS_VALUE)
                        {
                            m_ffmpeg->m_audioPTS = av_q2d(m_ffmpeg->m_audioStream->time_base) * packet.pts;
                        }
                        else
                        {
                            qDebug() << "No audio pts found";
                            m_ffmpeg->close();
                        }

                        m_ffmpeg->m_seekFlags &= (isVE) ? ~0x2 : ~0x3;
                        m_ffmpeg->m_seekFlagsCV.wakeAll();
                        if (isVE)
                        {
                            m_ffmpeg->m_seekFlagsCV.wait([this]() { return (m_ffmpeg->m_seekFlags & 0x1) == 0; },
                                &m_ffmpeg->m_seekFlagsMtx);
                        }

                        // qDebug() << "ASOUT: " << seekflags << " = " << *seekflags;
                        m_ffmpeg->m_seekFlags |= (isVE) ? 0x20 : 0x30;
                        m_ffmpeg->m_seekFlagsCV.wakeAll();

                        TAG("ffmpeg_sync") << "Audio position: " << m_ffmpeg->m_audioPTS;

                        break;
                    }

                    emit m_ffmpeg->downloadPendingStarted();
                    {
                        QMutexLocker locker(&m_ffmpeg->m_downloadLockerMutex);
                        m_ffmpeg->m_downloadLockerWait.wait([this]() { return !m_ffmpeg->m_downloading; },
                                                            &m_ffmpeg->m_downloadLockerMutex);
                    }
                    emit m_ffmpeg->downloadPendingFinished();
                }  // while

                if (m_isSeekingWhilePaused)
                {
                    m_isSeekingWhilePaused = false;
                    handlePacketPostponed = true;

                    break;
                }
            }

            if (packet.size == 0)
            {
                TAG("ffmpeg_audio") << "Packet size = 0";
                break;
            }

            handlePacket(packet);
            av_packet_unref(&packet);

            if (m_ffmpeg->m_isPaused && !m_isSeekingWhilePaused)
            {
                break;
            }
        }
    }
}

void AudioParseThread::handlePacket(const AVPacket& packet)
{
    const int ret = avcodec_send_packet(m_ffmpeg->m_audioCodecContext, &packet);
    if (ret < 0)
    {
        return;
    }

    while (avcodec_receive_frame(m_ffmpeg->m_audioCodecContext, m_ffmpeg->m_audioFrame) == 0)
    {
        int original_buffer_size =
            av_samples_get_buffer_size(nullptr, 
#if LIBAVUTIL_VERSION_MAJOR < 57
                m_ffmpeg->m_audioFrame->channels,
#else
                m_ffmpeg->m_audioFrame->ch_layout.nb_channels,
#endif
                m_ffmpeg->m_audioFrame->nb_samples,
                (AVSampleFormat)m_ffmpeg->m_audioFrame->format, 1);

        // write buffer
        uint8_t* write_data = (m_ffmpeg->m_audioFrame->extended_data != nullptr)
                                  ? *m_ffmpeg->m_audioFrame->extended_data
                                  : m_ffmpeg->m_audioFrame->data[0];
        int64_t write_size = original_buffer_size;

        auto dec_channel_layout = getChannelLayout(m_ffmpeg->m_audioFrame);

        const int wanted_nb_samples = m_ffmpeg->m_audioFrame->nb_samples;  // No sync resampling

        // Check is the new swr context require
        if (m_ffmpeg->m_audioSwrContext == nullptr ||
            (AVSampleFormat)m_ffmpeg->m_audioFrame->format != m_audioCurrentPref.format ||
            dec_channel_layout != m_audioCurrentPref.channel_layout ||
            m_ffmpeg->m_audioFrame->sample_rate != m_audioCurrentPref.frequency)
        {
            swr_free(&m_ffmpeg->m_audioSwrContext);

#if LIBAVUTIL_VERSION_MAJOR < 57

            m_ffmpeg->m_audioSwrContext = swr_alloc_set_opts(
                nullptr, m_ffmpeg->m_audioSettings.channel_layout, m_ffmpeg->m_audioSettings.format, m_ffmpeg->m_audioSettings.frequency,
                dec_channel_layout, (AVSampleFormat)m_ffmpeg->m_audioFrame->format, m_ffmpeg->m_audioFrame->sample_rate,
                0, nullptr);

#else
            swr_alloc_set_opts2(&m_ffmpeg->m_audioSwrContext, 
                &m_ffmpeg->m_audioSettings.channel_layout, m_ffmpeg->m_audioSettings.format, m_ffmpeg->m_audioSettings.frequency,
                &dec_channel_layout, (AVSampleFormat)m_ffmpeg->m_audioFrame->format, m_ffmpeg->m_audioFrame->sample_rate,
                0, nullptr);

#endif

            if ((m_ffmpeg->m_audioSwrContext == nullptr) || swr_init(m_ffmpeg->m_audioSwrContext) < 0)
            {
                qCritical() << "[FFMPEG] unable to initialize swr convert context";
            }

            m_audioCurrentPref.format = (AVSampleFormat)m_ffmpeg->m_audioFrame->format;

#if LIBAVUTIL_VERSION_MAJOR < 57
            m_audioCurrentPref.channels = m_ffmpeg->m_audioFrame->channels;
            m_audioCurrentPref.channel_layout = dec_channel_layout;
#else
            av_channel_layout_uninit(&m_audioCurrentPref.channel_layout);
            av_channel_layout_copy(&m_audioCurrentPref.channel_layout, &dec_channel_layout);
#endif

            m_audioCurrentPref.frequency = m_ffmpeg->m_audioFrame->sample_rate;
        }

        int converted_size = 0;
        if (m_ffmpeg->m_audioSwrContext != nullptr)
        {
            const int out_count =
                (int64_t)wanted_nb_samples * m_ffmpeg->m_audioSettings.frequency / m_ffmpeg->m_audioFrame->sample_rate +
                256;

            const int size_multiplier =
                m_ffmpeg->m_audioSettings.num_channels() * av_get_bytes_per_sample(m_ffmpeg->m_audioSettings.format);

            const size_t buffer_size = out_count * size_multiplier;

            if (m_resampleBuffer.size() < buffer_size)
            {
                m_resampleBuffer.resize(buffer_size);
            }
            uint8_t* out = m_resampleBuffer.data();

            converted_size = swr_convert(m_ffmpeg->m_audioSwrContext, &out, out_count,
                                         (m_ffmpeg->m_audioFrame->extended_data != nullptr)
                                             ? (const uint8_t**)m_ffmpeg->m_audioFrame->extended_data
                                             : (const uint8_t**)&m_ffmpeg->m_audioFrame->data[0],
                                         m_ffmpeg->m_audioFrame->nb_samples);

            if (converted_size < 0)
            {
                qCritical() << "swr_convert() failed";
                break;
            }

            if (converted_size == out_count)
            {
                qWarning() << "audio buffer is probably too small";
                swr_init(m_ffmpeg->m_audioSwrContext);
            }

            write_data = out;
            write_size = converted_size * size_multiplier;
        }

        //const double frame_clock =
        //    (double)original_buffer_size / (m_ffmpeg->m_audioFrame->channels * m_ffmpeg->m_audioFrame->sample_rate *
        //                                    av_get_bytes_per_sample((AVSampleFormat)m_ffmpeg->m_audioFrame->format));

        const double frame_clock = m_ffmpeg->m_audioFrame->sample_rate != 0 
            ? double(m_ffmpeg->m_audioFrame->nb_samples) / m_ffmpeg->m_audioFrame->sample_rate : 0;

        const double delta = getCurrentTime() - m_ffmpeg->m_videoStartClock - m_ffmpeg->m_audioPTS;
        if (fabs(delta) > 0.1)
        {
            TAG("ffmpeg_sync") << "Audio sync delta = " << delta;
            InterlockedAdd(m_ffmpeg->m_videoStartClock, delta / 2);
        }

        if (write_size > 0)
        {
            const auto m_FrameSize =
                m_ffmpeg->m_audioSettings.num_channels() * av_get_bytes_per_sample(m_ffmpeg->m_audioSettings.format);
            const auto framesToWrite = write_size / m_FrameSize;

            if (Pa_IsStreamStopped(m_ffmpeg->m_stream))
            {
                Pa_StartStream(m_ffmpeg->m_stream);
            }

            if (m_ffmpeg->m_stream != nullptr)
            {
                const double volume = m_ffmpeg->m_volume;
                if (volume != 1)
                {
                    auto* realData = (int16_t*)write_data;
                    for (unsigned int i = 0; i < write_size / 2; ++i)
                    {
                        realData[i] *= volume;
                    }
                }

                auto err = Pa_WriteStream(m_ffmpeg->m_stream, write_data, framesToWrite);
                if (err == paUnanticipatedHostError)
                {
                    Pa_CloseStream(m_ffmpeg->m_stream);
                    if (m_ffmpeg->openAudioProcessing())
                        swr_free(&m_ffmpeg->m_audioSwrContext);
                }
                else if (err != paNoError)
                {
                    qCritical() << "[FFMPEG] unable to write to audio stream error = " << err;
                }
            }
            InterlockedAdd(m_ffmpeg->m_audioPTS, frame_clock);
        }
    }
}
