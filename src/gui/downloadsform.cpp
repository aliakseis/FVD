#include "downloadsform.h"

#include <QClipboard>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QScrollBar>

#include "branding.hxx"
#include "downloadentity.h"
#include "downloadlistmodel.h"
#include "downloadscontrol.h"
#include "downloadsdelegate.h"
#include "downloadsortfiltermodel.h"
#include "global_functions.h"
#include "libraryform.h"
#include "mainwindow.h"
#include "playerheader.h"
#include "ui_downloadsform.h"
#include "ui_mainwindow.h"
#include "videowidget.h"

#ifdef DEVELOPER_FEATURES
#include <QMouseEvent>
#endif
#include "searchmanager.h"

static void openFolder(const QString& fileName)
{
    utilities::SelectFile(fileName, global_functions::GetVideoFolder());
}

DownloadsForm::DownloadsForm(VideoPlayerWidget* video_widget, QWidget* parent)
    : QFrame(parent), ui(new Ui::DownloadsForm), videoPlayer(video_widget), control(nullptr)
{
    ui->setupUi(this);
    ui->manageLabel->setVisible(false);
    ui->manageLabel->setImages(PreviewPanelButton::LeftArrow);

    ui->leSearch->installEventFilter(qApp);

    m_model = new DownloadListModel(ui->downloadsTreeView);
    m_proxyModel = new DownloadSortFilterModel(ui->downloadsTreeView);
    m_proxyModel->setSortRole(Qt::EditRole);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setHeader(ui->downloadsTreeView->header());
    control = new DownloadsControl(this, m_proxyModel);
    ui->downloadsTreeView->setModel(m_proxyModel);
    ui->downloadsTreeView->setSortingEnabled(true);
    ui->downloadsTreeView->header()->setSortIndicatorShown(false);
    // m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->sort(-1, Qt::AscendingOrder);
    m_proxyModel->setFilterKeyColumn(DL_Title);

    // initially hide the sort indicators
    ui->downloadsTreeView->header()->setSortIndicatorShown(false);

    auto* delegate = new DownloadsDelegate(ui->downloadsTreeView);
    ui->downloadsTreeView->setItemDelegate(delegate);
    ui->downloadsTreeView->header()->setSectionResizeMode(DL_Index, QHeaderView::Fixed);
    ui->downloadsTreeView->header()->setSectionResizeMode(DL_Icon, QHeaderView::Fixed);
    ui->downloadsTreeView->header()->setSectionResizeMode(DL_Title, QHeaderView::Stretch);
    ui->downloadsTreeView->header()->setSectionResizeMode(DL_Length, QHeaderView::Fixed);
    ui->downloadsTreeView->header()->setSectionResizeMode(DL_Size, QHeaderView::Fixed);
    ui->downloadsTreeView->header()->setSectionResizeMode(DL_Progress, QHeaderView::Fixed);
    ui->downloadsTreeView->header()->setSectionResizeMode(DL_Speed, QHeaderView::Fixed);
    ui->downloadsTreeView->header()->setSectionResizeMode(DL_Status, QHeaderView::Fixed);
    ui->downloadsTreeView->setColumnWidth(DL_Index, 28);
    ui->downloadsTreeView->setColumnWidth(DL_Icon, 34);
    ui->downloadsTreeView->setColumnWidth(DL_Title, 233);
    ui->downloadsTreeView->setColumnWidth(DL_Length, 65);
    ui->downloadsTreeView->setColumnWidth(DL_Size, 65);
    ui->downloadsTreeView->setColumnWidth(DL_Progress, 150);
    ui->downloadsTreeView->setColumnWidth(DL_Speed, 70);
    ui->downloadsTreeView->setColumnWidth(DL_Status, 130);

    ui->downloadsTreeView->viewport()->installEventFilter(this);
    ui->downloadsTreeView->installEventFilter(this);

    VERIFY(connect(ui->leSearch, SIGNAL(returnPressed()), SLOT(onSearch())));
    VERIFY(connect(ui->btnSearch, SIGNAL(clicked()), SLOT(onSearch())));
    VERIFY(connect(ui->btnPasteURLs, SIGNAL(clicked()), SLOT(onPasteURLs())));
    ui->btnPasteURLs->setShortcut(QKeySequence::Paste);

    VERIFY(connect(ui->downloadsTreeView, SIGNAL(customContextMenuRequested(QPoint)),
                   SLOT(onDownloadsContextMenu(QPoint))));
    VERIFY(
        connect(ui->downloadsTreeView, SIGNAL(doubleClicked(QModelIndex)), SLOT(onDownloadsDoubleClick(QModelIndex))));
    VERIFY(connect(ui->downloadsTreeView->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
                   SLOT(onSelectionChanged(QItemSelection, QItemSelection))));
    VERIFY(connect(ui->btnClear, SIGNAL(clicked()), SLOT(clearDownloadsList())));
    VERIFY(connect(control, SIGNAL(up()), SLOT(onItemUp())));
    VERIFY(connect(control, SIGNAL(down()), SLOT(onItemDown())));
    VERIFY(connect(control, SIGNAL(start()), SLOT(onStartItem())));
    VERIFY(connect(control, SIGNAL(stop()), SLOT(onStopItem())));
    VERIFY(connect(control, SIGNAL(reload()), SLOT(onReloadItem())));
    VERIFY(connect(control, SIGNAL(remove()), SLOT(onRemoveItem())));
    VERIFY(connect(control, SIGNAL(pause()), SLOT(onPauseItem())));
    VERIFY(connect(control, SIGNAL(setSpeedLimit(int)), SLOT(onSetSpeedLimit(int))));

    VERIFY(connect(ui->downloadsTreeView->verticalScrollBar(), SIGNAL(valueChanged(int)),
                   SLOT(onVerticalScrollChanged(int))));

    utilities::Tr::MakeRetranslatable(this, ui);
}

DownloadsForm::~DownloadsForm()
{
    delete ui;
    delete control;
}

void DownloadsForm::onDownloadsContextMenu(const QPoint& point)
{
    hideFloatingControl();

    bool selection = (ui->downloadsTreeView->selectionModel()->selectedRows().count() > 1);

    QMenu menu(this);

    if (selection)
    {
        QItemSelectionModel* selectionModel = ui->downloadsTreeView->selectionModel();
        QModelIndexList indexList = selectionModel->selectedRows();

        menu.addAction(ui->actionPauseSelected);
        menu.addAction(ui->actionResumeSelected);
        menu.addAction(ui->actionStopSelected);
        menu.addAction(ui->actionClearList);

        QAction* act = menu.exec(QCursor::pos());
        if (act == ui->actionPauseSelected)
        {
            Q_FOREACH (const QModelIndex& index, indexList)
            {
                QModelIndex srcIdx = m_proxyModel->mapToSource(index);
                DownloadEntity* entity = m_model->item(srcIdx.row());
                if (entity->state() == DownloadEntity::kDownloading)
                {
                    DownloadListModel::pauseDownload(m_model->item(srcIdx.row()));
                }
            }
        }
        else if (act == ui->actionResumeSelected)
        {
            Q_FOREACH (const QModelIndex& index, indexList)
            {
                QModelIndex srcIdx = m_proxyModel->mapToSource(index);
                DownloadEntity* entity = m_model->item(srcIdx.row());
                if (entity->state() == DownloadEntity::kPaused)
                {
                    m_model->resumeDownload(m_model->item(srcIdx.row()));
                }
                else if (entity->state() == DownloadEntity::kQueued)
                {
                    m_model->restartDownload(m_model->item(srcIdx.row()));
                }
            }
        }
        else if (act == ui->actionStopSelected)
        {
            Q_FOREACH (const QModelIndex& index, indexList)
            {
                QModelIndex srcIdx = m_proxyModel->mapToSource(index);
                DownloadEntity* entity = m_model->item(srcIdx.row());
                if (entity->state() != DownloadEntity::kFinished)
                {
                    DownloadListModel::stopDownload(m_model->item(srcIdx.row()));
                }
            }
        }
        else if (act == ui->actionClearList)
        {
            deleteItems(ui->downloadsTreeView->selectionModel()->selectedRows(),
                        qApp->keyboardModifiers() & Qt::ShiftModifier);
        }
    }
    else
    {
        QModelIndex index = ui->downloadsTreeView->indexAt(point);
        if (!index.isValid())
        {
            return;
        }

        DownloadEntity* entity = m_model->item(m_proxyModel->mappedRow(index));

        if (entity == nullptr)
        {
            return;
        }

        menu.addAction(ui->actionOpenFile);
        menu.addAction(ui->actionOpenFolder);
        if (MainWindow::Instance()->libraryForm()->exists(entity))
        {
            menu.addAction(ui->actionShowInLibrary);
        }
        menu.addAction(ui->actionClearList);

        QAction* act = menu.exec(QCursor::pos());

        if (act == ui->actionOpenFile)
        {
            VideoPlayerWidgetInstance()->playPauseButtonAction();
        }
        else if (act == ui->actionOpenFolder)
        {
            openFolder(entity->filename());
        }
        else if (act == ui->actionShowInLibrary)
        {
            MainWindow::Instance()->openLibraryTab(entity);
        }
        else if (act == ui->actionClearList)
        {
            bool deleteCompletely = (qApp->keyboardModifiers() & Qt::ShiftModifier) != 0;  // if shift pushed
            deleteItems(QModelIndexList() << index, deleteCompletely);
        }
    }
}

void DownloadsForm::onDownloadsDoubleClick(const QModelIndex& index)
{
    DownloadEntity* entity = m_model->item(m_proxyModel->mappedRow(index));
    Q_ASSERT(entity);
    openFolder(entity->filename());
}

void DownloadsForm::onSelectionChanged(const QItemSelection& selected, const QItemSelection& /*unused*/)
{
    if (!selected.empty())
    {
        const QItemSelectionRange& range = selected.at(0);
        emit entityActivated(m_model->item(m_proxyModel->mappedRow(range.topLeft())));
    }
}

void DownloadsForm::onReloadItem()
{
    hideFloatingControl();
    int mappedRow = m_proxyModel->mappedRow(control->affectedRow());
    if (fileIsInUse(videoPlayer, m_model->item(mappedRow)))
    {
        VERIFY(connect(videoPlayer, SIGNAL(fileReleased()), SLOT(restartDownload()), Qt::UniqueConnection));
        videoPlayer->stopVideo();  // this is synchronous method
    }
    else
    {
        restartDownload();
    }
}

void DownloadsForm::onRemoveItem()
{
    hideFloatingControl();
    int mappedRow = m_proxyModel->mappedRow(control->affectedRow());
    if (fileIsInUse(videoPlayer, m_model->item(mappedRow)))
    {
        VERIFY(connect(videoPlayer, SIGNAL(fileReleased()), SLOT(removeDownload()), Qt::UniqueConnection));
        videoPlayer->stopVideo();
    }
    else
    {
        removeDownload();
    }
}

void DownloadsForm::removeDownload()
{
    hideFloatingControl();
    disconnect(videoPlayer, SIGNAL(fileReleased()), this, SLOT(removeDownload()));
    int mappedRow = m_proxyModel->mappedRow(control->affectedRow());
    if (ui->downloadsTreeView->selectionModel()->isSelected(m_proxyModel->index(control->affectedRow(), 0)))
    {
        emit entityActivated(nullptr);
    }
    m_model->removeRow(mappedRow);
}

void DownloadsForm::restartDownload()
{
    hideFloatingControl();
    disconnect(videoPlayer, SIGNAL(fileReleased()), this, SLOT(restartDownload()));

    int mappedRow = m_proxyModel->mappedRow(control->affectedRow());
    auto de = m_model->item(mappedRow);
    if (de->state() != Downloadable::kFinished || DownloadEntity::confirmRestartDownload())
    {
        m_model->restartDownload(de);
    }
}

void DownloadsForm::onPauseItem()
{
    hideFloatingControl();
    int row = m_proxyModel->mappedRow(control->affectedRow());

    DownloadEntity* entity = m_model->item(row);
    Q_ASSERT(entity);
    if (entity->state() == DownloadEntity::kDownloading)
    {
        DownloadListModel::pauseDownload(m_model->item(row));
    }
    else if (entity->state() == DownloadEntity::kPaused)
    {
        m_model->resumeDownload(m_model->item(row));
    }
}

void DownloadsForm::onStartItem()
{
    hideFloatingControl();
    int row = m_proxyModel->mappedRow(control->affectedRow());

    DownloadEntity* entity = m_model->item(row);
    Q_ASSERT(entity);
    if (entity->state() == DownloadEntity::kPaused)
    {
        m_model->resumeDownload(entity);
    }
    else if (entity->state() == DownloadEntity::kQueued)
    {
        m_model->restartDownload(entity);
    }
}

void DownloadsForm::onStopItem()
{
    hideFloatingControl();
    int row = m_proxyModel->mappedRow(control->affectedRow());

    DownloadEntity* entity = m_model->item(row);
    Q_ASSERT(entity);
    DownloadListModel::stopDownload(entity);
}

void DownloadsForm::onSetSpeedLimit(int speedLimit)
{
    int row = m_proxyModel->mappedRow(control->affectedRow());

    DownloadEntity* entity = m_model->item(row);
    Q_ASSERT(entity);

    DownloadListModel::setSpeedLimit(entity, speedLimit);
}

void DownloadsForm::onActivated(const DownloadEntity* selEntity)
{
    ui->leSearch->clear();
    onSearch();

    if (selEntity != nullptr)
    {
        int row = m_model->entityRow(selEntity);

        QItemSelection selection;
        int proxyIndex = m_proxyModel->mapFromSource(m_model->index(row, 0)).row();
        selection << QItemSelectionRange(m_proxyModel->index(proxyIndex, 0),
                                         m_proxyModel->index(proxyIndex, DL_LastColumn - 1));

        ui->downloadsTreeView->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);

        emit entityActivated(selEntity);
    }
    else
    {
        if (ui->downloadsTreeView->selectionModel()->selection().count() > 0)
        {
            QItemSelectionRange range = ui->downloadsTreeView->selectionModel()->selection().at(0);
            emit entityActivated(m_model->item(range.topLeft().row()));
        }
        else
        {
            emit entityActivated(nullptr);
        }
    }
}

void DownloadsForm::onPreferencesChanged() { m_model->considerStartNextDownload(); }

void DownloadsForm::deleteItems(QModelIndexList const& indexList, bool deleteCompletely)
{
    hideFloatingControl();

    if (indexList.empty())
    {
        return;
    }

    QString warningText = (deleteCompletely ? tr("Are you sure want to remove %1 item(s)?").arg(indexList.count()) +
                                                  "\n" + tr("All downloaded files will be removed.")
                                            : tr("Are you sure want to remove %1 item(s)?").arg(indexList.count()));

    if (QMessageBox::Yes == QMessageBox::question(this, tr("Warning"), warningText, QMessageBox::Yes, QMessageBox::No))
    {
        // determine entities to be removed
        QList<DownloadEntity*> itemsToDelete;
        std::transform(indexList.constBegin(), indexList.constEnd(), std::back_inserter(itemsToDelete),
                       [this](const QModelIndex& index) -> DownloadEntity*
                       { return m_model->item(m_proxyModel->mapToSource(index).row()); });
        // find currently played entity in candidates to delete
        auto played = std::find_if(itemsToDelete.constBegin(), itemsToDelete.constEnd(),
                                   [capture0 = VideoPlayerWidgetInstance()](auto&& PH1)
                                   { return fileIsInUse(capture0, std::forward<decltype(PH1)>(PH1)); });
        // if currently played entity to be deleted, stop playing
        if (played != itemsToDelete.constEnd() && (deleteCompletely || (*played)->state() != Downloadable::kFinished))
        {
            VideoPlayerWidgetInstance()->stopVideo(true);
        }

        if (deleteCompletely)
        {
            // completely delete entities
            SearchManager::Instance().onItemsDeletedNotify(itemsToDelete);
        }
        else
        {
            // remove entities from downloads list, completed files will continue exist
            std::set<int> rows;
            std::transform(indexList.constBegin(), indexList.constEnd(), std::inserter(rows, rows.begin()),
                           [this](const QModelIndex& index) -> int { return m_proxyModel->mapToSource(index).row(); });
            m_model->removeRowsSet(rows);
        }

        // deselect items
        emit entityActivated(nullptr);
    }
}

void DownloadsForm::hideEvent(QHideEvent* event)
{
    hideFloatingControl();
    QFrame::hideEvent(event);
}

bool DownloadsForm::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->downloadsTreeView->viewport())
    {
        if (event->type() == QEvent::ToolTip
#ifdef DEVELOPER_FEATURES
            || (event->type() == QEvent::MouseButtonRelease &&
                static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)
#endif
        )
        {
            QPoint point = ui->downloadsTreeView->viewport()->mapFromGlobal(QCursor::pos());
            QModelIndex index = ui->downloadsTreeView->indexAt(point);
            if (index.isValid() && index.column() > 1)
            {
                DownloadEntity* entity = m_model->item(m_proxyModel->mappedRow(index));
                Q_ASSERT(entity);

                int proxyRow = index.row();  // m_proxyModel->mapFromSource(m_model->index(index.row() , 0)).row();
                auto is_edownload = entity->state() != DownloadEntity::kFinished;
                control->setState((entity->state() == DownloadEntity::kDownloading) ? DownloadsControl::Started
                                                                                    : DownloadsControl::Paused,
                                  (proxyRow > 0), (proxyRow < (m_proxyModel->rowCount() - 1)),
                                  DownloadListModel::canStopDownload(entity), is_edownload, entity->speedLimit());
                control->showAtCursor(index.row());
            }
        }
    }
    else if (obj == ui->downloadsTreeView && event->type() == QEvent::KeyRelease)
    {
        auto* keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Delete)
        {
            deleteItems(ui->downloadsTreeView->selectionModel()->selectedRows(),
                        qApp->keyboardModifiers() & Qt::ShiftModifier);
        }
        else if (keyEvent->key() == Qt::Key_Space)
        {
            VideoPlayerWidgetInstance()->playPauseButtonAction();
        }
        else if (keyEvent->key() == Qt::Key_Up && keyEvent->modifiers() == Qt::ControlModifier)
        {
            moveUpSelectedItems();
        }
        else if (keyEvent->key() == Qt::Key_Down && keyEvent->modifiers() == Qt::ControlModifier)
        {
            moveDownSelectedItems();
        }
        else if (keyEvent->key() == Qt::Key_C && keyEvent->modifiers() == Qt::ControlModifier)
        {
            copySelectionToClipboard();
        }
    }

    return QFrame::eventFilter(obj, event);
}

QWidget* DownloadsForm::manageWidget() const { return ui->manageLabel; }

void DownloadsForm::clearDownloadsList(bool silent /* =false*/)
{
    if (m_model->rowCount() > 0 &&
        (silent || QMessageBox::question(nullptr, utilities::Tr::Tr(PROJECT_FULLNAME_TRANSLATION),
                                         tr("<b>Are you sure want to clear the download list?</b>"),
                                         QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes))
    {
        m_model->clear();
        emit entityActivated(nullptr);
    }
}

void DownloadsForm::onSearch() { m_proxyModel->setFilterFixedString(ui->leSearch->text()); }

void DownloadsForm::onItemUp()
{
    m_proxyModel->moveItemUp(control->affectedRow());
    hideFloatingControl();
}

void DownloadsForm::onItemDown()
{
    m_proxyModel->moveItemDown(control->affectedRow());
    hideFloatingControl();
}

QObject* DownloadsForm::model() { return m_model; }

void DownloadsForm::hideFloatingControl()
{
    if (!control->isActiveWindow() || sender() == control)
    {
        control->QWidget::hide();
    }
}

bool DownloadsForm::exists(const DownloadEntity* entity)
{
    Q_ASSERT(m_model);
    if (entity != nullptr)
    {
        return (m_model->entityRow(entity) >= 0);
    }
    return false;
}

void DownloadsForm::moveUpSelectedItems() { m_proxyModel->moveItemsUp(ui->downloadsTreeView->selectionModel()); }

void DownloadsForm::moveDownSelectedItems() { m_proxyModel->moveItemsDown(ui->downloadsTreeView->selectionModel()); }

void DownloadsForm::copySelectionToClipboard()
{
    QModelIndexList indexes = ui->downloadsTreeView->selectionModel()->selectedIndexes();
    QMimeData* mime = m_proxyModel->mimeData(indexes);
    QClipboard* clipb = qApp->clipboard();
    auto* md = new QMimeData();
    md->setHtml(mime->html());
    md->setText(mime->text());
    md->setUrls(mime->urls());
    clipb->setMimeData(md);
}

void DownloadsForm::onVerticalScrollChanged(int /*unused*/) { hideFloatingControl(); }

void DownloadsForm::onPasteURLs()
{
    if (auto clipboard = QApplication::clipboard())
    {
        if (auto mimeData = clipboard->mimeData())
        {
            SearchManager::Instance().addLinks(*mimeData);
        }
    }
}
