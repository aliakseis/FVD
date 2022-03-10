#pragma once

#include <QWidget>

class QWindow;

class WindowWidget : public QWidget
{
	Q_OBJECT
public:
	explicit WindowWidget(QWidget* parent = 0);
	explicit WindowWidget(QWindow* hostedWindow, QWidget* parent = 0);

	QWindow* hostedWindow() const;
	void setHostedWindow(QWindow* window);
	void setVisible(bool visible);

protected:
	bool event(QEvent* evt);
	bool eventFilter(QObject* obj, QEvent* evt);

private:
	void init();
	void setHostedWindowHelper(QWindow* window);
	QWindow* m_hostedWindow;
};
