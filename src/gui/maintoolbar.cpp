#include "maintoolbar.h"

#include <QStyle>

#include "ui_maintoolbar.h"
#include "utilities/translation.h"

#define PROP_ACTIVE "active"

MainToolBar::MainToolBar(QWidget* parent) : QWidget(parent), m_activeTabIndex(SEARCH_TAB), ui(new Ui::MainToolBar)
{
    ui->setupUi(this);
    std::pair<TabId, QToolButton*> tabsOrder[] = {std::make_pair(SEARCH_TAB, ui->btnSearch),
                                                  std::make_pair(DOWNLOADS_TAB, ui->btnDownloads),
                                                  std::make_pair(LIBRARY_TAB, ui->btnLibrary)};
    m_tabs = {std::begin(tabsOrder), std::end(tabsOrder)};
    activateTab(m_activeTabIndex);
    utilities::Tr::MakeRetranslatable(this, ui);
}

MainToolBar::~MainToolBar() { delete ui; }

void MainToolBar::on_btnSearch_clicked()
{
    activateTab(SEARCH_TAB);
    emit search();
}

void MainToolBar::on_btnDownloads_clicked()
{
    activateTab(DOWNLOADS_TAB);
    emit downloads(nullptr);
}

void MainToolBar::on_btnLibrary_clicked()
{
    activateTab(LIBRARY_TAB);
    emit library(nullptr);
}

void MainToolBar::on_btnHelp_clicked() { emit help(); }

void MainToolBar::on_btnSettings_clicked() { emit settings(); }

void MainToolBar::activateTab(MainToolBar::TabId index)
{
    Q_ASSERT(index >= 0 && index < (int)m_tabs.size());
    m_activeTabIndex = index;

    for (auto& tab : m_tabs)
    {
        tab.second->setProperty(PROP_ACTIVE, (tab.first == index));
        tab.second->style()->unpolish(tab.second);
        tab.second->style()->polish(tab.second);
    }
}

void MainToolBar::switchTab(bool switchToNext)
{
    const int next = (switchToNext ? 1 : -1);
    m_tabs[(m_activeTabIndex + next + m_tabs.size()) % m_tabs.size()].second->click();
}

void MainToolBar::openDownloadsTab(const DownloadEntity* selEntity)
{
    activateTab(DOWNLOADS_TAB);
    emit downloads(selEntity);
}

void MainToolBar::openLibraryTab(const DownloadEntity* selEntity)
{
    activateTab(LIBRARY_TAB);
    emit library(selEntity);
}

int MainToolBar::getCurrTabIndex() { return m_activeTabIndex; }
