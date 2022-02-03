#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>
#include <QStringList>
#include <QDateTime>

#include "entityfilenames.h"

class LibraryAddWorker :  public QObject, public QRunnable
{
	Q_OBJECT

public:
	LibraryAddWorker(EntityFileNames&& allEntitiesFiles);
	~LibraryAddWorker();

	void run() override;

signals:
	void libraryFileAdded(const QString& path);

private:
	EntityFileNames m_allEntitiesFiles;

	QDateTime m_startTime;

	void processAdding(const QString& path);
};
