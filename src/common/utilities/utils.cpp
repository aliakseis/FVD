#include "utils.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <winsock2.h>
#include <winnt.h>
#include <shellapi.h>
#elif defined(Q_OS_LINUX)
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET_ERROR (-1)
#elif defined(Q_OS_DARWIN)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include "darwin/AppHandler.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET_ERROR (-1)
#endif

#include "modelserializer.h"
#include "modeldeserializer.h"

#include <algorithm>
#include <iterator>
#include <QObject>
#include <QStringList>
#include <QDir>
#include <QFont>
#include <QByteArray>
#include <QNetworkReply>
#include <QUrl>
#include <QMetaProperty>
#include <QDebug>
#include <QCoreApplication>
#include <QApplication>
#include <QMainWindow>
#include <set>
#include <map>


#include "logger.h"

namespace {

int getEscape(const QChar* uc, int* pos, int len, int maxNumber = 999)
{
    int i = *pos;
    ++i;
    if (i < len && uc[i] == QLatin1Char('L'))
    {
        ++i;
    }
    if (i < len)
    {
        int escape = uc[i].unicode() - '0';
        if (uint(escape) >= 10U)
        {
            return -1;
        }
        ++i;
        while (i < len)
        {
            int digit = uc[i].unicode() - '0';
            if (uint(digit) >= 10U)
            {
                break;
            }
            escape = (escape * 10) + digit;
            ++i;
        }
        if (escape <= maxNumber)
        {
            *pos = i;
            return escape;
        }
    }
    return -1;
}

bool isPortAvalible(unsigned short int dwPort, int type)
{
    sockaddr_in client{};

    client.sin_family = AF_INET;
    client.sin_port = htons(dwPort);
    client.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    auto sock = socket(AF_INET, type, 0);
    int result = bind(sock, (sockaddr*)&client, sizeof(client));
#ifdef Q_OS_WIN
    closesocket(sock);
#else
    close(sock);
#endif
    return result != SOCKET_ERROR;
}

} // namespace

namespace utilities
{


void InitializeProjectDescription()
{
    static struct Initializer
    {
        Initializer()
        {
            QCoreApplication::setApplicationVersion(BRAND_VERSION);
            QCoreApplication::setApplicationName(PROJECT_NAME);
            QCoreApplication::setOrganizationName(PROJECT_NAME);
            QCoreApplication::setOrganizationDomain(PROJECT_DOMAIN);
        }
    } initializer;
}

QFont GetAdaptedFont(int size, int additional_amount)
{
    Q_UNUSED(additional_amount)
#ifdef Q_OS_DARWIN
    QFont f("Lucida Grande");
    f.setPixelSize(size + additional_amount);
    return f;
#else
    const float koef = 4 / 3.F;
    QFont f("Segoe UI");
    f.setPixelSize(size * koef);
    return f;
#endif
}

///////////////////////////////////////////////////////////////////////////////


bool DeserializeObject(QXmlStreamReader* stream, QObject* object, const QString& name)
{
    Q_ASSERT(stream);
    ModelDeserializer deserializer(*stream);
    return deserializer.deserialize(object, name);
}

void SerializeObject(QXmlStreamWriter* stream, QObject* object, const QString& name)
{
    Q_ASSERT(stream);
    ModelSerializer serializer(*stream);
    serializer.serialize(object, name);
}


///////////////////////////////////////////////////////////////////////////////

void PrintReplyHeader(QNetworkReply* reply)
{
    qDebug() << "========Headers========";
    auto list = reply->rawHeaderList();
    for (auto & it : list)
    {
        qDebug() << QString::fromLatin1(it) << ": " << QString::fromLatin1(reply->rawHeader(it));
    }
    qDebug() << "=======================";
}

QString SizeToString(quint64 size, int precision, int fieldWidth)
{
    const unsigned int Kbytes_limit = 1 << 10; //1 Kb
    const unsigned int Mbytes_limit = 1 << 20; //1 Mb
    const unsigned int Gbytes_limit = 1 << 30; //1 Gb

    if (size < Kbytes_limit)
    {
        return QStringLiteral("%1 B").arg(size);
    }
    if (size < Mbytes_limit)
    {
        const double sizef = size / static_cast<double>(Kbytes_limit);
        return QStringLiteral("%1 kB").arg(sizef, fieldWidth, 'f', precision);
    }
    if (size < Gbytes_limit)
    {
        const double sizef = size / static_cast<double>(Mbytes_limit);
        return QStringLiteral("%1 MB").arg(sizef, fieldWidth, 'f', precision);
    }

    const double sizef = size / static_cast<double>(Gbytes_limit);
    return QStringLiteral("%1 GB").arg(sizef, fieldWidth, 'f', precision);
}

bool IsAsyncUrl(const QString& path)
{
    QString s(path.toLower().trimmed());
    return s.contains(QRegExp("^https?://")) || s.startsWith("qrc:/");
}

QString secondsToString(long seconds)
{
    QString result;
    const int s = seconds % 60;
    const int m = (seconds / 60) % 60;
    const int h = seconds / 3600;

    if (h > 0)
    {
        result = "%3:%2:%1";
        return result.arg(s, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(h, 2, 10, QChar('0'));
    }

    result = "%2:%1";
    return result.arg(s, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0'));
}

QString ProgressString(double progress)
{
    // We don't want to display 100% unless the torrent is really complete
    if (progress > 99.9 && progress < 100.)
    {
        progress = 99.9;
    }

    return
        (progress <= 0) ? "0%" : ((progress >= 100) ? "100%" : QString("%1%").arg(progress, 0, 'f', 1));
}

// shamelessly stolen from qstring.cpp
QString multiArg(const QString& str, int numArgs, const QString* args)
{
    QString result;
    QMap<int, int> numbersUsed;
    const QChar* uc = str.constData();
    const int len = str.size();
    const int end = len - 1;
    int lastNumber = -1;
    int i = 0;

    // populate the numbersUsed map with the %n's that actually occur in the string
    while (i < end)
    {
        if (uc[i] == QLatin1Char('%'))
        {
            int number = getEscape(uc, &i, len);
            if (number != -1)
            {
                numbersUsed.insert(number, -1);
                continue;
            }
        }
        ++i;
    }

    // concatenate if no placeholders
    if (numbersUsed.empty())
    {
        result = str;
        for (int i = 0; i < numArgs; ++i)
            result += args[i];
        return result;
    }

    // assign an argument number to each of the %n's
    QMap<int, int>::iterator j = numbersUsed.begin();
    QMap<int, int>::iterator jend = numbersUsed.end();
    int arg = 0;
    while (j != jend && arg < numArgs)
    {
        *j = arg++;
        lastNumber = j.key();
        ++j;
    }

    // sanity
    if (numArgs > arg)
    {
        qWarning("QString::arg: %d argument(s) missing in %s", numArgs - arg, str.toLocal8Bit().data());
        numArgs = arg;
    }

    i = 0;
    while (i < len)
    {
        if (uc[i] == QLatin1Char('%') && i != end)
        {
            int number = getEscape(uc, &i, len, lastNumber);
            int arg = numbersUsed[number];
            if (number != -1 && arg != -1)
            {
                result += args[arg];
                continue;
            }
        }
        result += uc[i++];
    }
    return result;
}


bool CheckPortAvailable(int targetPort, const char** reason)
{
    if (targetPort < 1 || targetPort > 0xffff)
    {
        if (reason)
        {
            *reason = "Port is out bounds.";
        }
        return false;
    }

    if (!isPortAvalible(targetPort, SOCK_STREAM))
    {
        if (reason)
        {
            *reason = "This TCP port is already in use.";
        }
        return false;
    }

    if (!isPortAvalible(targetPort, SOCK_DGRAM))
    {
        if (reason)
        {
            *reason = "This UDP port is already in use.";
        }
        return false;
    }

    return true;
}


QMainWindow* getMainWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (auto *mainWindow = qobject_cast<QMainWindow*>(widget)) {
            return mainWindow;
        }
    }
    return nullptr;
}

} // namespace utilities
