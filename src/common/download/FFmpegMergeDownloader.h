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
#include <mutex>

struct AVIOContext;
struct AVFormatContext;
struct AVPacket;

class FFmpegMergeDownloader final
    : public QObject
    , public IDownloader
{
    //Q_OBJECT

public:
    explicit FFmpegMergeDownloader(
        QObject* parent = nullptr);

    ~FFmpegMergeDownloader() override;

    // ---------------------------------------------------------------------
    // IDownloader
    // ---------------------------------------------------------------------

    const QString& destinationPath() const override;

    bool setDestinationPath(
        const QString& destination_path) override;

    qint64 totalFileSize() const override;

    void setTotalFileSize(
        qint64 value) override;

    void setExpectedFileSize(
        qint64 expected_size) override;

    int speedLimit() const override;

    void setSpeedLimit(
        int value) override;

    void setDownloadNamePolicy(
        DuplicateDownloadNamePolicy download_name_policy) override;

    void Start(
        const QList<QUrl>& urls,
        QNetworkAccessManager* network_manager,
        const QString& filename = QString(),
        const QStringList& httpHeaders = QStringList()) override;

    void Resume(
        const QList<QUrl>& urls,
        QNetworkAccessManager* network_manager,
        const QString& filename = QString(),
        const QStringList& httpHeaders = QStringList()) override;

    void Pause() override;

    void Stop() override;

    void setObserver(
        DownloaderObserverInterface* observer) override;

private:
    struct OutputContext;

    // ---------------------------------------------------------------------
    // Worker
    // ---------------------------------------------------------------------

    void mergeWorker(
        QList<QUrl> urls,
        QString outputFilename);

    // ---------------------------------------------------------------------
    // Filename handling
    // ---------------------------------------------------------------------

    QString makeOutputFilename(
        const QList<QUrl>& urls,
        const QString& filename) const;

    // ---------------------------------------------------------------------
    // Observer notification helpers
    // ---------------------------------------------------------------------

    void notifyStart(
        const QByteArray& data);

    void notifyProgress(
        qint64 bytes);

    void notifySpeed(
        qint64 bytesPerSecond);

    void notifyFileCreated(
        const QString& filename);

    void notifyFinished();

    void notifyError(
        utilities::ErrorCode::ERROR_CODES code,
        const QString& description);

private:
    // ---------------------------------------------------------------------
    // Configuration
    // ---------------------------------------------------------------------

    QString m_destinationPath;

    DuplicateDownloadNamePolicy m_downloadNamePolicy =
        kGenerateNewName;

    std::atomic<qint64> m_totalFileSize{ -1 };

    std::atomic<qint64> m_expectedFileSize{ -1 };

    std::atomic<int> m_speedLimit{ 0 };

    // ---------------------------------------------------------------------
    // Worker state
    // ---------------------------------------------------------------------

    std::thread m_worker;

    std::atomic<bool> m_running{ false };

    std::atomic<bool> m_stopRequested{ false };

    std::atomic<bool> m_pauseRequested{ false };

    // ---------------------------------------------------------------------
    // Observer
    // ---------------------------------------------------------------------

    mutable std::mutex m_observerMutex;

    DownloaderObserverInterface* m_observer = nullptr;
};
