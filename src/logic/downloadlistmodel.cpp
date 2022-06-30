#include "downloadlistmodel.h"

#include <QDebug>
#include <QPixmap>

#include "downloadentity.h"
#include "globals.h"
#include "mainwindow.h"
#include "searchmanager.h"

DownloadListModel::DownloadListModel(QObject* parent) : QAbstractListModel(parent), m_reportSM(true)
{
    qRegisterMetaType<QMap<int, LinkInfo>>("QMap<int,LinkInfo>");

    VERIFY(connect(&SearchManager::Instance(), SIGNAL(notifyAddItemsInModel(const QList<DownloadEntity*>&, QObject*)),
                   SLOT(onAddItemsInModel(const QList<DownloadEntity*>&, QObject*))));
    VERIFY(connect(&SearchManager::Instance(),
                   SIGNAL(notifyRemoveItemsFromModel(const QList<DownloadEntity*>&, QObject*)),
                   SLOT(onRemoveItemsFromModel(const QList<DownloadEntity*>&, QObject*))));

    VERIFY(connect(this, SIGNAL(notifyRemoveItemsFromModel(const QList<DownloadEntity*>&)), &SearchManager::Instance(),
                   SLOT(onItemsRemovedNotify(const QList<DownloadEntity*>&))));
    VERIFY(connect(this, SIGNAL(notifyFinishedDownloading(const QList<DownloadEntity*>&)), &SearchManager::Instance(),
                   SLOT(onItemsExistNotify(const QList<DownloadEntity*>&))));

    VERIFY(connect(this, SIGNAL(notifyRemoveItemsFromModel(const QList<DownloadEntity*>&, QObject*)),
                   &SearchManager::Instance(),
                   SIGNAL(notifyRemoveItemsFromModel(const QList<DownloadEntity*>&, QObject*))));

    mapSignalToHeader("progressChanged(qint64, qint64)", DL_Progress);
    mapSignalToHeader("speed(qint64)", DL_Speed);
}

DownloadListModel::~DownloadListModel() = default;

int DownloadListModel::rowCount(const QModelIndex& /*parent*/) const { return numEntities(); }

int DownloadListModel::columnCount(const QModelIndex& /*parent*/) const { return DL_LastColumn; }

QVariant DownloadListModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::DisplayRole)
    {
        DownloadEntity* downloadEntity = item(index.row());
        switch (index.column())
        {
        case DL_Index:
            return {index.row() + 1};
        case DL_Title:
            return downloadEntity->videoTitle();
        case DL_Length:
            return utilities::secondsToString(downloadEntity->videoDuration());
        case DL_Progress:
            return downloadEntity->progress();
        case DL_Speed:
            return downloadEntity->speedAsString();
        case DL_Size:
            return downloadEntity->totalFileSizeAsString();
        case DL_Status:
            if (downloadEntity->directUrl().isEmpty())
            {
                return Tr::Tr(TREEVIEW_PPEPARING_STATUS);
            }
            else
            {
                return DownloadEntity::stateToString(downloadEntity->state());
            }
        default:
            return {};
        }
    }
    else if (role == Qt::EditRole)
    {
        DownloadEntity* downloadEntity = item(index.row());
        switch (index.column())
        {
        case DL_Index:
            return {index.row() + 1};
        case DL_Icon:
            return downloadEntity->strategyName();
        case DL_Title:
            return downloadEntity->videoTitle();
        case DL_Length:
            return downloadEntity->videoDuration();
        case DL_Progress:
            return downloadEntity->progress();
        case DL_Speed:
            return downloadEntity->speed();
        case DL_Size:
            return downloadEntity->totalFileSize();
        case DL_Status:
            return (int)downloadEntity->state();
        default:
            return {};
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        switch (index.column())
        {
        case DL_Index:
        case DL_Icon:
        {
            DownloadEntity* de = item(index.row());
            Q_ASSERT(de);
            return de->strategyName();
        }
        default:
            return {};
        };
    }
    else if (role == Qt::SizeHintRole && index.column() == DL_Icon)
    {
        return QSize(32, 32);
    }
    else if (role == Qt::DecorationRole && index.column() == DL_Icon)
    {
        QString iconName = item(index.row())->strategyName();
        QPixmap image(QString(":/sites/") + iconName + "-logo");
        if (!image.isNull())
        {
            return QVariant(image.scaled(30, 30));
        }
    }
    else if (role == Qt::UserRole)
    {
        DownloadEntity* downloadEntity = item(index.row());
        switch (index.column())
        {
        case DL_Index:
            return downloadEntity->getParent()->originalUrl();
        default:
            return {};
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (index.column())
        {
        case DL_Index:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return {};
        }
    }
    return {};
}

QVariant DownloadListModel::headerData(int section, Qt::Orientation /*orientation*/, int role) const
{
    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
        case DL_Index:
            return {"#"};
        case DL_Title:
            return utilities::Tr::Tr(TREEVIEW_TITLE_HEADER);
        case DL_Length:
            return utilities::Tr::Tr(TREEVIEW_LENGTH_HEADER);
        case DL_Status:
            return utilities::Tr::Tr(DOWNLOAD_TREEVIEW_STATUS_HEADER);
        case DL_Size:
            return utilities::Tr::Tr(TREEVIEW_SIZE_HEADER);
        case DL_Speed:
            return utilities::Tr::Tr(TREEVIEW_SPEED_HEADER);
        case DL_Progress:
            return utilities::Tr::Tr(TREEVIEW_PROGRESS_HEADER);
        }
    }
    return {};
}

void DownloadListModel::rowUpdated(int row, DownloadListHeaders header)
{
    const int rows = rowCount();
    if (row >= 0 && row < rows)
    {
        // this implementation is optimized for Qt4
        if (header != DL_All)
        {
            QModelIndex ind = index(row, header);
            emit dataChanged(ind, ind);
        }
        else
        {
            emit dataChanged(index(row, 0), index(row, DL_LastColumn - 1));
        }
    }
    else
    {
        qWarning() << __FUNCTION__ << " -- row " << row << " is out of bounds; rowCount()==" << rows;
    }
}

void DownloadListModel::onAppendEntities(const QList<DownloadEntity*>& list)
{
    if (!list.empty())
    {
        beginInsertRows(QModelIndex(), numEntities(), numEntities() + list.size() - 1);
        addEntities_impl(list);
        Q_FOREACH (DownloadEntity* de, list)
        {
            // do this in S.M.	if (de->visibilityState() != visTemp)
            VERIFY(connect(de, SIGNAL(progressChanged(qint64, qint64)), SLOT(updateEntityRow())));
            VERIFY(connect(de, SIGNAL(speed(qint64)), SLOT(updateEntityRow())));
            VERIFY(connect(de, SIGNAL(stateChanged(Downloadable::State, Downloadable::State)),
                           SLOT(entityStateChanged(Downloadable::State, Downloadable::State))));
            VERIFY(connect(de->getParent(), SIGNAL(linksExtracted()), SLOT(onLinkExtracted())));
        }
        endInsertRows();

        DownloadManager::considerStartNextDownload();
    }
}

void DownloadListModel::clear() { removeRows(0, rowCount()); }

void DownloadListModel::updateEntityRow()
{
    auto it = m_mapSignalsToHeaders.find(senderSignalIndex());
    DownloadListHeaders header = (it != m_mapSignalsToHeaders.end() ? it.value() : DL_All);
    auto* entity = qobject_cast<DownloadEntity*>(sender());
    if (entity != nullptr)
    {
        int row = entityRow(entity);
        rowUpdated(row, header);
    }
}

void DownloadListModel::entityStateChanged(Downloadable::State newState, Downloadable::State oldState)
{
    auto* dle = qobject_cast<DownloadEntity*>(sender());
    Q_ASSERT(dle);
    DownloadManager::entityStateChanged(dle, newState, oldState);

    if (newState == Downloadable::kFinished)
    {
        emit notifyFinishedDownloading(QList<DownloadEntity*>() << dle);
    }

    int row = entityRow(dle);
    rowUpdated(row, DL_All);
}

bool DownloadListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    qDebug() << "remove:" << row << count;
    if (count > 0)
    {
        beginRemoveRows(parent, row, row + count - 1);
        const QList<DownloadEntity*> removedItems = DownloadManager::removeEntities_impl(row, count);
        for (const auto& ent : removedItems)
        {
            ent->disconnect(this);
        }
        endRemoveRows();

        if (isReportSM())
        {
            emit notifyRemoveItemsFromModel(removedItems);
        }

        considerStartNextDownload();
        MainWindow::Instance()->askForSavingModel();
        return true;
    }

    return false;
}

void DownloadListModel::removeRowsSet(const std::set<int>& rows)
{
    auto it = rows.rbegin();
    while (it != rows.rend())
    {
        removeRow(*it);  // calls removeRows
        ++it;
    }
}

void DownloadListModel::onLinkExtracted()
{
    DownloadManager::considerStartNextDownload();
    MainWindow::Instance()->askForSavingModel();
}

void DownloadListModel::onAddItemsInModel(const QList<DownloadEntity*>& list, QObject* obj)
{
    if (obj == this)
    {
        return;
    }

    qDebug() << "DownloadListModel::onAddItemsInModel:";
    onAppendEntities(list);
}

void DownloadListModel::onRemoveItemsFromModel(const QList<DownloadEntity*>& list, QObject* obj)
{
    if (obj == this)
    {
        return;
    }
    m_reportSM = false;
    qDebug() << __FUNCTION__;

    Q_FOREACH (DownloadEntity* de, list)
    {
        int row = entityRow(de);
        if (-1 == row)
        {
            continue;
        }

        removeRow(row);
    }
    m_reportSM = true;
}

QObjectList DownloadListModel::entities() const
{
    QObjectList ents;
    iterateEntities(
        [&ents](DownloadEntity* const ent)
        {
            if (ent->isPersistable())
            {
                ents.append(ent);
            }
        });
    return ents;
}

void DownloadListModel::setEntities(const QObjectList& ents)
{
    QList<DownloadEntity*> deList;
    std::transform(ents.begin(), ents.end(), std::back_inserter(deList),
                   [](QObject* const o) -> DownloadEntity* { return qobject_cast<DownloadEntity*>(o); });
    deList.removeAll(nullptr);
    onAppendEntities(deList);
}

void DownloadListModel::notifyDownloadRestarted(DownloadEntity* de)
{
    emit notifyRemoveItemsFromModel(QList<DownloadEntity*>() << de, this);
}

void DownloadListModel::mapSignalToHeader(const char* signal, DownloadListHeaders header)
{
    const int index = DownloadEntity::staticMetaObject.indexOfSignal(QMetaObject::normalizedSignature(signal));
    Q_ASSERT(index >= 0);
    m_mapSignalsToHeaders[index] = header;
}
