#include "librarymodel.h"

#include <QDebug>
#include <QFileInfo>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>
#include <algorithm>
#include <utility>

#include "downloadentity.h"
#include "imagecache.h"
#include "libraryaddworker.h"
#include "libraryremoveworker.h"
#include "mainwindow.h"
#include "searchmanager.h"
#include "settings_declaration.h"
#include "utilities/utils.h"
#include "videoplayerwidget.h"

namespace {

int countFiles(const QString& dirPath)
{
    QDir dir(dirPath);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    int fileCount = dir.count();

    QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& subDir : subDirs)
    {
        fileCount += countFiles(subDir.filePath());
    }

    return fileCount;
}

}

const int LibraryUpdateTimerDelay = 5000;

LibraryModel::LibraryModel(QObject* parent)
    : QAbstractListModel(parent),
      m_reportSM(true),
      m_isLibraryAddWorkerRunning(false),
      m_isLibraryRemoveWorkerRunning(false),
      m_videoDirectoryChanged(false)  // do the first sychronization
{
    qRegisterMetaType<QPointer<DownloadEntity> >("QPointer<DownloadEntity>");

    VERIFY(connect(&SearchManager::Instance(), SIGNAL(notifyAddItemsInModel(const QList<DownloadEntity*>&, QObject*)),
                   SLOT(onAddItemsInModel(const QList<DownloadEntity*>&, QObject*))));
    VERIFY(connect(&SearchManager::Instance(), SIGNAL(notifyLibraryAddItems(const QList<DownloadEntity*>&, QObject*)),
                   SLOT(onAddItemsInModel(const QList<DownloadEntity*>&, QObject*))));
    VERIFY(connect(&SearchManager::Instance(),
                   SIGNAL(notifyRemoveItemsFromModel(const QList<DownloadEntity*>&, QObject*)),
                   SLOT(onRemoveItemsFromModel(const QList<DownloadEntity*>&, QObject*))));

    VERIFY(connect(this, SIGNAL(notifyRemoveItemsFromModel(const QList<DownloadEntity*>&)), &SearchManager::Instance(),
                   SLOT(onItemsDeletedNotify(const QList<DownloadEntity*>&))));

    VERIFY(connect(&FileMissingSignaller::Instance(), SIGNAL(fileMissing(const QString&)),
                   SLOT(onLibraryFileMissing(const QString&))));

    update();
}

void LibraryModel::update()
{
    beginResetModel();
    endResetModel();
}

int LibraryModel::rowCount(const QModelIndex& /*parent*/) const { return numEntities(); }

QVariant LibraryModel::data(const QModelIndex& index, int role) const
{
    DownloadEntity* entity = item(index.row());
    switch (role)
    {
    case Qt::DisplayRole:
        return entity->videoTitle();
    case RoleThumbnail:
        return entity->getParent()->thumbnailUrl().isEmpty()? entity->filename() : entity->getParent()->thumbnailUrl();
    case RoleTitle:
        return entity->videoTitle();
    case RoleDate:
        return entity->published().date().toString(Qt::SystemLocaleShortDate);
    case RoleSize:
        if (auto size = entity->totalFileSize())
            return utilities::SizeToString(size, 1, 1);
        else
            return tr("%1 file(s)").arg(countFiles(entity->filename()));
    case RoleEntity:
        return qVariantFromValue(entity);
    case RoleTimeDownload:
        return entity->fileCreated();
    case RoleFileName:
        return entity->filename();
    case RoleFilter:
        if (m_showMode == (entity->totalFileSize()? ShowOnlyFolders : ShowOnlyFiles))
            return {};
        return utilities::replaceBoldItalicSymbols(entity->videoTitle());
    }
    return {};
}

bool LibraryModel::removeRows(int row, int count, const QModelIndex& parent)
{
    int rowCount = this->rowCount(parent);
    if (row < rowCount && count > 0)
    {
        int lastRow = row + count - 1;
        beginRemoveRows(parent, row, lastRow);
        QList<DownloadEntity*> removedItems = removeEntities_impl(row, count);
        endRemoveRows();

        if (isReportSM())
        {
            emit notifyRemoveItemsFromModel(removedItems);
        }

        MainWindow::Instance()->askForSavingModel();
        return true;
    }
    return false;
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[RoleThumbnail] = "thumb";
    roles[RoleTitle] = "title";
    roles[RoleDate] = "fileCreated";
    roles[RoleSize] = "fileSize";
    roles[RoleTimeDownload] = "fileTimeDownload";
    roles[RoleFileName] = "fileName";
    return roles;
}

void LibraryModel::onAppendEntities(const QList<DownloadEntity*>& entitisList)
{
    QList<DownloadEntity*> list(entitisList);
    auto newEnd = std::remove_if(list.begin(), list.end(),
                                 [this](DownloadEntity* entity)
                                 { return entity->state() != Downloadable::kFinished || -1 != entityRow(entity); });
    list.erase(newEnd, list.end());

    if (!list.empty())
    {
        beginInsertRows(QModelIndex(), 0, list.size() - 1);

        prependEntities_impl(list);
        endInsertRows();

        Q_FOREACH (DownloadEntity* ent, list)
        {
            VERIFY(connect(ent, SIGNAL(stateChanged(Downloadable::State, Downloadable::State)),
                           SLOT(entityStateChanged(Downloadable::State, Downloadable::State))));
        }
        MainWindow::Instance()->askForSavingModel();
    }
}

void LibraryModel::clear()
{
    beginResetModel();
    clear_impl();
    endResetModel();
}

QObjectList LibraryModel::entities() const
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

void LibraryModel::setEntities(const QObjectList& ents)
{
    QList<DownloadEntity*> deList;
    std::transform(ents.begin(), ents.end(), std::back_inserter(deList),
                   [](QObject* const o) -> DownloadEntity* { return qobject_cast<DownloadEntity*>(o); });
    deList.removeAll(nullptr);

    onAppendEntities(deList);
}

void LibraryModel::onAddItemsInModel(const QList<DownloadEntity*>& list, QObject* obj)
{
    if (obj == this)
    {
        return;
    }

    qDebug() << __FUNCTION__;
    onAppendEntities(list);
}

void LibraryModel::onRemoveItemsFromModel(const QList<DownloadEntity*>& list, QObject* obj)
{
    if (obj == this || list.isEmpty())
    {
        return;
    }
    qDebug() << __FUNCTION__;

    if (list.size() == 1)
    {
        removeRowDontReportSM(list[0]);
    }
    else
    {
        beginResetModel();
        removeEntities_impl(list);
        endResetModel();
    }
}

void LibraryModel::synchronize(bool mandatory)
{
    if (!(m_videoDirectoryChanged || mandatory))
    {
        return;
    }

    if (m_isLibraryAddWorkerRunning || m_isLibraryRemoveWorkerRunning)
    {
        // grab the files that could have been updated since workers had started
        m_videoDirectoryChanged = true;  // force update
        QTimer::singleShot(LibraryUpdateTimerDelay, this, SLOT(onLibraryUpdateTimer()));
    }

    m_videoDirectoryChanged = false;

    if (!m_isLibraryAddWorkerRunning)
    {
        m_isLibraryAddWorkerRunning = true;
        EntityFileNames allEntitiesFiles(m_allEntitiesFiles);
        QObjectList allEntities = SearchManager::Instance().getEntities();
        Q_FOREACH (QObject* o, allEntities)
        {
            if (auto* entity = dynamic_cast<RemoteVideoEntity*>(o))
            {
                QList<DownloadEntity*> dleList = entity->allDownloadEntities();
                Q_FOREACH (DownloadEntity* entity, dleList)
                {
                    allEntitiesFiles.insert(entity->filename());
                }
            }
        }

        auto* libraryAddWorker = new LibraryAddWorker(std::move(allEntitiesFiles));
        connect(libraryAddWorker, &LibraryAddWorker::libraryFileAdded, this, &LibraryModel::onLibraryFileAdded);
        connect(libraryAddWorker, &LibraryAddWorker::destroyed, this, &LibraryModel::onLibraryAddWorkerDestroyed);
        QThreadPool::globalInstance()->start(libraryAddWorker);
    }

    if (!m_isLibraryRemoveWorkerRunning)
    {
        m_isLibraryRemoveWorkerRunning = true;

        QMap<QString, QPointer<DownloadEntity> > libraryEntities;
        iterateEntities([&libraryEntities](DownloadEntity* const entity)
                        { libraryEntities[entity->filename()] = QPointer<DownloadEntity>(entity); });

        auto* libraryRemoveWorker = new LibraryRemoveWorker(libraryEntities);
        connect(libraryRemoveWorker, &LibraryRemoveWorker::libraryFileDeleted, this,
                &LibraryModel::onLibraryFileDeleted);
        connect(libraryRemoveWorker, &LibraryRemoveWorker::destroyed, this,
                &LibraryModel::onLibraryRemoveWorkerDestroyed);
        QThreadPool::globalInstance()->start(libraryRemoveWorker);
    }
}

void LibraryModel::onVideoDirectoryChanged()
{
    if (!m_videoDirectoryChanged)
    {
        QTimer::singleShot(LibraryUpdateTimerDelay, this, SLOT(onLibraryUpdateTimer()));
    }
    m_videoDirectoryChanged = true;
}

void LibraryModel::onLibraryUpdateTimer() { synchronize(false); }

void LibraryModel::onLibraryAddWorkerDestroyed() { m_isLibraryAddWorkerRunning = false; }

void LibraryModel::onLibraryRemoveWorkerDestroyed() { m_isLibraryRemoveWorkerRunning = false; }

void LibraryModel::onLibraryFileAdded(const QString& filePath)
{
    qDebug() << __FUNCTION__ << " filePath: " << filePath;

    if (auto videoPlayerWidgetInstance = VideoPlayerWidgetInstance())
    {
        if (QFileInfo(videoPlayerWidgetInstance->currentFilename()) == QFileInfo(filePath))
        {
            return;
        }
    }

    const bool hadWorkToDoOnIdle = hasWorkToDoOnIdle();
    if (m_allEntitiesFiles.insert(filePath).second && !hadWorkToDoOnIdle)
    {
        scheduleOnIdle();
    }
}

void LibraryModel::onLibraryFileDeleted(const QPointer<DownloadEntity>& entity)
{
    if (entity.isNull())
    {
        return;
    }

    qDebug() << __FUNCTION__ << " fileName: " << entity->filename();
    const bool hadWorkToDoOnIdle = hasWorkToDoOnIdle();
    m_missingFiles.push_back(entity);
    if (!hadWorkToDoOnIdle)
    {
        scheduleOnIdle();
    }
}

void LibraryModel::onLibraryFileMissing(const QString& filePath)
{
    qDebug() << __FUNCTION__ << "filePath:" << filePath;

    QList<DownloadEntity*> entities;
    iterateEntities(
        [&entities, filePath](DownloadEntity* const entity)
        {
            if (entity->filename() == filePath)
            {
                entities.push_back(entity);
            }
        });

    SearchManager::Instance().onItemsDeletedNotify(entities);
}

void LibraryModel::entityStateChanged(Downloadable::State newState, Downloadable::State oldState)
{
    Q_UNUSED(oldState);
    auto* ent = qobject_cast<DownloadEntity*>(sender());
    Q_ASSERT(ent);

    VERIFY(disconnect(ent, SIGNAL(stateChanged(Downloadable::State, Downloadable::State)), this,
                      SLOT(entityStateChanged(Downloadable::State, Downloadable::State))));

    // take into account that order of notifications can be altered
    if (newState != Downloadable::kFinished)
    {
        removeRowDontReportSM(ent);
    }
}

void LibraryModel::removeRowDontReportSM(DownloadEntity* ent)
{
    m_reportSM = false;

    int row = entityRow(ent);
    if (-1 != row)
    {
        removeRow(row);
    }

    m_reportSM = true;
}

void LibraryModel::onIdle()
{
    EntityFileNames allEntitiesFiles;
    allEntitiesFiles.swap(m_allEntitiesFiles);
    QList<QPointer<DownloadEntity> > missingFiles;
    missingFiles.swap(m_missingFiles);

    if (!allEntitiesFiles.empty())
    {
        SearchManager::Instance().onLibraryFilesAdded(allEntitiesFiles);
    }

    QList<DownloadEntity*> validEntities;
    Q_FOREACH (const QPointer<DownloadEntity>& missingEntity, missingFiles)
    {
        if (!missingEntity.isNull())
        {
            DownloadEntity* ent = missingEntity.data();
            if ((ent->visibilityState() == visLibOnly) ||
                (ent->visibilityState() == visNorm && ent->state() == Downloadable::kFinished))
            {
                qDebug() << __FUNCTION__ << " fileName: " << ent->filename();
                validEntities.push_back(ent);
            }
        }
    }
    if (!validEntities.isEmpty())
    {
        SearchManager::Instance().onItemsDeletedNotify(validEntities);
    }
}

void LibraryModel::scheduleOnIdle() { QMetaObject::invokeMethod(this, "onIdle", Qt::QueuedConnection); }

bool LibraryModel::hasWorkToDoOnIdle() { return !m_allEntitiesFiles.empty() || !m_missingFiles.empty(); }
