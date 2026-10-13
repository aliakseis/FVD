#pragma once

#include <QEventLoop>
#include <QImage>
#include <QNetworkAccessManager>
#include <QObject>

#include "utilities/singleton.h"

class FileMissingSignaller : public QObject, public Singleton<FileMissingSignaller>
{
    Q_OBJECT

    friend class Singleton<FileMissingSignaller>;
    friend class ImageCache;

Q_SIGNALS:
    void fileMissing(const QString& filePath);

private:
    FileMissingSignaller() = default;
};

const char GET_RANDOM_FRAME_OPTION[] = "-getRandomFrame";

int getRandomFrame();

class ImageCache : public QObject
{
    Q_OBJECT

public:
    ImageCache();
    ~ImageCache();

    void getAsync(const QString& id, const QString& url, const QString& filePath);
    QImage getSync(const QString& id, const QString& url, const QString& filePath);

Q_SIGNALS:
    void getImageFinished(QImage image);

private Q_SLOTS:
    void imageReceived(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_networkImageManager;
    QEventLoop m_eventLoop;
    bool m_isAsync;
    QNetworkReply* m_lastReply;
    QImage m_image;

    void get(const QString& id, const QString& url, const QString& filePath);
};
