#include "windowwidget.h"
#include <QWindow>
#include <QGuiApplication>
#include <QDebug>

WindowWidget::WindowWidget(QWidget* parent) :
	QWidget(parent), m_hostedWindow(0)
{
	init();
}


WindowWidget::WindowWidget(QWindow* window, QWidget* parent)
	: QWidget(parent) , m_hostedWindow(window)
{
	init();
}

void WindowWidget::init()
{
	//setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NoBackground);
	setHostedWindowHelper(m_hostedWindow);
}

void WindowWidget::setHostedWindowHelper(QWindow* window)
{
	if (m_hostedWindow)
	{
		m_hostedWindow->removeEventFilter(this);
	}

	m_hostedWindow = window;
	if (m_hostedWindow)
	{
		m_hostedWindow->installEventFilter(this);
		//m_hostedWindow->setWindowFlags(m_hostedWindow->windowFlags()|Qt::FramelessWindowHint);
		m_hostedWindow->setParent(windowHandle());
		m_hostedWindow->resize(size());
		m_hostedWindow->setVisible(isVisible());
	}
}

QWindow* WindowWidget::hostedWindow() const
{
	return m_hostedWindow;
}

void WindowWidget::setHostedWindow(QWindow* window)
{
	setHostedWindowHelper(window);
}

void WindowWidget::setVisible(bool visible)
{
	QWidget::setVisible(visible);
	if (m_hostedWindow)
	{
		m_hostedWindow->setVisible(visible);
	}
}

bool WindowWidget::event(QEvent* evt)
{
	bool ret = QWidget::event(evt);
	if (m_hostedWindow)
	{
		//        if (evt->type() == QEvent::UpdateRequest)
		//            return true;
		switch (evt->type())
		{
		case QEvent::Wheel:
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			qGuiApp->sendEvent(m_hostedWindow, evt);
			break;
		case QEvent::Show:
			if (!m_hostedWindow->parent())
			{
				m_hostedWindow->setParent(windowHandle());
			}
			break;
		case QEvent::Resize:
			m_hostedWindow->resize(this->size());
			break;
		case QEvent::Move:
			break;
		case QEvent::Hide:
			windowHandle()->close();
			break;
		}
	}
	return ret;
}

bool WindowWidget::eventFilter(QObject* obj, QEvent* evt)
{
	if (obj == m_hostedWindow)
	{
		if (this->size().isEmpty() && evt->type() == QEvent::Expose)
		{
			return true;
		}
	}
	return false;
}
