#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "librarymodel.h"

#include "qrangemodel.h"

#include <QKeyEvent>
#include <QDebug>


#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QtQuick/QQuickView>
#include <QtQml/QQmlContext>
#include <QtQml/qqml.h>
#include "windowwidget.h"
#else
#include "qdeclarativetoplevelitem.h"

#include <QDeclarativeView>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <qdeclarative.h>
#endif


MainWindow::MainWindow(QWidget* parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	LibraryModel* model = new LibraryModel(this);
	m_qmlListener.m_model = model;

	ui->setupUi(this);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
	QQuickView* view = new QQuickView();
	view->setResizeMode(QQuickView::SizeRootObjectToView);
	QQmlContext* ctxt = view->rootContext();
	ctxt->setContextProperty("qmllistener", &m_qmlListener);
	ctxt->setContextProperty("libraryModel", model);
	view->setSource(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../qml2.0/LibraryView.qml"));
#else
	qmlRegisterType<QRangeModel>("QtComponents", 1, 0, "RangeModel");
	qmlRegisterType<QtDeclarativeTopLevelItem>("QtComponents", 1, 0, "TopLevelItemHelper");
	QDeclarativeView* view = new QDeclarativeView(this);
	view->setResizeMode(QDeclarativeView::SizeRootObjectToView);
	setCentralWidget(view);
	QDeclarativeContext* ctxt = view->rootContext();
	ctxt->setContextProperty("qmllistener", &m_qmlListener);
	ctxt->setContextProperty("libraryModel", model);
	view->setSource(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../qml/LibraryView.qml"));
#endif



#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))

	WindowWidget* ww = new WindowWidget(view, this);
	//view->showNormal();
	//view->resize(500, 300);
	setCentralWidget(ww);
#endif
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::keyReleaseEvent(QKeyEvent* ev)
{
	if (ev->key() == Qt::Key_D)
	{
		m_qmlListener.m_model->addFakeRow();
	}
	else if (ev->key() == Qt::Key_Delete)
	{
		m_qmlListener.m_model->removeRow(0);
	}
}
