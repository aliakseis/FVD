#include "logger.h"

#include <QFile>
#include <QTextStream>
#include <QDesktopServices>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QMutex>
#include <QThread>
#include <stdio.h>
#include <stdlib.h>

#include <qlogging.h>

#include "utilities/filesystem_utils.h"

namespace
{

const QString FILE_FORMAT = PROJECT_NAME"%1.txt";
const QString FIRST_FILE = PROJECT_NAME".txt";
const int MAX_FILE_SIZE = 1024 * 1024 * 5;

bool write_to_log_file_ = false;

QtMessageHandler previousMsgHandler = 0;
void messageOutput(QtMsgType type, const QMessageLogContext& context, const QString& message);

class Logger
{
public:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator =(const Logger&) = delete;

    void log(const QString& text);

    void InitializeLogFile();

    void messageOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg);

    void setLogHandler(utilities::LoggerHandler* handler) { m_handler = handler; }

private:
    QFile* log_file_;
    QTextStream* log_stream_;
    QMutex m_mutex;
    bool m_isRecursive;
    utilities::LoggerHandler* m_handler{};
};

Logger::Logger()
    : log_file_(nullptr)
    , log_stream_(nullptr)
    , m_mutex(QMutex::Recursive)
    , m_isRecursive(false)
{
}

Logger::~Logger()
{
    //    it causes weird crash
    //    if (log_file_)
    //        qDebug() << "--- Logging Stopped ---";
    qInstallMessageHandler(nullptr);

    delete log_stream_;
    delete log_file_;
}

void Logger::InitializeLogFile()
{
    QDir dir(utilities::PrepareCacheFolder(), QString(FILE_FORMAT).arg("*"), QDir::Size);
    QFileInfoList fileList = dir.entryInfoList();
    int i = 0;
    while (i < fileList.size() && fileList[i].size() >= MAX_FILE_SIZE) { ++i; }

    QString path;
    if (i < fileList.size())
    {
        path = fileList[i].absoluteFilePath();
    }
    else if (fileList.empty())
    {
        path = dir.absoluteFilePath(FIRST_FILE);
    }
    else
    {
        path = dir.absoluteFilePath(QString(FILE_FORMAT).arg(fileList.size()));
    }

    log_file_ = new QFile(path);
    Q_ASSERT(log_file_);
    log_file_->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
    log_stream_ = new QTextStream(log_file_);

    previousMsgHandler = qInstallMessageHandler(&::messageOutput);
    qDebug() << "--- Logging Started ---";
}

void Logger::log(const QString& text)
{
    if (write_to_log_file_)
    {
        QString record = '\n' + QDateTime::currentDateTimeUtc().toString("dd.MM.yy hh:mm:ss:zzz") + ": "
                         + QString::number((unsigned long long) QThread::currentThreadId()) + ' ' + text;

        QMutexLocker ml(&m_mutex);

        if (m_isRecursive)
        {
            return;    // Can we do anything here?
        }
        m_isRecursive = true;

        if (!log_file_)
        {
            InitializeLogFile();
        }
        *log_stream_ << record;
        log_stream_->flush();

        m_isRecursive = false; // Qt does not use exceptions
    }
}

void Logger::messageOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (m_handler != nullptr)
    {
        if (!m_handler->log(type, msg))
        {
            return;
        }
    }

    QString message;

    switch (type)
    {
    case QtInfoMsg:
        message = "[Info] ";
        break;
    case QtDebugMsg:
        message = "[Debug] ";
        break;
    case QtWarningMsg:
        message = "[Warning] ";
        break;
    case QtCriticalMsg:
        message = "[Critical] ";
        break;
    case QtFatalMsg:
        message = "[Fatal] ";
    }

    message += msg;

    log(message);

    if (type == QtFatalMsg)
    {
        qInstallMessageHandler(0);
        qt_message_output(type, context, msg);
        //abort();
    }
    else
    {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
        fflush(stderr);
        if (previousMsgHandler != 0)
        {
            previousMsgHandler(type, context, msg);
        }
    }
}


Logger logger;


void messageOutput(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    logger.messageOutput(type, context, message);
}

} // namespace


namespace utilities
{
    
void setWriteToLogFile(bool write_to_log_file)
{
    if (!write_to_log_file_)
    {
        write_to_log_file_ = write_to_log_file;
        if (write_to_log_file)
        {
            logger.InitializeLogFile();
        }
    }
}

void setLogHandler(LoggerHandler* handler)
{
    logger.setLogHandler(handler);
}

} // namespace utilities
