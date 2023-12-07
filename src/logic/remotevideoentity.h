#pragma once

#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <utility>

#include "download/downloader.h"
#include "errors.h"
#include "utilities/utils.h"

class DownloadEntity;
namespace strategies
{
class LinkExtractorInterface;
}

struct LinkInfo
{
    int resolutionId;
    QString resolution, extension, directLink;

    LinkInfo(int resId = 0, const QString& resolution_ = QString(), const QString& extension_ = QString(),
             const QString& link = QString())
        : resolutionId(resId), resolution(resolution_), extension(extension_), directLink(link)
    {
    }
};

struct SearchResult
{
    SearchResult();

    QString id;
    QString videoTitle;
    QString thumbnailUrl;
    QString originalUrl;
    QString description;
    QString strategyName;
    QDateTime published;
    int viewCount;
    time_t duration;
};

#define DECLARE_RVE_PROPERTY(prop, read, write) \
    prop() const { return m_videoInfo.read; }   \
    void write(prop) { m_videoInfo.read = std::move(read); }

#define DECLARE_RVE_PROPERTY_ADAPTOR(params) DECLARE_RVE_PROPERTY params

enum VisibilityState
{
    visNorm,  // rename to visDownloads
    visLibOnly,
    visTemp
};

class RemoteVideoEntity : public QObject  //, public IParentAdvice
{
    Q_OBJECT
public:
    RemoteVideoEntity();
    virtual ~RemoteVideoEntity();

    DownloadEntity* download(int resolutionId, VisibilityState visState);
    DownloadEntity* requestStartDownload(VisibilityState visState = visNorm);

    void extractLinks();

    DownloadEntity* actualDownload() const;

    bool hasDownloads() const { return !m_downloads.isEmpty(); }
    bool hasPersistableDownloads() const;

    DownloadEntity* createDownloadEntityByFilename(const QString& fileName);

    QVariant progress() const;
    Downloadable::State state() const;
    LinkInfo linkInfo(int linkId) const { return m_resolutionLinks[linkId]; }
    QString prefResolution() const;

    Errors::Code lastError() const { return m_lastErrorCode; }

    QList<DownloadEntity*> allDownloadEntities() const;

    QMap<int, LinkInfo> m_resolutionLinks;
    SearchResult m_videoInfo;

    /// \fn	void RemoteVideoEntity::deleteTempDE(DownloadEntity* de);
    /// \brief	Deletes the DownloadEntity if it is temporary (i.e. for preview).
    void deleteTempDE(DownloadEntity* de);
    static void pauseTempDE(DownloadEntity* de);
    static void startTempDownloading(DownloadEntity* de);

    void setCreatedByUrl(const QString& url, const QString& strategyName);

#ifndef Q_MOC_RUN

#define READ ,
#define WRITE ,
#undef Q_PROPERTY
#define Q_PROPERTY(...) DECLARE_RVE_PROPERTY_ADAPTOR((__VA_ARGS__))

#endif

    Q_PROPERTY(QString id READ id WRITE setId)
    Q_PROPERTY(QString videoTitle READ videoTitle WRITE setVideoTitle)
    Q_PROPERTY(QString thumbnailUrl READ thumbnailUrl WRITE setThumbnailUrl)
    Q_PROPERTY(QString originalUrl READ originalUrl WRITE setOriginalUrl)
    Q_PROPERTY(QString description READ description WRITE setDescription)
    Q_PROPERTY(QString strategyName READ strategyName WRITE setStrategyName)
    Q_PROPERTY(QDateTime published READ published WRITE setPublished)
    Q_PROPERTY(int viewCount READ viewCount WRITE setViewCount)
    Q_PROPERTY(int duration READ duration WRITE setDuration)

#undef Q_PROPERTY
#define Q_PROPERTY(text)
#undef READ
#undef WRITE

private:
    Q_PROPERTY(QObjectList downloads READ getDownloads WRITE setDownloads)
    QObjectList getDownloads() const;
    void setDownloads(const QObjectList& items);
    Q_PROPERTY(int preferredResolutionId READ preferredResolutionId WRITE setPreferredResolutionId)
    int preferredResolutionId() const { return m_preferredResolutionId; }
    void setPreferredResolutionId(int preferredResolutionId) { m_preferredResolutionId = preferredResolutionId; }

    void removeDownload(DownloadEntity* entity);

    DownloadEntity* getLastVisible() const;

    static bool restart(DownloadEntity* entity);

    void downloadEntitiesAdded(const QList<DownloadEntity*>&);
    static void downloadEntitiesRemoved(const QList<DownloadEntity*>&);

    void manageDownloads();

signals:
    void linksExtracted();
    void signRequestStartDwnl(VisibilityState visState);
    void signRVEUpdated();
    void signRVEProgressUpdated();

    void handleExtractedLinks(const QMap<int, LinkInfo>& links, int preferredResolutionId);

    void startByUrlFailed();

public Q_SLOTS:
    void onlinksExtracted(QVariantMap links, int preferredResolutionId);
    void onlinksExtractionFinished();

private Q_SLOTS:
    void onHandleExtractedLinks(const QMap<int, LinkInfo>& links, int preferredResolutionId);
    void extractFailed();
    void connectEntityProgress(const QList<DownloadEntity*>&);
    void onInfoRequestFinished();

private:
    friend class SearchManager;
    friend class FileReleasedRestartCallback;

    QList<DownloadEntity*> m_downloads;
    int m_preferredResolutionId;
    Errors::Code m_lastErrorCode;

    bool m_createdByUrl = false;
    bool m_delayAddEntity = false;
    int m_redirectCount = 0;

    DISALLOW_COPY_AND_ASSIGN(RemoteVideoEntity);
};

#undef DECLARE_RVE_PROPERTY_ADAPTOR
#undef DECLARE_RVE_PROPERTY

inline bool operator==(const RemoteVideoEntity& a1, const RemoteVideoEntity& a2)
{
    return a1.m_videoInfo.id == a2.m_videoInfo.id;
}
