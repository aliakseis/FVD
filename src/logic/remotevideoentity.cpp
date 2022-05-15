#include "remotevideoentity.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>

#include "downloadentity.h"
#include "gui/videoplayerwidget.h"
#include "searchmanager.h"
#include "utilities/instantiator.h"
#include "utilities/notify_helper.h"

class FileReleasedRestartCallback : public NotifyHelper
{
public:
    FileReleasedRestartCallback(QPointer<DownloadEntity> entity) : m_entity(std::move(entity)) {}

    void slotNoParams() override
    {
        if (!m_entity.isNull())
        {
            RemoteVideoEntity::restart(m_entity);
        }

        deleteLater();
    }

private:
    QPointer<DownloadEntity> m_entity;
};

const QDateTime nullDateTime;

SearchResult::SearchResult() : published(nullDateTime), viewCount(0), duration(0) {}

REGISTER_QOBJECT_METATYPE(RemoteVideoEntity)

RemoteVideoEntity::RemoteVideoEntity() : QObject(nullptr), m_preferredResolutionId(0), m_lastErrorCode(Errors::NoError)
{
    connect(this, &RemoteVideoEntity::handleExtractedLinks, this, &RemoteVideoEntity::onHandleExtractedLinks);
    VERIFY(connect(this, SIGNAL(signRVEProgressUpdated()), &SearchManager::Instance(), SLOT(onDownloadChanged())));
}

RemoteVideoEntity::~RemoteVideoEntity() = default;

void RemoteVideoEntity::extractLinks()
{
    qDebug() << __FUNCTION__;
    ScriptStrategy* strategy = SearchManager::Instance().scriptStrategy(m_videoInfo.strategyName);
    Q_ASSERT(strategy);
    strategy->extractDirectLinks(m_videoInfo.originalUrl, this);
}

DownloadEntity* RemoteVideoEntity::requestStartDownload(VisibilityState visState /* = visNorm*/)
{
    return download(m_preferredResolutionId, visState);
}

DownloadEntity* RemoteVideoEntity::download(int resolutionId, VisibilityState visState)
{
    qDebug() << __FUNCTION__ << "resolutionId:" << resolutionId << "visState:" << visState;

    DownloadEntity* resultEntity = nullptr;
    auto it =
        std::find_if(m_downloads.begin(), m_downloads.end(),
                     [&resolutionId](DownloadEntity* ent) { return (ent->currentResolutionId() == resolutionId); });
    if (it == m_downloads.end())
    {
        if (m_downloads.empty() && m_resolutionLinks.empty() && visState == visNorm) {
            m_delayAddEntity = true;
        }

        resultEntity = DownloadEntity::create(this, visState, false);
        m_downloads.append(resultEntity);

        if (m_resolutionLinks.isEmpty())
        {
            extractLinks();
        }
        else
        {
            resultEntity->setup(m_resolutionLinks[resolutionId]);
        }

        if (visState == visNorm)
        {
            if (!m_delayAddEntity) {
                /*emit*/ downloadEntitiesAdded(QList<DownloadEntity*>() << resultEntity);
            }
        }
        else if (visState == visTemp && !m_resolutionLinks.isEmpty())
        {
            resultEntity->downloadTemp();
        }
    }
    else if (visState == visNorm)
    {
        resultEntity = *it;
        const bool isFileExists = resultEntity->isFileExists();
        if (visNorm != resultEntity->visibilityState())  // Jedi Yoda's style comparison
        {
            if (isFileExists)
            {
                if (visTemp == resultEntity->visibilityState())
                {
                    resultEntity->setVisibilityState(visNorm);
                }
                /*emit*/ downloadEntitiesAdded(QList<DownloadEntity*>() << resultEntity);
            }
            else
            {
                /*emit*/ downloadEntitiesRemoved(QList<DownloadEntity*>() << resultEntity);
                download(resolutionId, visState);
            }
        }
        else if (isFileExists && resultEntity->state() == Downloadable::kFinished)
        {
            if (DownloadEntity::confirmRestartDownload())
            {
                if (fileIsInUse(VideoPlayerWidgetInstance(), resultEntity))
                {
                    auto* fileReleasedCallback = new FileReleasedRestartCallback(resultEntity);
                    VERIFY(connect(VideoPlayerWidgetInstance(), SIGNAL(fileReleased()), fileReleasedCallback,
                                   SLOT(slotNoParams())));
                    VideoPlayerWidgetInstance()->stopVideo(true);
                }
                else
                {
                    resultEntity->restart();
                }
            }
        }
        else if (m_resolutionLinks.isEmpty())
        {
            m_downloads.erase(it);
            m_downloads.append(resultEntity);  // move it to the end
            extractLinks();
        }
        else if (resultEntity->state() == Downloadable::kFailed)
        {
            resultEntity->enqueueAndResetFailedState();
        }
    }

    return resultEntity;
}

QString RemoteVideoEntity::prefResolution() const
{
    DownloadEntity* entity = actualDownload();
    if (entity != nullptr)
    {
        return entity->currentResolution();
    }
    return m_resolutionLinks[m_preferredResolutionId].resolution;
}

DownloadEntity* RemoteVideoEntity::getLastVisible() const
{
    QList<DownloadEntity*>::const_iterator it = m_downloads.end();
    while (it != m_downloads.begin())
    {
        --it;
        if ((*it)->visibilityState() == visNorm || (*it)->visibilityState() == visLibOnly)
        {
            return *it;
        }
    }

    return nullptr;
}

QVariant RemoteVideoEntity::progress() const
{
    DownloadEntity* lastVisible = getLastVisible();

    return (lastVisible != nullptr) ? lastVisible->progress() : QVariant();
}

Downloadable::State RemoteVideoEntity::state() const
{
    DownloadEntity* lastVisible = getLastVisible();

    return (lastVisible != nullptr) ? lastVisible->state() : Downloadable::kFailed;
}

void RemoteVideoEntity::onHandleExtractedLinks(const QMap<int, LinkInfo>& links, int preferredResolutionId)
{
    if (!links.isEmpty())
    {
        m_resolutionLinks = links;
        m_lastErrorCode = Errors::NoError;

        if (m_delayAddEntity)
        {
            m_delayAddEntity = false;
            if (!m_downloads.empty())
            {
                /*emit*/ downloadEntitiesAdded(QList<DownloadEntity*>() << m_downloads[0]);
            }
        }

        m_preferredResolutionId = preferredResolutionId;
        // if given preferred resolution id doesn't exist in the links map, set it as first item
        if (!m_resolutionLinks.contains(m_preferredResolutionId) && !m_resolutionLinks.empty())
        {
            Q_ASSERT(false);
            m_preferredResolutionId = m_resolutionLinks.begin().key();
        }

        bool nextIteraion = false;
        for (auto it(m_downloads.end()), itBegin(m_downloads.begin()); it != itBegin; nextIteraion = true)
        {
            --it;
            DownloadEntity* de = *it;
            const bool entityBeingReloaded = DownloadEntity::kFailed == de->state() && !de->isFailed();
            if (nextIteraion && !entityBeingReloaded)
            {
                break;
            }

            int currResolutionID = de->currentResolutionId();
            if (currResolutionID == -1 || !m_resolutionLinks.contains(currResolutionID))
            {
                currResolutionID = m_preferredResolutionId;
            }
            de->setup(links[currResolutionID]);

            if (de->visibilityState() == visTemp)
            {
                startTempDownloading(m_downloads[0]);
            }

            if (!entityBeingReloaded)
            {
                break;
            }

            de->setState(DownloadEntity::kQueued);  // trigger download, unless it failed for the second time
        }
    }
    else
    {
        if (!m_downloads.isEmpty())  // We appended one empty download
        {
            DownloadEntity* de = m_downloads[m_downloads.size() - 1];
            de->setState(DownloadEntity::kFailed);
        }
        m_lastErrorCode = Errors::FailedToExtractLinks;
    }

    emit linksExtracted();
    emit signRVEUpdated();
}
void RemoteVideoEntity::onlinksExtracted(QVariantMap links, int preferredResolutionId)
{
    QMap<int, LinkInfo> linksMap;
    for (auto it = links.begin(); it != links.end(); ++it)
    {
        QVariantMap map = it.value().toMap();
        LinkInfo linkInfo;
        linkInfo.directLink = map["url"].toString();
        linkInfo.extension = map["extension"].toString();
        linkInfo.resolution = map["resolution"].toString();
        linkInfo.resolutionId = it.key().toInt();
        linksMap[it.key().toInt()] = linkInfo;
    }
    emit handleExtractedLinks(linksMap, preferredResolutionId);
}

void RemoteVideoEntity::extractFailed()
{
    qDebug() << __FUNCTION__;

    m_lastErrorCode = Errors::FailedToExtractLinks;
}

DownloadEntity* RemoteVideoEntity::actualDownload() const
{
    return (m_downloads.empty() ? nullptr : *m_downloads.constBegin());
}

QObjectList RemoteVideoEntity::getDownloads() const
{
    if (1 == m_downloads.size() && m_downloads[0]->isPersistable())
    {
        static QObjectList singleElementResult;
        if (singleElementResult.isEmpty())
        {
            singleElementResult.push_back(m_downloads[0]);
            return singleElementResult;
        }
        if (singleElementResult.isDetached())
        {
            singleElementResult[0] = m_downloads[0];
            return singleElementResult;
        }
    }

    QObjectList result;
    std::copy_if(m_downloads.begin(), m_downloads.end(), std::back_inserter(result), &isDownloadEntityPersistable);
    return result;
}

QList<DownloadEntity*> RemoteVideoEntity::allDownloadEntities() const { return m_downloads; }

void RemoteVideoEntity::setDownloads(const QObjectList& items)
{
    m_downloads.clear();

    for (auto* item : items)
    {
        auto* entity = dynamic_cast<DownloadEntity*>(item);
        if (nullptr == entity)
        {
            qWarning() << __FUNCTION__ << ": Item is not a DownloadEntity";
            continue;
        }

        if (entity->state() == DownloadEntity::kDownloading)
        {
            entity->setState(DownloadEntity::kQueued);
        }

        m_downloads.append(entity);

        if (entity->visibilityState() == visNorm)
        {
            VERIFY(connect(entity, SIGNAL(progressChanged(qint64, qint64)), this, SIGNAL(signRVEProgressUpdated())));
            VERIFY(connect(entity, SIGNAL(stateChanged(Downloadable::State, Downloadable::State)), this,
                           SIGNAL(signRVEUpdated())));
        }
    }
}

void RemoteVideoEntity::removeDownload(DownloadEntity* entity)
{
    Q_ASSERT(entity);
    auto it = std::find(m_downloads.begin(), m_downloads.end(), entity);
    if (it != m_downloads.end())
    {
        qDebug() << QString("Delete DownloadEntity %1 in function %2").arg(m_videoInfo.videoTitle, __FUNCTION__);
        (*it)->deleteLater();
        m_downloads.erase(it);
    }
}

bool RemoteVideoEntity::hasPersistableDownloads() const
{
    return std::find_if(m_downloads.begin(), m_downloads.end(), &isDownloadEntityPersistable) != m_downloads.end();
}

DownloadEntity* RemoteVideoEntity::createDownloadEntityByFilename(const QString& fileName)
{
    DownloadEntity* entity = DownloadEntity::create(this, visLibOnly, true);
    entity->setState(Downloadable::kFinished);
    entity->setFilename(fileName);
    entity->setDownloadedSize(QFileInfo(fileName).size());
    m_downloads.append(entity);
    return entity;
}

void RemoteVideoEntity::connectEntityProgress(const QList<DownloadEntity*>& entities)
{
    Q_FOREACH (DownloadEntity* ent, m_downloads)
    {
        ent->disconnect(this);
    }

    Q_FOREACH (DownloadEntity* entity, entities)
    {
        VERIFY(connect(entity, SIGNAL(progressChanged(qint64, qint64)), this, SIGNAL(signRVEProgressUpdated())));
    }
}

void RemoteVideoEntity::startTempDownloading(DownloadEntity* de)
{
    if (de->downloadTemp())
    {
        qDebug() << QString("%1").arg(__FUNCTION__);
    }
}

void RemoteVideoEntity::deleteTempDE(DownloadEntity* de)
{
    if ((de != nullptr) && de->visibilityState() == visTemp)
    {
        qDebug() << QString("%1(%2)").arg(__FUNCTION__, de->directUrl());
        de->stopTemp();
        removeDownload(de);
    }
}

void RemoteVideoEntity::pauseTempDE(DownloadEntity* de)
{
    if ((de != nullptr) && de->visibilityState() == visTemp)
    {
        qDebug() << QString("%1(%2)").arg(__FUNCTION__, de->directUrl());
        de->pauseTemp();
    }
}

bool RemoteVideoEntity::restart(DownloadEntity* entity) { return entity->restart(); }

void RemoteVideoEntity::downloadEntitiesAdded(const QList<DownloadEntity*>& entities)
{
    SearchManager::Instance().onItemsExistNotify(entities);
    connectEntityProgress(entities);
}

void RemoteVideoEntity::downloadEntitiesRemoved(const QList<DownloadEntity*>& entities)
{
    SearchManager::Instance().onItemsDeletedNotify(entities);
}
