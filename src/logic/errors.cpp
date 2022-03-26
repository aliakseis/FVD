#include "errors.h"

#include <QObject>

std::map<Errors::Code, QString> Errors::m_errors;

QString Errors::description(Code code)
{
    if (m_errors.empty())
    {
        m_errors[FailedToExtractLinks] = QObject::tr("Failed to extract links");
        m_errors[NetworkError] = QObject::tr("Network error");
    }
    return (m_errors.find(code) != m_errors.end() ? m_errors[code] : QString("errorcode: %1").arg((int)code));
}
