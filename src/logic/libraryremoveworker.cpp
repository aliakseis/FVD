#include "libraryremoveworker.h"

#include <QFile>
#include <QDebug>

LibraryRemoveWorker::~LibraryRemoveWorker()
{
	qDebug() << __FUNCTION__;
}

void LibraryRemoveWorker::run()
{
	for (auto it(m_libraryEntities.begin()), itEnd(m_libraryEntities.end()); it != itEnd; ++it)
	{
		if (!QFile::exists(it.key()))
		{
			emit libraryFileDeleted(it.value());
		}
	}
}
