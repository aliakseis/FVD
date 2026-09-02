#pragma once

#include "IDownloader.h"

#include <QObject>
#include <QFile>
#include <QMutex>
#include <QList>
#include <QUrl>

#include <atomic>
#include <memory>
#include <thread>

struct AVIOContext;
struct AVFormatContext;
struct AVPacket;

class FFmpegMergeDownloader final : public QObject, public IDownloader
{
    //Q_OBJECT

public:
    explicit FFmpegMergeDownloader(QObject* parent = nullptr);
    ~FFmpegMergeDownloader() override;

    // IDownloader
    const QString& destinationPath() const override;
    bool setDestinationPath(const QString& destination_path) override;

    qint64 totalFileSize() const override;
    void setTotalFileSize(qint64 value) override;
    void setExpectedFileSize(qint64 expected_size) override;

    int speedLimit() const override;
    void setSpeedLimit(int value) override;

    void setDownloadNamePolicy(
        DuplicateDownloadNamePolicy policy) override;

    void Start(
        const QList<QUrl>& urls,
        QNetworkAccessManager* network_manager,
        const QString& filename = QString(),
        const QStringList& httpHeaders = QStringList()) override;

    void Pause() override;

    void Resume(
        const QList<QUrl>& urls,
        QNetworkAccessManager* network_manager,
        const QString& filename = QString(),
        const QStringList& httpHeaders = QStringList()) override;

    void Stop() override;

    void setObserver(
        DownloaderObserverInterface* observer) override;

private:
    struct OutputContext;

    void workerMain(
        QList<QUrl> urls,
        QString outputFilename);

    bool merge(
        const QUrl& videoUrl,
        const QUrl& audioUrl,
        const QString& outputFilename,
        QString& errorDescription,
        utilities::ErrorCode::ERROR_CODES& errorCode);

    bool openInput(
        const QUrl& url,
        AVFormatContext** context,
        QString& errorDescription,
        utilities::ErrorCode::ERROR_CODES& errorCode);

    QString makeOutputFilename(
        const QString& requestedFilename,
        const QUrl& videoUrl) const;

    QString makeUniqueFilename(
        const QString& filename) const;

    void notifyFinished();
    void notifyError(
        utilities::ErrorCode::ERROR_CODES code,
        const QString& description);
    void notifyProgress(qint64 bytes);
    void notifySpeed(qint64 bytesPerSecond);
    void notifyFileCreated(const QString& filename);

    static QString ffmpegErrorString(int error);

    static utilities::ErrorCode::ERROR_CODES
    mapFfmpegError(int error);

private:
    QString m_destinationPath;
    qint64 m_totalFileSize = -1;
    qint64 m_expectedFileSize = -1;

    int m_speedLimit = 0;

    DuplicateDownloadNamePolicy m_namePolicy =
        kGenerateNewName;

    DownloaderObserverInterface* m_observer = nullptr;

    std::thread m_worker;

    std::atomic_bool m_running{false};

    // Reserved for future Pause/Stop implementation.
    std::atomic_bool m_stopRequested{false};
    std::atomic_bool m_pauseRequested{false};

    mutable QMutex m_mutex;
};
