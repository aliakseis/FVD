#pragma once

#include "utilities/errorcode.h"
#include "utilities/credsretriever.h"

#include <QNetworkAccessManager>


/// \class DownloaderObserverInterface Observer interface for the Downloader
struct DownloaderObserverInterface
{
    virtual void onFinished() = 0;
    virtual void onError(utilities::ErrorCode::ERROR_CODES code, const QString& description) = 0;
    virtual void onProgress(qint64 bytes_downloaded) = 0;
    virtual void onSpeed(qint64 bytes_per_second) = 0;
    virtual void onFileCreated(const QString& filename) = 0;
    virtual void onNeedLogin(utilities::ICredentialsRetriever* retriever) = 0;
    virtual void onReplyInvalidated() = 0;
    virtual void onFileToBeReleased(const QString& filename) = 0;
    virtual void onStart(const QByteArray& data) = 0;
}; // DownloaderObserverInterface

struct IDownloader
{
    /// \enum    DuplicateDownloadNamePolicy
    /// \brief    kReplaceFile does not change name;
    ///         kGenerateNewName downloads to "filename(<N+1>).ext", where N=0..MAX_INT, until file exists;
    ///
    enum DuplicateDownloadNamePolicy { kGenerateNewName, kReplaceFile };

    virtual const QString& destinationPath() const = 0;
    virtual bool setDestinationPath(const QString& destination_path) = 0;
    virtual qint64 totalFileSize() const = 0;
    virtual void setTotalFileSize(qint64 value) = 0;
    virtual void setExpectedFileSize(qint64 expected_size) = 0;
    virtual int speedLimit() const = 0;
    virtual void setSpeedLimit(int) = 0;
    virtual void setDownloadNamePolicy(DuplicateDownloadNamePolicy download_name_policy) = 0;

    virtual void Start(const QUrl& url, QNetworkAccessManager* network_manager, const QString& filename = QString(), const QStringList& httpHeaders = QStringList()) = 0;
    virtual void Pause() = 0;
    virtual void Resume(const QUrl& url, QNetworkAccessManager* network_manager, const QString& filename = QString(), const QStringList& httpHeaders = QStringList()) = 0;
    virtual void Stop() = 0;

    virtual void setObserver(DownloaderObserverInterface* observer) = 0;
};
