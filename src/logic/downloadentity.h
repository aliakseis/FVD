#pragma once

#include <QObject>
#include <QScopedPointer>
#include <QString>

#include "download/downloader.h"
#include "remotevideoentity.h"
#include "utilities/credsretriever.h"
#include "utilities/errorcode.h"
#include "utilities/utils.h"

template <typename F>
class AdvisingClassFactory;

template <typename T>
class ControllableByRVE
{
    friend class RemoteVideoEntity;

protected:
    static T* create(RemoteVideoEntity* parentVideoEntity, VisibilityState vState, bool isFileAssigned)
    {
        return new T(parentVideoEntity, vState, isFileAssigned);
    }
    void setup(const LinkInfo& info) { static_cast<T*>(this)->doSetup(info); }
    bool downloadTemp() { return static_cast<T*>(this)->doDownload(); }
    void stopTemp() { return static_cast<T*>(this)->doStop(); }
    void pauseTemp() { return static_cast<T*>(this)->doPause(); }
};

template <typename T>
class ControllableBySM
{
    friend class SearchManager;

protected:
    void remove() { static_cast<T*>(this)->doRemove(); }
};

template <typename T>
class ControllableByRVEandSM
{
    friend class RemoteVideoEntity;
    friend class SearchManager;

protected:
    void setVisibilityState(VisibilityState value) { static_cast<T*>(this)->doSetVisibilityState(value); }
};

template <typename T>
class ControllableByDM
{
    friend class DownloadManager;

protected:
    bool download() { return static_cast<T*>(this)->doDownload(); }
    void pause() { return static_cast<T*>(this)->doPause(); }
    bool resume() { return static_cast<T*>(this)->doResume(); }
    void stop() { return static_cast<T*>(this)->doStop(); }

    void setSpeedLimit(int speedLimit)
    {
        auto downloader = static_cast<T*>(this)->m_downloader.data();
        if (downloader != 0)
        {
            downloader->setSpeedLimit(speedLimit);
        }
    }
};

template <typename T>
class ControllableByRVEandDM
{
    friend class RemoteVideoEntity;
    friend class DownloadManager;

protected:
    bool restart() { return static_cast<T*>(this)->doRestart(); }
};

class DownloadEntity : public QObject,
                       public Downloadable,
                       public ControllableByRVE<DownloadEntity>,
                       public ControllableBySM<DownloadEntity>,
                       public ControllableByRVEandSM<DownloadEntity>,
                       public ControllableByDM<DownloadEntity>,
                       public ControllableByRVEandDM<DownloadEntity>,
                       public download::DownloaderObserverInterface,
                       public IParentAdvice
{
    Q_OBJECT
    Q_PROPERTY(int state READ state WRITE decerializeSetState)
    Q_PROPERTY(QString filename READ filename WRITE setFilename)
    Q_PROPERTY(QString destinationPath READ destinationPath WRITE setDestinationPath)
    Q_PROPERTY(qint64 totalFileSize READ totalFileSize WRITE setTotalFileSize)
    Q_PROPERTY(qint64 downloadedSize READ downloadedSize WRITE setDownloadedSize)
    Q_PROPERTY(QString directUrl READ directUrl WRITE setDirectUrl)
    Q_PROPERTY(QString currentResolution READ currentResolution WRITE setCurrentResolution)
    Q_PROPERTY(int currentResolutionId READ currentResolutionId WRITE setCurrentResolutionId)
    Q_PROPERTY(int visibilityState READ visibilityState WRITE decerializeSetVisibilityState)
    Q_PROPERTY(QString fileExtension READ fileExtension WRITE setFileExtension)

private:
    DownloadEntity(RemoteVideoEntity* parentVideoEntity = 0, VisibilityState vState = visNorm,
                   bool isFileAssigned = true);

public:
#ifdef ALLOW_TRAFFIC_CONTROL
    typedef download::Downloader<download::speed_limitable_tag, false> DownloaderType;
#else
    typedef download::Downloader<download::speed_readable_tag, false> DownloaderType;
#endif  // ALLOW_TRAFFIC_CONTROL

    ~DownloadEntity()
    {
        if (visTemp == m_visibilityState)
        {
            utilities::DeleteFileWithWaiting(m_filepath);
        }
    }

    bool operator==(const DownloadEntity& download_entity) const
    {
        return m_parentVideoEntity->m_videoInfo.id == download_entity.m_parentVideoEntity->m_videoInfo.id;
    }
    bool operator!=(const DownloadEntity& download_entity) const { return !(*this == download_entity); }
    State state() const { return m_state; }
    void setState(State state);
    void enqueueAndResetFailedState();
    static QString stateToString(State s);

    RemoteVideoEntity* getParent() const { return m_parentVideoEntity; }

    const QString& filename() const { return m_filepath; }
    void setFilename(const QString& filename) { m_filepath = filename; }

    const QString& destinationPath() const
    {
        return m_downloader.isNull() ? m_destinationPath : m_downloader->destinationPath();
    }
    void setDestinationPath(const QString& destinationPath)
    {
        if (m_downloader.isNull())
        {
            m_destinationPath = destinationPath;
        }
        else
        {
            m_downloader->setDestinationPath(destinationPath);
        }
    }

    const QString& videoTitle() const { return m_parentVideoEntity->m_videoInfo.videoTitle; }
    int videoDuration() const { return m_parentVideoEntity->m_videoInfo.duration; }

    QString strategyName() const { return m_parentVideoEntity->m_videoInfo.strategyName; }
    QString description() const
    {
        Q_ASSERT(m_parentVideoEntity);
        return m_parentVideoEntity->m_videoInfo.description;
    }

    QDateTime published() const { return m_parentVideoEntity->m_videoInfo.published; }
    QDateTime fileCreated() const
    {
        if (!m_lastModified.isValid())
        {
            m_lastModified = QFileInfo(m_filepath).lastModified();
        }
        return m_lastModified;
    }
    qint64 totalFileSize() const
    {
        return (kFinished == state()) ? m_downloadedSize
                                      : (m_downloader.isNull() ? m_totalFileSize : m_downloader->totalFileSize());
    }
    void setTotalFileSize(qint64 value)
    {
        if (m_downloader.isNull())
        {
            m_totalFileSize = value;
        }
        else
        {
            m_downloader->setTotalFileSize(value);
            m_downloader->setExpectedFileSize(value);
        }
    }

    QString totalFileSizeAsString() const { return utilities::SizeToString(totalFileSize(), 1); }
    QString speedAsString() const { return (state() == kDownloading) ? m_speed : QString(); }
    qint64 speed() const { return m_bytesPerSecond; }

    bool isFileExists() const { return QFile::exists(m_filepath); }
    qint64 downloadedSize() const { return m_downloadedSize; }
    void setDownloadedSize(qint64 size) { m_downloadedSize = size; }

    QVariant progress() const;

    QString directUrl() const { return m_url; }
    int currentResolutionId() const { return m_currentResolutionId; }
    QString currentResolution() const { return m_currentResolution; }
    VisibilityState visibilityState() const { return m_visibilityState; }

    QString fileExtension() const { return m_fileExtension; }
    void setFileExtension(const QString& value) { m_fileExtension = value; }

    bool isPersistable() const
    {
        return (visibilityState() == visLibOnly && state() == kFinished) ||
               (visibilityState() != visTemp && !directUrl().isNull());
    }

    bool startDownloadWithHighestPriority();

    bool isFailed() const { return m_isFailed; }

    static bool confirmRestartDownload();

    int speedLimit() const { return m_downloader.isNull() ? 0 : m_downloader->speedLimit(); }

private:
    void doSetup(const LinkInfo& info);
    bool doDownload();
    void doPause();
    bool doResume();
    void doRemove();
    void doStop();
    bool doRestart();

    void decerializeSetState(int state)
    {
        // we don't emit stateChanged here
        m_state = (State)state;
    }
    void decerializeSetVisibilityState(int value) { m_visibilityState = (VisibilityState)value; }

private:
    // DownloaderObserverInterface interface
    void onProgress(qint64 bytes_downloaded) override;
    void onSpeed(qint64 bytes_per_second) override;
    void onFinished() override;
    void onFileCreated(const QString& filename) override;
    void onError(utilities::ErrorCode::ERROR_CODES code, const QString& err) override;
    void onFileToBeReleased(const QString& filename) override;
    void onNeedLogin(utilities::ICredentialsRetriever*) override {}
    void onReplyInvalidated() override {}
    void onStart(const QByteArray& data) override;

    void doSetVisibilityState(VisibilityState value);

    void setAdviceParent(QObject* parentVideoEntity) override
    {
        m_parentVideoEntity = dynamic_cast<RemoteVideoEntity*>(parentVideoEntity);
        QObject::setParent(parentVideoEntity);
    }

    void setDirectUrl(const QString& url) { m_url = url; }
    void setCurrentResolutionId(int currentResolutionId) { m_currentResolutionId = currentResolutionId; }
    void setCurrentResolution(const QString& cres) { m_currentResolution = cres; }

    DownloaderType* downloader();

    State m_state;
    QScopedPointer<DownloaderType> m_downloader;
    QString m_filepath;
    int m_currentResolutionId;
    QString m_currentResolution;
    qint64 m_downloadedSize;
    RemoteVideoEntity* m_parentVideoEntity;
    QString m_url;
    QString m_speed;
    QString m_fileExtension;
    VisibilityState m_visibilityState;
    bool m_isFailed;
    qint64 m_bytesPerSecond;

    mutable QDateTime m_lastModified;

    QString m_destinationPath;
    qint64 m_totalFileSize;
    bool m_isFileAssigned;

    DISALLOW_COPY_AND_ASSIGN(DownloadEntity);

    friend class AdvisingClassFactory<DownloadEntity>;
    friend class ControllableByRVE<DownloadEntity>;
    friend class ControllableBySM<DownloadEntity>;
    friend class ControllableByRVEandSM<DownloadEntity>;
    friend class ControllableByDM<DownloadEntity>;
    friend class ControllableByRVEandDM<DownloadEntity>;

Q_SIGNALS:
    void stateChanged(Downloadable::State newState, Downloadable::State lastState);
    void fileCreated(const QString& path);
    void fileRemoving();
    void finished();
    void downloadStarted(const QString& url);
    void errorHappened(utilities::ErrorCode::ERROR_CODES, const QString&);
    void progressChanged(qint64 downloaded, qint64 totalSize);
    void speed(qint64 bytesPerSecond);
};

Q_DECLARE_METATYPE(DownloadEntity*)

inline bool isDownloadEntityPersistable(const DownloadEntity* e) { return e->isPersistable(); }
