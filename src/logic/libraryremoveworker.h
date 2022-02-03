#pragma once

#include <QObject>
#include <QRunnable>
#include <QMap>
#include <QString>
#include <QPointer>

class DownloadEntity;

class LibraryRemoveWorker :  public QObject, public QRunnable
{
	Q_OBJECT

public:
	LibraryRemoveWorker(const QMap<QString, QPointer<DownloadEntity> >& libraryEntities)
		: m_libraryEntities(libraryEntities)
	{
	}
	~LibraryRemoveWorker();

	void run() override;

signals:
	void libraryFileDeleted(const QPointer<DownloadEntity>& entity);

private:
	QMap<QString, QPointer<DownloadEntity> > m_libraryEntities;
};
