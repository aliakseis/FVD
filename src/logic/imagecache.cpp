#include "imagecache.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <algorithm>

#include "global_functions.h"
#include "player/ffmpegdecoder.h"
#include "settings_declaration.h"
#include "utilities/filesystem_utils.h"
#include "utilities/utils.h"

static int doGetRandomFrame(const QString& filePath, const QString& imageFilePath)
{
    if (imageFilePath.isEmpty())
    {
        return 1;
    }

    if (QDir(filePath).exists())
    {
        const auto videoFilesMask = global_functions::GetVideoFileExts();
        QDirIterator it(filePath, videoFilesMask, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            auto p = it.next();
            FFmpegDecoder decoder;
            auto image = decoder.getRandomFrame(p);
            if (!image.isEmpty())
            {
                QFile imageFile(imageFilePath);
                if (imageFile.open(QIODevice::WriteOnly))
                {
                    imageFile.write(image);
                    imageFile.close();
                    return 0;
                }
            }
        }
    }
    else if (QFile::exists(filePath))
    {
        FFmpegDecoder decoder;
        auto image = decoder.getRandomFrame(filePath);
        if (!image.isEmpty())
        {
            QFile imageFile(imageFilePath);
            if (imageFile.open(QIODevice::WriteOnly))
            {
                imageFile.write(image);
                imageFile.close();
                return 0;
            }
        }
    }

    return 1;
}

int getRandomFrame() { return doGetRandomFrame(QApplication::arguments().at(2), QApplication::arguments().at(3)); }

const QString ImageCache::imagesCacheDir = "images";

const char propImageFilePath[] = "imageFilePath";

ImageCache::ImageCache() : m_networkImageManager(nullptr), m_isAsync(false), m_lastReply(nullptr) {}

ImageCache::~ImageCache() = default;

void ImageCache::getAsync(const QString& id, const QString& url, const QString& filePath)
{
    m_isAsync = true;
    get(id, url, filePath);
}

QImage ImageCache::getSync(const QString& id, const QString& url, const QString& filePath)
{
    m_isAsync = false;
    get(id, url, filePath);
    return m_image;
}

void ImageCache::get(const QString& id, const QString& url, const QString& filePath)
{
    QString imageFilePath;

    if (!id.isEmpty())
    {
        imageFilePath = utilities::PrepareCacheFolder(imagesCacheDir) + fileNameFromId(id);
        if (QFile::exists(imageFilePath))
        {
            if (m_image.load(imageFilePath))
            {
                if (m_isAsync)
                {
                    emit getImageFinished(m_image);
                }
                return;
            }
        }
    }
    if (!url.isEmpty())
    {
        if (nullptr == m_networkImageManager)
        {
            m_networkImageManager = new QNetworkAccessManager(this);
            VERIFY(
                connect(m_networkImageManager, SIGNAL(finished(QNetworkReply*)), SLOT(imageReceived(QNetworkReply*))));
        }
        m_lastReply = m_networkImageManager->get(QNetworkRequest(url));
        if (!imageFilePath.isEmpty())
        {
            m_lastReply->setProperty(propImageFilePath, imageFilePath);
        }
        if (!m_isAsync)
        {
            m_eventLoop.exec();
        }
        return;
    }
    if (!filePath.isEmpty())
    {
        if (QFile::exists(filePath))
        {
            auto it = m_missingLastModified.find(id);
            if (it != m_missingLastModified.end() && it->second <= QFileInfo(filePath).lastModified())
            {
                QImage().swap(m_image);  // reset
            }
            else
            {
                //*
                const int status =
                    QProcess::execute(QCoreApplication::applicationFilePath(),
                                      QStringList() << GET_RANDOM_FRAME_OPTION << filePath << imageFilePath);
                //*/
                // const int status = doGetRandomFrame(filePath, imageFilePath);
                if (0 == status)
                {
                    m_image.load(imageFilePath);
                    m_missingLastModified.erase(id);
                }
                else
                {
                    QImage().swap(m_image);  // reset
                    m_missingLastModified.insert({id, QFileInfo(filePath).lastModified()});
                }
            }
        }
        else
        {
            emit FileMissingSignaller::Instance().fileMissing(filePath);
        }
    }
    if (m_isAsync)
    {
        emit getImageFinished(m_image);
    }
}

/* static */ QString ImageCache::fileNameFromId(QString id)
{
    return id.remove('\"').replace(QRegExp("[/\\\\:*?<>|]"), "_").right(80) + ".jpg";
}

void ImageCache::imageReceived(QNetworkReply* reply)
{
    const quint64 MAX_SIZE = 1024 * 1024;
    QByteArray imageArray(reply->read(MAX_SIZE));
    const bool valid = !imageArray.isEmpty() && (uint)imageArray.size() < MAX_SIZE && m_image.loadFromData(imageArray);
    auto imageFilePath = reply->property(propImageFilePath).value<QString>();
    if (valid)
    {
        if (!imageFilePath.isEmpty())
        {
            m_image.save(imageFilePath, "jpg");
        }
    }
    else
    {
        QImage cleanImage;
        m_image.swap(cleanImage);  // reset

        imageArray.truncate(512);
        qDebug() << "Incorrect image data:" << imageArray;
        utilities::PrintReplyHeader(reply);
    }

    if (m_isAsync && (m_lastReply == reply))
    {
        emit getImageFinished(m_image);
    }
    else
    {
        m_eventLoop.quit();
    }
}

/* static */ bool ImageCache::clear(const QString& id)
{
    return QFile::remove(utilities::PrepareCacheFolder(imagesCacheDir) + fileNameFromId(id));
}

/* static */ void ImageCache::clearAll() { QDir(utilities::PrepareCacheFolder(imagesCacheDir)).removeRecursively(); }
