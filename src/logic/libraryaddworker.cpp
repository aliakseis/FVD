#include "libraryaddworker.h"

#include <QDebug>
#include <QDir>
#include <utility>

#include "global_functions.h"
#include "settings_declaration.h"

LibraryAddWorker::LibraryAddWorker(EntityFileNames&& allEntitiesFiles)
    : m_allEntitiesFiles(std::move(allEntitiesFiles)), m_startTime(QDateTime::currentDateTime())
{
}

LibraryAddWorker::~LibraryAddWorker() { qDebug() << __FUNCTION__; }

void LibraryAddWorker::run()
{
    if (global_functions::SaveVideoPathHasWildcards())
    {
        QString strSite = QSettings().value(app_settings::Sites, app_settings::Sites_Default).toString();
        QStringList listSites = strSite.split(";", QString::SkipEmptyParts);
        foreach (const QString& str, listSites)
        {
            processAdding(global_functions::getSaveFolder(str));
        }
    }
    else
    {
        processAdding(global_functions::GetVideoFolder());
    }
}

void LibraryAddWorker::processAdding(const QString& path)
{
    QDir dir = path;
    const QStringList videoFilesMask = global_functions::GetVideoFileExts();
    for (const auto& fileInfo : dir.entryInfoList(videoFilesMask, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot))
    {
        if (fileInfo.created() < m_startTime && fileInfo.lastModified() < m_startTime &&
            (fileInfo.size() != 0 || fileInfo.isDir()))
        {
            QString fileName = QDir::toNativeSeparators(fileInfo.filePath());
            QString canonicalFileName = QDir::toNativeSeparators(fileInfo.canonicalFilePath());
            if (m_allEntitiesFiles.find(fileName) == m_allEntitiesFiles.end() &&
                m_allEntitiesFiles.find(canonicalFileName) == m_allEntitiesFiles.end())
            {
                emit libraryFileAdded(fileName);
            }
        }
    }
}
