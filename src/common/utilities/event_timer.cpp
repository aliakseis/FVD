#include "event_timer.h"

#include <QTimerEvent>


namespace utilities
{

void EventTimer::exec(int time)
{
	timer_id_ = startTimer(time);
	QEventLoop::exec();
}

void EventTimer::timerEvent(QTimerEvent* event)
{
	if (timer_id_ && timer_id_ == event->timerId())
	{
		killTimer(timer_id_);
		timer_id_ = 0;
		QEventLoop::exit(0);
	}
}

} // namespace utilities