#pragma once

#include <QDebug>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QMutex>

#define TAG(tag) ::utilities::LoggerTag::filter(tag)
const QtMsgType QtRejectedLoggerTagMsg = (QtMsgType)99;
const QtMsgType QtLoggerTagMsg = (QtMsgType)100;

namespace utilities
{

namespace LoggerTag
{

struct LoggerTagHandler
{
	virtual QtMsgType getTagId(const QString& tag) = 0;
};

QDebug filter(const QString& tag);
void setHandler(LoggerTagHandler*);

};

} // utilites namespace
