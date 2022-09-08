#pragma once

#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <QDeadlineTimer>

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
    void setWaiter(InterruptibleWaitCondition* waitCondition) { m_waitCondition = waitCondition; }

private:
    std::atomic_bool m_abort;
    std::atomic<InterruptibleWaitCondition*> m_waitCondition;
};

class InterruptibleWaitCondition
{
public:
    bool wait(QMutex* lock, QDeadlineTimer deadline = QDeadlineTimer(QDeadlineTimer::Forever))
    {
        ThreadControl* control = dynamic_cast<ThreadControl*>(QThread::currentThread());
        if (control != 0)
        {
            if (control->isAbort())
            {
                return true;
            }
            control->setWaiter(this);
        }

        const bool result = m_condition.wait(lock, deadline);

        if (control != 0)
        {
            control->setWaiter(nullptr);
        }

        return result;
    }

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
            control->setWaiter(this);
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
            control->setWaiter(nullptr);
        }

        return result;
    }

    void wakeOne() { m_condition.wakeOne(); }
    void wakeAll() { m_condition.wakeAll(); }

private:
    QWaitCondition m_condition;
};
