#pragma once

#include "libraryqmllistener.h"

#include <QMainWindow>


namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = 0);
	~MainWindow();

protected:
	void keyReleaseEvent(QKeyEvent* ev);

private:
	Ui::MainWindow* ui;

	LibraryQmlListener m_qmlListener;
	QFileSystemModel m_model;
};
