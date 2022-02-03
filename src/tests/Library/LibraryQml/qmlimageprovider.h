#pragma once

#include <QDeclarativeImageProvider>
#include <QDebug>
#include <QLabel>
#include "ffmpegdecoder.h"

class QmlImageProvider : public QDeclarativeImageProvider
{
public:
	QmlImageProvider()
		: QDeclarativeImageProvider(QDeclarativeImageProvider::Image),
		  dec(nullptr)
	{
	}

	QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize)
	{
		qDebug() << "requestImage:" << id;
		QImage res;


		if (dec == nullptr)
		{
			dec = new FFmpegDecoder();
		}

		return dec->getRandomFrame(id);
	}
	FFmpegDecoder* dec;
};
