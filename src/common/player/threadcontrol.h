#pragma once

#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <QDeadlineTimer>

#include <type_traits>
#include <utility>

class InterruptibleWaitCondition;  // clang?

class ThreadControl
{
    friend class InterruptibleWaitCondition;

public:
    ThreadControl();
    virtual ~ThreadControl();
    void setAbort();
    bool isAbort() const;

private:
    void setWaiter(InterruptibleWaitCondition* waitCondition, QMutex* lock) 
    { 
        m_waitCondition = waitCondition; 
        m_lock = lock;
    }

private:
    std::atomic_bool m_abort;
    std::atomic<InterruptibleWaitCondition*> m_waitCondition;
    std::atomic<QMutex*> m_lock;
};

class InterruptibleWaitCondition
{
public:
    template <typename P>
    bool wait(P pred, QMutex* lock, QDeadlineTimer deadline = QDeadlineTimer(QDeadlineTimer::Forever))
    {
        ThreadControl* control = dynamic_cast<ThreadControl*>(QThread::currentThread());
        if (control != 0)
        {
            if (control->isAbort())
            {
                return true;
            }
            control->setWaiter(this, lock);
        }

        bool result(true);

        while (!(control != 0 && control->isAbort() || pred()))
        {
            result = m_condition.wait(lock, deadline);
            if (!result)
            {
                break;
            }
        }

        if (control != 0)
        {
            control->setWaiter(nullptr, nullptr);
        }

        return result;
    }

    bool wait(QMutex* lock, QDeadlineTimer deadline = QDeadlineTimer(QDeadlineTimer::Forever))
    {
        return this->wait(std::false_type(), lock, std::move(deadline));
    }

    void wakeOne() { m_condition.wakeOne(); }
    void wakeAll() { m_condition.wakeAll(); }

private:
    QWaitCondition m_condition;
};
