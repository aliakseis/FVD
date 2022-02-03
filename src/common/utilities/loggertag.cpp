#include "loggertag.h"

#include <QIODevice>

namespace utilities
{

namespace LoggerTag
{

class NullIODevice : public QIODevice
{
	qint64 readData(char*, qint64) override
	{
		return -1;
	}
	qint64 writeData(const char*, qint64 len) override
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
	QtMsgType tagId = m_handler ? m_handler->getTagId(tag) : QtRejectedLoggerTagMsg;

	if (tagId != QtRejectedLoggerTagMsg)
	{
		return QDebug(tagId);
	}

	static NullIODevice nullIODevice;
	return QDebug(&nullIODevice);
}

} // namespace LoggerTag

} // namespace utilities
