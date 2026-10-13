#pragma once

#include <QtQuick/QQuickAsyncImageProvider>
#include <QtQuick/QQuickImageResponse>
#include <QtQuick/QQuickTextureFactory>

#include <QImage>
#include <QMutex>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>

#include "imagecache.h"

// Asynchronous replacement for the former QQuickImageProvider-based provider.
// Existing imageprovider:<id> URLs remain unchanged.
class QmlImageResponse final : public QQuickImageResponse
{
    //Q_OBJECT

public:
    explicit QmlImageResponse(const QString& id);
    ~QmlImageResponse() override = default;

    QQuickTextureFactory* textureFactory() const override;
    QString errorString() const override { return {}; }

private:
    friend class QmlImageLoadTask;

    void setImageAndFinish(const QImage& image);

    QImage m_image;
};

class QmlImageProvider final : public QQuickAsyncImageProvider
{
public:
    QmlImageProvider();

    QQuickImageResponse* requestImageResponse(
        const QString& id, const QSize& requestedSize) override;

private:
    static QThreadPool& imageThreadPool();
};
