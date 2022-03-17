#pragma once

#include "utilities/singleton.h"

#include <QEventLoop>
#include <QImage>
#include <QObject>
#include <QNetworkAccessManager>

#include <map>

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
	ImageCache(void);
	~ImageCache(void);

	void getAsync(const QString& id, const QString& url, const QString& filePath);
	QImage getSync(const QString& id, const QString& url, const QString& filePath);

	static bool clear(const QString& id);
	static void clearAll();

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

    std::map<QString, QDateTime> m_missingLastModified;

	void get(const QString& id, const QString& url, const QString& filePath);

	static QString fileNameFromId(QString id);

	static const QString imagesCacheDir;
};
