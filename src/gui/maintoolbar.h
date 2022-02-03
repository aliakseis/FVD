#pragma once
#ifndef MAINTOOLBAR_H
#define MAINTOOLBAR_H

#include <QWidget>
#include <utility>
#include <vector>

namespace Ui
{
class MainToolBar;
}

class QToolButton;
class DownloadEntity;

class MainToolBar : public QWidget
{
	Q_OBJECT
public:

	explicit MainToolBar(QWidget* parent = 0);
	~MainToolBar();

	void switchTab(bool switchToNext);

	void openDownloadsTab(const DownloadEntity* selEntity);
	void openLibraryTab(const DownloadEntity* selEntity);

	int getCurrTabIndex();

signals:
	void downloads(const DownloadEntity* selEntity);
	void search();
	void help();
	void settings();
	void library(const DownloadEntity* selEntity);

private slots:
	void on_btnDownloads_clicked();
	void on_btnSearch_clicked();
	void on_btnHelp_clicked();
	void on_btnSettings_clicked();
	void on_btnLibrary_clicked();

private:

	enum TabId
	{
		SEARCH_TAB,
		DOWNLOADS_TAB,
		LIBRARY_TAB
	};

	void activateTab(MainToolBar::TabId index);
	std::vector<std::pair<TabId, QToolButton*> > m_tabs;
	TabId m_activeTabIndex;
	Ui::MainToolBar* ui;
};

#endif // MAINTOOLBAR_H
