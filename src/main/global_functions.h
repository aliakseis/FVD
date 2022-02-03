#pragma once

#include <QStringList>

namespace global_functions
{

QStringList GetVideoFileExts();

QString getSaveFolder(QString folderName, const QString& strategyName);

QString GetVideoFolder();

inline QString getSaveFolder(const QString& strategyName)
{
    return getSaveFolder(GetVideoFolder(), strategyName);
}

int GetTrafficLimitActual();

bool SaveVideoPathHasWildcards();

} // namespace global_functions
