#pragma once

#include <QtQuick/QQuickImageProvider>
#define QDeclarativeImageProvider QQuickImageProvider

#include <QDebug>
#include <QLabel>
#include "imagecache.h"


class QmlImageProvider : public QDeclarativeImageProvider
{
public:
	QmlImageProvider()
		: QDeclarativeImageProvider(QDeclarativeImageProvider::Image)
	{
	}
	QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize)
	{
		Q_UNUSED(size)
		Q_UNUSED(requestedSize)
		QImage img;

		if (id.startsWith("http"))
		{
			Q_ASSERT(false);    //img = imageCache.getSync("", id, "");
		}
		else
		{
			QByteArray decoded(QByteArray::fromPercentEncoding(id.toUtf8()));
			QString id(QString::fromUtf8(decoded));
			img = imageCache.getSync(id, "", id);
		}

		if (img.isNull())
		{
			return QImage(":/images/fvd_banner.png");
		}

		return img;
	}
	ImageCache imageCache;
};
