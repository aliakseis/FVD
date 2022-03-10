#pragma once

#ifdef ALLOW_TRAFFIC_CONTROL
#include "traffic_limitation/InterceptingApplication.h"
#else
#include "qtsingleapplication/qtsingleapplication.h"
#endif // ALLOW_TRAFFIC_CONTROL
#include "utilities/translatable.h"
#include <QKeyEvent>


#include "mainwindow.h"

class Application
#ifdef ALLOW_TRAFFIC_CONTROL
	: public traffic_limitation::InterceptingApp,
#else
	: public QtSingleApplication,
#endif // ALLOW_TRAFFIC_CONTROL
  public utilities::Translatable
{
public:
	Application(int& argc, char** argv, bool GUIenabled = true)
#ifdef ALLOW_TRAFFIC_CONTROL
		: traffic_limitation::InterceptingApp(argc, argv, GUIenabled)
#else
		: QtSingleApplication(argc, argv, GUIenabled)
#endif // ALLOW_TRAFFIC_CONTROL
	{}

	virtual bool eventFilter(QObject* target, QEvent* event) override
	{
		if (event->type() == QEvent::KeyPress)
		{
			QKeyEvent* keyEvent = (QKeyEvent*)event;
			if (keyEvent->modifiers() == Qt::AltModifier && keyEvent->nativeVirtualKey() == Qt::Key_X)
			{
				MainWindow::Instance()->closeApp();
			}
		}
		return QtSingleApplication::eventFilter(target, event);
	}
};
