#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>
#include <QScopedPointer>
#include <QVector>
#include <QMimeData>

#include <array>
#include <iterator>
#include <unordered_set>

#include "downloadentity.h"
#include "scriptstrategy.h"
#include "stdint.h"
#include "utilities/errorcode.h"
#include "utilities/singleton.h"
#include "utilities/utils.h"

class ViewModelFacade;
class SearchManager : public QObject, public Singleton<SearchManager>
{
    Q_OBJECT
    friend class Singleton<SearchManager>;
    typedef QSharedPointer<RemoteVideoEntity> EntitiesSetItem_t;

public:
    void search(const QString& query, const QStringList& sites, int page = 1);
    void cancelSearch();
    void clearSearchResults();
    void reInitializeStrategies()
    {
        m_scriptStrategies.clear();
        InitializeStrategies();
    }

    void clearScriptStrategies() { m_scriptStrategies.clear(); }
    std::array<QStringList, 2> allStrategiesNames() const;
    ScriptStrategy* scriptStrategy(const QString& name) const;

    Q_PROPERTY(QObjectList entities READ getEntities WRITE setEntities)
    QObjectList getEntities() const;
    void setEntities(const QObjectList& items);

    void removeDownload(DownloadEntity* entity);
    void removeDownload(const QList<DownloadEntity*>& entities);

    static void download(RemoteVideoEntity* rve, int resolutionId, VisibilityState visState);

    template <typename T>
    void onLibraryFilesAdded(const T& list)
    {
        QList<DownloadEntity*> dles;
        for (const auto& v : list)
        {
            DownloadEntity* de = createLibraryDE(v);
            if (m_allEntities.insert(QSharedPointer<RemoteVideoEntity>(de->getParent())).second)
            {
                dles.push_back(de);
            }
        }
        if (!dles.isEmpty())
        {
            emit notifyLibraryAddItems(dles, this);
        }
    }
    void onLibraryFileAdded(const QString& path)
    {
        const QString param[] = {path};
        onLibraryFilesAdded(param);
    }

    bool addLinks(const QMimeData& urls);
    void addLinks(QStringList urls);

private:
    SearchManager();

    static DownloadEntity* createLibraryDE(const QString& fileName);
    void InitializeStrategies();

    void addLink(const QString& url);

private:
    struct rveHasher
    {
        size_t operator()(const EntitiesSetItem_t& rve) const
        {
            return std::hash<std::string>()(rve->id().toStdString());
        }
    };
    struct rveComparer
    {
        bool operator()(const EntitiesSetItem_t& l, const EntitiesSetItem_t& r) const
        {
            return l->id() == r->id() && l->strategyName() == r->strategyName();
        }
    };

    typedef std::unordered_set<EntitiesSetItem_t, rveHasher, rveComparer> EntitiesSet;
    EntitiesSet m_allEntities;
    QList<QSharedPointer<ScriptStrategy> > m_scriptStrategies;
    int m_searchSitesAmount;  // temporary value to handle search strategies status

    DISALLOW_COPY_AND_ASSIGN(SearchManager);
Q_SIGNALS:
    void searchFinished();
    void searchResultFound(QList<RemoteVideoEntity*> entities);
    void searchFailed(const QString& error);

    void notifyLibraryAddItems(const QList<DownloadEntity*>& list, QObject* senderObject);
    void notifyAddItemsInModel(const QList<DownloadEntity*>& list, QObject* obj);
    void notifyRemoveItemsFromModel(const QList<DownloadEntity*>& list, QObject* obj);

    void signalSearchStarted(const QString& query);
    void signalDownloadStarted(const QString& url);

    void downloadProgressChanged(int progress);
    void downloadFinished(const QString& videoTitle);

public Q_SLOTS:
    void onSearchResultsFound(const QList<SearchResult>& searchResults);

    void onDownloadStarted(const QString& url);
    void onDownloadError(utilities::ErrorCode::ERROR_CODES, const QString&);
    void onDownloadFinished();

    void onItemsRemovedNotify(const QList<DownloadEntity*>& list);
    void onItemsDeletedNotify(const QList<DownloadEntity*>& list);
    void onItemsExistNotify(const QList<DownloadEntity*>& list);

    void onDownloadChanged();
};
