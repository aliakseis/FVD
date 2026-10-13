#include "qmlimageprovider.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QThread>
#include <QDebug>

#include <utility>

namespace
{
QThreadPool& imageThreadPool()
{
    static QThreadPool pool;
    static const bool configured = []()
    {
        pool.setMaxThreadCount(qMax(2, qMin(4, QThread::idealThreadCount())));
        pool.setExpiryTimeout(30000);
        return true;
    }();
    Q_UNUSED(configured)
    return pool;
}

QImage loadImage(const QString& originalId)
{
    QImage image;

    // Keep the old provider's URL convention. The HTTP branch was already
    // anticipated in the original implementation but had been disabled.
    ImageCache cache;
    if (originalId.startsWith(QLatin1String("http"), Qt::CaseInsensitive))
    {
        image = cache.getSync(QString(), originalId, QString());
    }
    else
    {
        const QByteArray decoded = QByteArray::fromPercentEncoding(originalId.toUtf8());
        const QString id = QString::fromUtf8(decoded);
        image = cache.getSync(id, QString(), id);
    }

    if (image.isNull())
        image = QImage(QStringLiteral(":/images/fvd_banner.png"));

    return image;
}
} // namespace

class QmlImageLoadTask final : public QRunnable
{
public:
    QmlImageLoadTask(QString id, QmlImageResponse* response)
        : m_id(std::move(id)), m_response(response)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QImage image = loadImage(m_id);

        // The response may have been discarded if its QML Image was removed.
        // Queue the result to the response's thread; never update QML-facing
        // state from the worker thread.
        const QPointer<QmlImageResponse> response = m_response;
        if (!response)
            return;

        QMetaObject::invokeMethod(
            response.data(),
            [response, image = std::move(image)]() mutable
            {
                if (response)
                    response->setImageAndFinish(image);
            },
            Qt::QueuedConnection);
    }

private:
    QString m_id;
    QPointer<QmlImageResponse> m_response;
};

QmlImageResponse::QmlImageResponse(const QString& id)
{
    imageThreadPool().start(new QmlImageLoadTask(id, this));
}

QQuickTextureFactory* QmlImageResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void QmlImageResponse::setImageAndFinish(const QImage& image)
{
    m_image = image;
    emit finished();
}

QmlImageProvider::QmlImageProvider()
    : QQuickAsyncImageProvider()
{
}

QQuickImageResponse* QmlImageProvider::requestImageResponse(
    const QString& id, const QSize& requestedSize)
{
    Q_UNUSED(requestedSize)
    return new QmlImageResponse(id);
}
