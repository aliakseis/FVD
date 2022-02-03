#pragma once

#include <QEventLoop>

class QTimerEvent;

namespace utilities
{

class EventTimer : protected QEventLoop
{
	Q_OBJECT

public:
	EventTimer() : timer_id_(0) {}

	void exec(int time);

protected:
	void timerEvent(QTimerEvent* event);

private:
	int timer_id_;
};

} // namespace utilities