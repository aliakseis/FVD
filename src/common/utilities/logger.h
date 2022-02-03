#pragma once

#include <QtGlobal>

namespace utilities
{

struct LoggerHandler
{
	virtual bool log(QtMsgType type, const QString& text) = 0;
};

void setWriteToLogFile(bool write_to_log_file);
void setLogHandler(LoggerHandler* handler);

} // namespace utilities
