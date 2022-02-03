#include <QFile>

#include "Preferences.h"
#include "ui_preferences.h"
#include <QKeyEvent>
#include <QStringList>
#include <QScrollBar>
#include <iostream>
#include <math.h>

Preferences::Preferences(QWidget* parent) :
	QDialog(parent),
	ui(new Ui::Preferences)
{
	ui->setupUi(this);
	ui->listLang->setWrapping(true);
	ui->listSites->setWrapping(true);
	ui->listSites->setFlow(QListView::LeftToRight);
	//ui->listSites->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	ui->listAdultSites->setWrapping(true);
	ui->listAdultSites->setFlow(QListView::LeftToRight);
	//ui->listAdultSites->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setPrefStyleSheet();

	QStringList strList;

	for (int i = 0; i < 8; i++)
	{
		strList << QString("AdultTest_%1").arg(i);
	}
	setListAbultSites(strList);
	for (int i = 0; i < 8; i++)
	{
		strList << QString("SiteTest_%1").arg(i);
	}
	setListSites(strList);
	for (int i = 0; i < 20; i++)
	{
		strList << QString("Test_%1").arg(i);
	}
	setListLanguage(strList);

	ui->listSites->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	ui->listAdultSites->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	ui->listLang->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

	connect(ui->listLang, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(onCurrItemLangChanged(QListWidgetItem*)));
	connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(onCurrTabChanged(int)));
}

Preferences::~Preferences()
{
	delete ui;
}

bool Preferences::setPrefStyleSheet()
{
	QString css = readStyleSheet();
	if (!css.isEmpty())
	{
		setStyleSheet(css);
		return true;
	}
	return false;
}

QString Preferences::readStyleSheet()
{
	QString str_path = QString("%1/../../../../../resources/images/Preferences/style.css").arg(QCoreApplication::applicationDirPath());
	QFile file(str_path);
	//QFile file(":/images/Preferences/style.css");
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		return QString();
	}
	return file.readAll();
}

void Preferences::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_F5)
	{
		setPrefStyleSheet();
	}


	QWidget::keyPressEvent(event);
}

void Preferences::setListLanguage(const QStringList& langs)
{
	foreach(const QString & str, langs)
	{
		QListWidgetItem* item = new QListWidgetItem(str, ui->listLang);
		item->setIcon(QIcon(":/images/Preferences/itemIcoLang.png"));
	}
}

void Preferences::setListSites(const QStringList& langs)
{
	foreach(const QString & str, langs)
	{
		QListWidgetItem* item = new QListWidgetItem(str, ui->listSites);
		item->setCheckState(Qt::Unchecked);
	}

	setContentSize(ui->listSites);
}

void Preferences::setContentSize(QListWidget* wdt)
{
	QSize size = wdt->size();
	QSize sizeHin = wdt->sizeHint();
	int shforCcol = wdt->sizeHintForColumn(0);
	int shforRow = wdt->sizeHintForRow(0);
	int countItem = wdt->count();

	int heightView = ceil(countItem / (double)(size.width() / shforCcol)) * shforRow;
	wdt->setMinimumHeight(heightView + 5);
}


void Preferences::setListAbultSites(const QStringList& langs)
{
	foreach(const QString & str, langs)
	{
		QListWidgetItem* item = new QListWidgetItem(str, ui->listAdultSites);
		item->setCheckState(Qt::Unchecked);
	}
	setContentSize(ui->listAdultSites);
}


void Preferences::onCurrItemLangChanged(QListWidgetItem* item)
{
	setCurLanguage(item->text());
}

void Preferences::setCurLanguage(const QString& lang)
{
	ui->labelSelectedLang->setText(lang);
}

void Preferences::onCurrTabChanged(int index)
{
	if (0 == index)
	{
		setMinimumSize(QSize(430, 595));
		setMaximumSize(QSize(430, 595));
	}
	else if (1 == index)
	{
		setMinimumSize(QSize(430, 350));
		setMaximumSize(QSize(430, 350));
	}
	else if (2 == index)
	{
		setMinimumSize(QSize(430, 300));
		setMaximumSize(QSize(430, 300));
	}
}
