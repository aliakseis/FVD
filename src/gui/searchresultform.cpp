#include "searchresultform.h"

#include <QAction>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QToolTip>
#include <utility>

#include "branding.hxx"
#include "downloadentity.h"
#include "downloadsform.h"
#include "itemsdelegate.h"
#include "libraryform.h"
#include "mainwindow.h"
#include "scriptprovider.h"
#include "searchlistmodel.h"
#include "searchmanager.h"
#include "settings_declaration.h"
#include "sitescheckboxdelegate.h"
#include "ui_searchresultform.h"
#include "videoqualitydialog.h"
#include "videowidget.h"

using namespace utilities;

namespace
{
const utilities::Tr::Translation OPEN_IN_BROWSER = {"SearchResultsForm", "Open in browser"};
}

SearchResultForm::SearchResultForm(QWidget* parent) : QFrame(parent), ui(new Ui::SearchResultForm), m_searchPage(1)
{
    ui->setupUi(this);
    ui->manageLabel->setVisible(false);
    ui->manageLabel->setImages(PreviewPanelButton::LeftArrow);

    ui->cbSites->setItemDelegate(new SitesCheckboxDelegate(ui->cbSites));

    ui->leSearch->installEventFilter(qApp);
    ui->cbSites->installEventFilter(qApp);

    m_model = new SearchListModel(ui->searchResultView);
    m_model->setHeaderView(ui->searchResultView->header());

    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSortRole(Qt::EditRole);
    m_proxyModel->setSourceModel(m_model);
    ui->searchResultView->setModel(m_proxyModel);
    ui->searchResultView->setSortingEnabled(true);
    ui->searchResultView->header()->setSortIndicatorShown(false);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->sort(-1, Qt::AscendingOrder);

    // initially hide the sort indicators
    ui->searchResultView->header()->setSortIndicatorShown(false);
    ui->searchResultView->header()->setSectionResizeMode(SR_Index, QHeaderView::Fixed);
    ui->searchResultView->header()->setSectionResizeMode(SR_Icon, QHeaderView::Fixed);
    ui->searchResultView->header()->setSectionResizeMode(SR_Title, QHeaderView::Stretch);
    ui->searchResultView->header()->setSectionResizeMode(SR_Description, QHeaderView::Fixed);
    ui->searchResultView->header()->setSectionResizeMode(SR_Length, QHeaderView::Fixed);
    ui->searchResultView->header()->setSectionResizeMode(SR_Status, QHeaderView::Fixed);

    ui->searchResultView->setColumnWidth(SR_Index, 28);
    ui->searchResultView->setColumnWidth(SR_Icon, 32);
    ui->searchResultView->setColumnWidth(SR_Title, 200);
    ui->searchResultView->setColumnWidth(SR_Description, 150);
    ui->searchResultView->setColumnWidth(SR_Length, 50);
    ui->searchResultView->setColumnWidth(SR_Status, 135);

    auto* delegate = new ItemsDelegate(ui->searchResultView);
    ui->searchResultView->setItemDelegate(delegate);
    ui->searchResultView->viewport()->setMouseTracking(true);
    ui->searchResultView->installEventFilter(this);

    populateSitesCombo(true);
    createActions();
    showSearchButton();
    updatePrevNextState();

    VERIFY(connect(ui->searchResultView, SIGNAL(doubleClicked(QModelIndex)), SLOT(onViewDoubleClick(QModelIndex))));
    VERIFY(
        connect(ui->searchResultView, SIGNAL(customContextMenuRequested(QPoint)), SLOT(onSearchContextMenu(QPoint))));
    VERIFY(connect(ui->leSearch, SIGNAL(returnPressed()), SLOT(doSearch())));
    VERIFY(connect(ui->btnSearch, SIGNAL(clicked()), SLOT(doSearch())));
    VERIFY(connect(ui->btnStopSearch, SIGNAL(clicked()), SLOT(stopSearch())));
    VERIFY(connect(delegate, SIGNAL(downloadClicked(int)), SLOT(onDownload(int))));
    VERIFY(connect(delegate, SIGNAL(downloadMenuClicked(int, QPoint)), SLOT(onDownloadContextMenu(int, QPoint))));
    VERIFY(connect(&SearchManager::Instance(), SIGNAL(searchFinished()), SLOT(onSearchFinished())));
    VERIFY(connect(&SearchManager::Instance(), SIGNAL(searchFailed(const QString&)),
                   SLOT(onSearchFailed(const QString&))));
    // Preview change
    VERIFY(connect(ui->searchResultView->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
                   SLOT(onSelectionChanged(QItemSelection, QItemSelection))));

    VERIFY(connect(m_model, SIGNAL(updateCurrentRow()), SLOT(onActivated())));

    VERIFY(connect(ui->searchResultView->verticalScrollBar(), SIGNAL(valueChanged(int)),
                   SLOT(onVerticalScrollChanged(int))));

    utilities::Tr::MakeRetranslatable(this, ui);
}

SearchResultForm::~SearchResultForm() { delete ui; }

bool SearchResultForm::populateSitesComboHelper(const QStringList& strategiesNames, const QIcon& sectionIcon,
                                                const QString& sectionName)
{
    QSettings settings;
    QSet<QString> checkedSitesSet = settings.value(app_settings::CheckedSites, app_settings::CheckedSites_Default)
                                        .toString()
                                        .split(';', QString::SkipEmptyParts)
                                        .toSet();
    QSet<QString> settingSitesSet = settings.value(app_settings::Sites, app_settings::Sites_Default)
                                        .toString()
                                        .split(';', QString::SkipEmptyParts)
                                        .toSet();
    // fix empty strings from split
    checkedSitesSet.remove("");
    settingSitesSet.remove("");

    const int initialSitesCount = ui->cbSites->count();

    if (!settingSitesSet.empty() && !strategiesNames.empty())
    {
        ui->cbSites->addSection(sectionIcon, sectionName);

        Q_FOREACH (const QString& strategyName, strategiesNames)
        {
            if (settingSitesSet.contains(strategyName))
            {
                QIcon icon = QIcon(QString(":/sites/") + strategyName + "-logo");
                ui->cbSites->addSite(icon, strategyName,
                                     strategyName);  // For now strategy name match combobox display text
            }
        }
    }

    bool result = false;
    for (int i = initialSitesCount; i < ui->cbSites->count(); ++i)
    {
        if (checkedSitesSet.contains(ui->cbSites->itemText(i)))
        {
            result = true;
            ui->cbSites->toggleCheckState(i, true);
        }
    }

    return result;
}

void SearchResultForm::populateSitesCombo(bool autoCheckEmpty)
{
    ui->cbSites->clear();
    std::array<QStringList, 2> strategiesNames = SearchManager::Instance().allStrategiesNames();
    bool hasSitesChecked =
        populateSitesComboHelper(strategiesNames[0], QIcon(":/images/fvdownloader.png"), tr("All Video Sites"));
    if (autoCheckEmpty && !hasSitesChecked)
    {
        ui->cbSites->toggleCheckState(0, true);
    }
    ui->cbSites->updateToolTip();
    ui->cbSites->fitContent();
}

void SearchResultForm::onSelectionChanged(const QItemSelection& selected, const QItemSelection& /*unused*/)
{
    if (!selected.empty())
    {
        const QItemSelectionRange& range = selected.at(0);
        emit entityActivated(m_model->item(m_proxyModel->mapToSource(range.topLeft()).row()));
    }
}

void SearchResultForm::onViewDoubleClick(const QModelIndex& index) { onDownload(index.row()); }

void SearchResultForm::onActivated()
{
    if (ui->searchResultView->selectionModel()->selection().count() > 0)
    {
        QItemSelectionRange range = ui->searchResultView->selectionModel()->selection().at(0);
        emit entityActivated(m_model->item(range.topLeft().row()));
    }
    else
    {
        emit entityActivated(nullptr);
    }
}

void SearchResultForm::createActions()
{
    m_openUrl = new QAction(this);
    utilities::Tr::SetTr(m_openUrl, &QAction::setText, OPEN_IN_BROWSER);
}

void SearchResultForm::showSearchButton()
{
    ui->btnSearch->show();
    ui->btnStopSearch->hide();
}

void SearchResultForm::showStopButton()
{
    ui->btnSearch->hide();
    ui->btnStopSearch->show();
}

void SearchResultForm::doSearch(QString query, int page)
{
    int row = ui->cbSites->currentIndex();
    QString site = ui->cbSites->itemData(row).toString();
    m_lastSearchString = std::move(query);

    m_model->clear();
    emit entityActivated(nullptr);
    SearchManager::Instance().clearSearchResults();

    if (!(m_lastSearchString.isEmpty() || ui->cbSites->selectedSites().empty()))
    {
        m_searchPage = page;
        auto send = sender();
        if ((send != nullptr) && (send == ui->btnSearch || send == ui->leSearch))
        {
            showStopButton();
        }
        updatePrevNextState();
        QApplication::instance()->processEvents();
        SearchManager::Instance().search(m_lastSearchString, ui->cbSites->selectedSites(), page);
    }
    m_model->update();
}

void SearchResultForm::saveSitesList()
{
    QSettings().setValue(app_settings::CheckedSites, ui->cbSites->selectedSites().join(";"));
}

void SearchResultForm::doSearch() { doSearch(ui->leSearch->text()); }

void SearchResultForm::stopSearch()
{
    SearchManager::Instance().cancelSearch();
    showSearchButton();
    updatePrevNextState();
}

void SearchResultForm::onSearchFinished()
{
    showSearchButton();
    updatePrevNextState();
}

void SearchResultForm::onSearchFailed(const QString& error) { qDebug() << "[SearchResultForm] " << error; }

void SearchResultForm::onDownload(int row)
{
    QItemSelectionModel* selModel = ui->searchResultView->selectionModel();
    if (selModel->selectedRows().count() > 1)
    {
        QMessageBox msgBox(QMessageBox::Question, Tr::Tr(PROJECT_FULLNAME_TRANSLATION),
                           tr("What videos would you like to download?"));
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setWindowIcon(QIcon(":/images/fvdownloader.png"));
        msgBox.addButton(tr("Download all"), QMessageBox::YesRole);
        msgBox.addButton(tr("Current one"), QMessageBox::NoRole);
        msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
        int res = msgBox.exec();
        switch (res)
        {
        case 0:
        {
            QModelIndexList indexList = selModel->selectedRows();
            Q_FOREACH (const QModelIndex& index, indexList)
            {
                requestStartDownload(index.row());
            }
        }
        break;
        case 1:
        {
            requestStartDownload(row);  // TODO: call searchManager method
        }
        break;
        }
    }
    else
    {
        requestStartDownload(row);  // TODO: call searchManager method
    }
}

void SearchResultForm::onDownloadContextMenu(int row, QPoint pos)
{
    VideoQualityDialog dialog(m_model->item(row));
    dialog.move(pos);
    dialog.exec();
}

void SearchResultForm::onSearchContextMenu(const QPoint& point)
{
    QModelIndex index = ui->searchResultView->indexAt(point);
    if (!index.isValid())
    {
        return;
    }
    QMenu menu(this);

    const RemoteVideoEntity* rve = m_model->item(m_proxyModel->mapToSource(index).row());
    const QList<DownloadEntity*> downloads = rve->allDownloadEntities();
    for (const auto& ent : downloads)
    {
        if (MainWindow::Instance()->downloadsForm()->exists(ent))
        {
            menu.addAction(ui->actionShowInDownload);
        }
        if (MainWindow::Instance()->libraryForm()->exists(ent))
        {
            menu.addAction(ui->actionShowInLibrary);
        }
    }

    if (!menu.isEmpty())
    {
        QAction* act = menu.exec(QCursor::pos());

        if (act == ui->actionShowInDownload)
        {
            MainWindow::Instance()->openDownloadsTab(rve->allDownloadEntities().first());
        }
        else if (act == ui->actionShowInLibrary)
        {
            MainWindow::Instance()->openLibraryTab(rve->allDownloadEntities().first());
        }
    }
}

QWidget* SearchResultForm::manageWidget() const { return ui->manageLabel; }

void SearchResultForm::on_btnPrevious_clicked()
{
    if (m_searchPage > 1)
    {
        --m_searchPage;
    }
    doSearch(m_lastSearchString, m_searchPage);
}

void SearchResultForm::on_btnNext_clicked() { doSearch(m_lastSearchString, ++m_searchPage); }

void SearchResultForm::updatePrevNextState()
{
    // TODO: rewrite
    if ((m_searchPage > 1) && (m_model->rowCount() == 0))
    {
        ui->btnNext->setEnabled(false);
        ui->btnPrevious->setEnabled(true);
    }
    else if (m_model->rowCount() == 0)
    {
        ui->btnNext->setEnabled(false);
        ui->btnPrevious->setEnabled(false);
    }
    else if (m_searchPage == 1)
    {
        ui->btnNext->setEnabled(true);
        ui->btnPrevious->setEnabled(false);
    }
    else
    {
        ui->btnNext->setEnabled(true);
        ui->btnPrevious->setEnabled(true);
    }
}

void SearchResultForm::onPreferencesChanged()
{
    populateSitesCombo(false);
    m_searchPage = 0;
}

bool SearchResultForm::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->searchResultView && event->type() == QEvent::KeyRelease)
    {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_C && keyEvent->modifiers() == Qt::ControlModifier)
        {
            copySelectionToClipboard();
        }
    }

    return QFrame::eventFilter(obj, event);
}

void SearchResultForm::copySelectionToClipboard()
{
    QModelIndexList indexes = ui->searchResultView->selectionModel()->selectedIndexes();
    QMimeData* mime = m_proxyModel->mimeData(indexes);
    QClipboard* clipb = qApp->clipboard();
    auto* md = new QMimeData();
    md->setHtml(mime->html());
    md->setText(mime->text());
    md->setUrls(mime->urls());
    clipb->setMimeData(md);
}

void SearchResultForm::onVerticalScrollChanged(int /*unused*/)
{
    if (ui->searchResultView->underMouse())
    {
        QToolTip::hideText();
    }
}

void SearchResultForm::requestStartDownload(int row) { m_model->item(row)->requestStartDownload(); }
