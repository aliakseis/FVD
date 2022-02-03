#include "threadcontrol.h"


ThreadControl::ThreadControl()
	: m_abort(false)
	, m_waitCondition(nullptr)
{
}

ThreadControl::~ThreadControl()
= default;

void ThreadControl::setAbort()
{
	m_abort = true;

	if (InterruptibleWaitCondition* const waitCondition = m_waitCondition)
	{
		waitCondition->wakeAll();
	}

	if (auto* thread = dynamic_cast<QThread*>(this))
	{
		thread->disconnect();
		thread->exit(0);
	}
}

bool ThreadControl::isAbort() const
{
	return m_abort;
}
