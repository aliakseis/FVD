#include "searchmanager.h"

#include <QMessageBox>
#include <QPushButton>
#include <QSharedPointer>
#include <QString>

#include "branding.hxx"
#include "globals.h"
#include "gui/videoplayerwidget.h"
#include "preferences.h"
#include "settings_declaration.h"


namespace {

QString UnescapeParameters(QString text)
{
    static const struct {
        const char* ampersand_code;
        const char replacement;
    } kEscapeToChars[] = {
        {"%2F", '/'},
        {"%3A", ':'},
        {"%3D", '='},
        {"%3F", '?'},
    };

    text.replace("&amp;", "&", Qt::CaseInsensitive);

    if (!text.contains('%')) {
        return text;
    }

    for (const auto& r : kEscapeToChars)
    {
        text.replace(QLatin1String(r.ampersand_code), QLatin1String(&r.replacement, 1), Qt::CaseInsensitive);
    }

    return text;
}

} // namespace

using namespace strategies;

SearchManager::SearchManager() : m_searchSitesAmount(0) { InitializeStrategies(); }

void SearchManager::InitializeStrategies()
{
    if (m_scriptStrategies.isEmpty())
    {
        QStringList listSites = QString(app_settings::SiteScripts).split(";", QString::SkipEmptyParts);
        ScriptStrategy* strategy;
        foreach (const QString& str, listSites)
        {
            strategy = new ScriptStrategy(str, true);
            m_scriptStrategies.push_back(QSharedPointer<ScriptStrategy>(strategy));
            VERIFY(QObject::connect(strategy, SIGNAL(searchResultsFound(const QList<SearchResult>&)),
                                    SLOT(onSearchResultsFound(const QList<SearchResult>&))));
        }
    }
}

void SearchManager::search(const QString& query, const QStringList& searchSites, int page)
{
    m_searchSitesAmount = searchSites.count();

    Q_FOREACH (const QString& name, searchSites)
    {
        ScriptStrategy* strategy = scriptStrategy(name);
        Q_ASSERT(strategy);
        QSettings settings;
        using namespace app_settings;
        strategy->search(
            query, static_cast<strategies::SortOrder>(settings.value(SearchSortOrder, SearchSortOrder_Default).toInt()),
            settings.value(resultsOnPage, resultsOnPage_Default).toInt(), page);
    }

    if (m_searchSitesAmount <= 0)
    {
        emit searchFinished();
    }
    emit signalSearchStarted(query);
}

void SearchManager::cancelSearch() { m_searchSitesAmount = 0; }

std::array<QStringList, 2> SearchManager::allStrategiesNames() const
{
    std::array<QStringList, 2> result;
    for (const auto& strat : qAsConst(m_scriptStrategies))
    {
        result[strat->isSafeForWork() ? 0 : 1].push_back(strat->name());
    }
    return result;
}

ScriptStrategy* SearchManager::scriptStrategy(const QString& name) const
{
    auto foundSearchStrategy =
        std::find_if(m_scriptStrategies.constBegin(), m_scriptStrategies.constEnd(),
                     [&name](const QSharedPointer<ScriptStrategy>& strat) { return strat->name() == name; });
    return (foundSearchStrategy != m_scriptStrategies.constEnd() ? foundSearchStrategy->data() : nullptr);
}

void SearchManager::onSearchResultsFound(const QList<SearchResult>& searchResults)
{
    if (m_searchSitesAmount > 0)
    {
        --m_searchSitesAmount;
    }

    QList<RemoteVideoEntity*> entitiesList;
    Q_FOREACH (SearchResult result, searchResults)
    {
        EntitiesSetItem_t videoEntity(new RemoteVideoEntity());
        videoEntity->m_videoInfo = result;

        auto myEnt = m_allEntities.find(videoEntity);

        if (myEnt == m_allEntities.end())
        {
            entitiesList.append(videoEntity.data());
            m_allEntities.insert(videoEntity);
        }
        else
        {
            entitiesList.append((*myEnt).data());
        }
    }
    emit searchResultFound(entitiesList);
    if (m_searchSitesAmount == 0)
    {
        emit searchFinished();
    }
}

QObjectList SearchManager::getEntities() const
{
    QObjectList result;
    result.reserve((int)m_allEntities.size());
    for (const auto& entity : m_allEntities)
    {
        if (entity->hasPersistableDownloads())
        {
            result.push_back(entity.data());
        }
    }
    return result;
}

void SearchManager::setEntities(const QObjectList& items)
{
    m_allEntities.clear();
    Q_FOREACH (QObject* item, items)
    {
        if (auto* videoEntity = dynamic_cast<RemoteVideoEntity*>(item))
        {
            m_allEntities.insert(QSharedPointer<RemoteVideoEntity>(videoEntity));
        }
        else
        {
            qWarning() << __FUNCTION__ << ": Item is not a RemoteVideoEntity";
        }
        // see RemoteVideoEntity::setParent
    }
}

void SearchManager::clearSearchResults()
{
    for (auto it = m_allEntities.begin(); it != m_allEntities.end();)
    {
        if (!(*it)->hasDownloads())
        {
            it = m_allEntities.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SearchManager::onDownloadStarted(const QString& url) { emit signalDownloadStarted(url); }

void SearchManager::onDownloadError(utilities::ErrorCode::ERROR_CODES code, const QString& /*unused*/)
{
    qDebug() << __FUNCTION__;

    auto* de = dynamic_cast<DownloadEntity*>(sender());
    if (nullptr == de)
    {
        return;
    }

    if (visTemp == de->visibilityState() || de->isFailed())
    {
        if (fileIsInUse(VideoPlayerWidgetInstance(), de))
        {
            VideoPlayerWidgetInstance()->stopVideo(true);
        }
        return;
    }

    if (code == utilities::ErrorCode::eDOWLDOPENFILERR)
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle(Tr::Tr(PROJECT_FULLNAME_TRANSLATION));
        msgBox.setText(Tr::Tr(CREATEFILE_ERROR).arg(de->filename()));
        QPushButton* setNewPathButton = msgBox.addButton(tr("Set new path"), QMessageBox::ActionRole);
        msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
        msgBox.exec();
        if (msgBox.clickedButton() == setNewPathButton)
        {
            Preferences prefs;
            int res = prefs.execSelectFolder();
            if (res == QDialog::Rejected)
            {
                return;
            }
        }
    }

    if (code != utilities::ErrorCode::eDOWLDUNKWNFILERR)  // possibly we would need to add some other statuses as well
    {
        const int resolutionId = de->currentResolutionId();

        RemoteVideoEntity* rve = de->getParent();
        rve->m_resolutionLinks.clear();
        rve->download(resolutionId, visNorm);
    }
    onDownloadChanged();
}

void SearchManager::onDownloadFinished()
{
    qDebug() << __FUNCTION__;
    onDownloadChanged();
    auto* finishedEntity = qobject_cast<DownloadEntity*>(sender());
    if ((finishedEntity != nullptr) && finishedEntity->visibilityState() != visTemp)
    {
        emit downloadFinished(finishedEntity->videoTitle());
    }
}

void SearchManager::removeDownload(DownloadEntity* entity)
{
    Q_ASSERT(entity);
    RemoteVideoEntity* parentEntity = entity->getParent();
    Q_ASSERT(parentEntity);
    entity->remove();
    parentEntity->removeDownload(entity);
    onDownloadChanged();
}

void SearchManager::removeDownload(const QList<DownloadEntity*>& entities)
{
    Q_FOREACH (DownloadEntity* ent, entities)
    {
        removeDownload(ent);
    }
    onDownloadChanged();
}

void SearchManager::download(RemoteVideoEntity* rve, int resolutionId, VisibilityState visState)
{
    Q_ASSERT(rve);
    rve->download(resolutionId, visState);
}

void SearchManager::onItemsRemovedNotify(const QList<DownloadEntity*>& list)
{
    Q_FOREACH (DownloadEntity* ent, list)
    {
        if (ent->state() != Downloadable::kFinished)
        {
            removeDownload(ent);
        }
        else
        {
            ent->setVisibilityState(visLibOnly);
        }
    }
}

/// \fn	void SearchManager::onItemsDeletedNotify(const QList<DownloadEntity*>& list)
///
/// \brief	Deletes items from model completely, with files.
///
/// \param	list	The list of items.
void SearchManager::onItemsDeletedNotify(const QList<DownloadEntity*>& list)
{
    emit notifyRemoveItemsFromModel(list, QObject::sender());
    removeDownload(list);
}

void SearchManager::onItemsExistNotify(const QList<DownloadEntity*>& list)
{
    emit notifyAddItemsInModel(list, QObject::sender());
}

void SearchManager::onDownloadChanged()
{
    float overallProgress = 0.0F;
    int downloadsAmount = 0;
    for (const auto& entity : m_allEntities)
    {
        if (entity->state() == Downloadable::kDownloading && !entity->progress().isNull())
        {
            overallProgress += entity->progress().toFloat();
            ++downloadsAmount;
        }
    }
    if (downloadsAmount > 0)
    {
        // TODO: use downloaded size / total size formula
        overallProgress = overallProgress / downloadsAmount;
    }
    emit downloadProgressChanged((int)overallProgress);
}

DownloadEntity* SearchManager::createLibraryDE(const QString& fileName)
{
    qDebug() << __FUNCTION__ << " fileName: " << fileName;

    auto* rve = new RemoteVideoEntity();
    QFileInfo finfo = QFileInfo(fileName);
    rve->m_videoInfo.id = fileName;
    rve->m_videoInfo.videoTitle = finfo.fileName();
    rve->m_videoInfo.description = finfo.fileName();
    rve->m_videoInfo.published = finfo.created();
    rve->m_videoInfo.thumbnailUrl = fileName;
    DownloadEntity* dle = rve->createDownloadEntityByFilename(fileName);
    return dle;
}

bool SearchManager::addLinks(const QMimeData& urls)
{
    QString text;
    QRegExp intrestedDataRx("(http|https):[^\\s\\n]+", Qt::CaseInsensitive);

    if (urls.hasHtml())
    {
        text = UnescapeParameters(urls.html());
        intrestedDataRx = QRegExp(R"((http|https):[^:"'<>\s\n]+)", Qt::CaseInsensitive);
    }
    else if (urls.hasText())
    {
        text = urls.text();
    }
    else if (urls.hasFormat("text/uri-list"))
    {
        text = urls.data("text/uri-list");
    }
    else
    {
        return false;
    }

    qDebug() << "Some data dropped to program. Trying to manage it.";

    int pos = 0;
    QStringList linksForDownload;
    while ((pos = intrestedDataRx.indexIn(text, pos)) != -1)
    {
        QString someLink = intrestedDataRx.cap(0);
        QString typeOfLink = intrestedDataRx.cap(1);
        qDebug() << QString(PROJECT_NAME) + " takes " + someLink;
        pos += std::max(5, intrestedDataRx.matchedLength() - 5);

        QUrl url(someLink);
        if (!url.isValid() || !url.hasQuery())
        {
            continue;
        }

        auto it = std::lower_bound(linksForDownload.begin(), linksForDownload.end(), someLink);
        if (it == linksForDownload.end())
        {
            linksForDownload.push_back(someLink);
        }
        else if (!someLink.startsWith(*it))
        {
            auto itNext = std::next(it);
            if (itNext == linksForDownload.end() || !itNext->startsWith(someLink))
            {
                linksForDownload.insert(it, someLink);
            }
            else
            {
                *itNext = someLink;
            }
        }
    }
    addLinks(linksForDownload);

    return true;
}

void SearchManager::addLinks(const QStringList& urls)
{
    for (const auto& url : urls)
    {
        for (const auto& strat : qAsConst(m_scriptStrategies))
        {
            EntitiesSetItem_t rve(new RemoteVideoEntity());

            rve->m_videoInfo.id = url;
            rve->m_videoInfo.videoTitle = url;
            rve->m_videoInfo.description = url;
            // Don't set rve->m_videoInfo.thumbnailUrl
            rve->m_videoInfo.originalUrl = url;
            rve->m_videoInfo.strategyName = strat->name();

            rve->setCreatedByUrl();

            if (m_allEntities.insert(rve).second)
            {
                rve->requestStartDownload();
            }
        }
    }
}
