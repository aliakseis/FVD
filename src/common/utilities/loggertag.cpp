#include "loggertag.h"

#include <QIODevice>

namespace utilities::LoggerTag
{

class NullIODevice : public QIODevice
{
	qint64 readData(char* /*data*/, qint64 /*maxlen*/) override
	{
		return -1;
	}
	qint64 writeData(const char* /*data*/, qint64 len) override
	{
		return len;
	}
public:
    NullIODevice()
    {
        open(QIODevice::WriteOnly); // workaround
    }
};

static LoggerTagHandler* m_handler;

void setHandler(LoggerTagHandler* handler)
{
	m_handler = handler;
}

QDebug filter(const QString& tag)
{
    if (!m_handler || m_handler->hasTagId(tag))
    {
        auto debug = qDebug();
        debug << tag;
        return debug;
    }

	static NullIODevice nullIODevice;
	return {&nullIODevice};
}

} // namespace utilities
