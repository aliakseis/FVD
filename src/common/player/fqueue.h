#pragma once

#include "threadcontrol.h"

class FQueue : public QQueue<AVPacket>
{
public:
    FQueue() : QQueue<AVPacket>(), m_packetsSize(0) {}

    AVPacket dequeue()
    {
        AVPacket packet = QQueue<AVPacket>::dequeue();
        m_packetsSize -= packet.size;
        Q_ASSERT(m_packetsSize >= 0);
        return packet;
    }

    AVPacket dequeue(QMutex* mutex, InterruptibleWaitCondition* cond)
    {
        QMutexLocker locker(mutex);
        AVPacket packet = dequeue();
        cond->wakeAll();
        return packet;
    }

    void enqueue(const AVPacket& packet)
    {
        m_packetsSize += packet.size;
        Q_ASSERT(m_packetsSize >= 0);
        QQueue<AVPacket>::enqueue(packet);
    }

    void enqueue(const AVPacket& packet, QMutex* mutex, InterruptibleWaitCondition* cond)
    {
        QMutexLocker locker(mutex);
        enqueue(packet);
        cond->wakeAll();
    }

    int size(QMutex* mutex) const
    {
        QMutexLocker locker(mutex);
        return QQueue<AVPacket>::size();
    }

    int size() const { return QQueue<AVPacket>::size(); }

    int64_t packetsSize() const { return m_packetsSize; }

    void clear()
    {
        AVPacket packet;
        while (size() > 0)
        {
            packet = dequeue();
            av_packet_unref(&packet);
        }
        Q_ASSERT(m_packetsSize == 0);
    }

    void clear(QMutex* mutex, InterruptibleWaitCondition* cond)  // with lock
    {
        QMutexLocker locker(mutex);
        clear();
        cond->wakeAll();
    }

private:
    int64_t m_packetsSize;
};
