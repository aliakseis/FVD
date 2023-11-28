#pragma once

#include <QDebug>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QMutex>

#define TAG(tag) ::utilities::LoggerTag::filter(tag)

namespace utilities
{

namespace LoggerTag
{

struct LoggerTagHandler
{
	virtual bool hasTagId(const QString& tag) = 0;
};

QDebug filter(const QString& tag);
void setHandler(LoggerTagHandler*);

};

} // utilites namespace
