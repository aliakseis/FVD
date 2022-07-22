#include "downloadentity.h"

#include <QDir>
#include <QMessageBox>

#include "branding.hxx"
#include "global_functions.h"
#include "globals.h"
#include "searchmanager.h"
#include "settings_declaration.h"
#include "strategiescommon.h"
#include "utilities/instantiator.h"
#include "utilities/logger.h"
#include "utilities/translation.h"

#include "player/ffmpegdecoder.h"

#include <cmath>
#include <cctype>
#include <algorithm>

static double getVideoDuration(const QString& file)
{
    FFmpegDecoder decoder;
    return decoder.getDuration(file);
}

const QDateTime nullDateTime;

REGISTER_QOBJECT_METATYPE(DownloadEntity)

DownloadEntity::DownloadEntity(RemoteVideoEntity* parentVideoEntity /* = 0*/, VisibilityState vState /* = visNorm*/,
                               bool isFileAssigned /* = true*/)
    : QObject(parentVideoEntity),
      m_state(kQueued),
      m_currentResolutionId(-1),
      m_downloadedSize(0),
      m_parentVideoEntity(parentVideoEntity),
      m_visibilityState(vState),  // assume we create it on download
      m_isFailed(false),
      m_bytesPerSecond(0),
      m_lastModified(nullDateTime),
      m_totalFileSize(0),
      m_isFileAssigned(isFileAssigned)
{
}

void DownloadEntity::onProgress(qint64 bytesDownloaded)
{
    if (bytesDownloaded > 0)
    {
        m_isFailed = false;
    }
    m_downloadedSize = bytesDownloaded;
    emit progressChanged(bytesDownloaded, downloader()->totalFileSize());
}

void DownloadEntity::onSpeed(qint64 bytesPerSecond)
{
    // if for any reason bytes_per_second is negative, keep previous speed
    if (bytesPerSecond > 0)
    {
        if (totalFileSize() > m_downloadedSize)
        {
            m_bytesPerSecond = bytesPerSecond;
            m_speed = utilities::SizeToString(bytesPerSecond, 2) + "/s";
        }
        else
        {
            m_speed.clear();
            m_bytesPerSecond = 0;
        }
    }
    bytesPerSecond *= static_cast<long long>(downloader()->totalFileSize() > m_downloadedSize);
    emit speed(bytesPerSecond);
}

void DownloadEntity::onFinished()
{
    if (getParent()->m_videoInfo.duration == 0)
    {
        getParent()->m_videoInfo.duration = std::lround(getVideoDuration(filename()));
    }

    setState(kFinished);
    emit finished();
}

void DownloadEntity::onStart(const QByteArray& data)
{
    auto pData = data.begin();
    const auto pDataEnd = data.end();
    while ((pData = std::find_if(pData, pDataEnd, [](char ch) {
        return std::isdigit(static_cast<unsigned char>(ch));
        })) != pDataEnd)
    {
        const auto localEnd = std::find_if(pData, pDataEnd, [](char ch) {
            return ch != '/' && !std::isdigit(static_cast<unsigned char>(ch));
        });

        if (localEnd - pData == 10)
        {
            QLatin1String s(&*pData, &*localEnd);
            const auto published = QDateTime::fromString(s, QStringLiteral("MM/dd/yyyy"));
            if (published.isValid())
            {
                getParent()->m_videoInfo.published = published;
                return;
            }
        }

        if (localEnd == pDataEnd) {
            break;
        }

        pData = std::next(localEnd);
    }
}

void DownloadEntity::onFileCreated(const QString& filename)
{
    downloader()->setDownloadNamePolicy(DownloaderType::kReplaceFile);

    m_filepath = QDir::toNativeSeparators(filename);
    emit fileCreated(filename);
}

void DownloadEntity::onError(ErrorCode::ERROR_CODES code, const QString& error)
{
    setState(kFailed);
    qDebug() << __FUNCTION__ << (error.isEmpty() ? utilities::ErrorCode::Instance().getDescription(code).key : error)
             << " code:" << code;
    emit errorHappened(code, error);
}

void DownloadEntity::doSetup(const LinkInfo& linkInfo)
{
    m_filepath = m_parentVideoEntity->m_videoInfo.videoTitle;

    const QRegExp filenameEnemies(R"([\"@&~=\n\t\/:?#!|<>*^\\])");

    m_filepath.replace(filenameEnemies, "_");

    QString savePath = global_functions::getSaveFolder(strategyName());
    if (!savePath.endsWith(QDir::separator()))
    {
        savePath += QDir::separator();
    }
    m_filepath =
        savePath + m_filepath + "(" + QString(linkInfo.resolution).remove(filenameEnemies) + ")." + linkInfo.extension;

    m_url = linkInfo.directLink;
    m_currentResolutionId = linkInfo.resolutionId;
    m_currentResolution = linkInfo.resolution;
    m_fileExtension = linkInfo.extension;
}

bool DownloadEntity::doDownload()
{
    if (m_downloadedSize > 0)
    {
        return doResume();
    }

    if (m_state != kFailed && m_state != kFinished && m_state != kQueued)
    {
        qDebug() << QString("Cannot start download task, it is in state %1").arg(m_state);
        return false;
    }

    if (!downloader()->setDestinationPath(global_functions::getSaveFolder(strategyName())))
    {
        setState(kFailed);
        emit errorHappened(utilities::ErrorCode::eDOWLDOPENFILERR,
                           "Cannot create " + global_functions::getSaveFolder(strategyName()));
        return false;
    }

    downloader()->Start(m_url, &TheQNetworkAccessManager::Instance(), QFileInfo(m_filepath).fileName());
    emit downloadStarted(m_url);
    if (m_state != kFailed)
    {
        setState(kDownloading);
    }
    return true;
}

void DownloadEntity::doPause()
{
    if (m_state != kDownloading)
    {
        return;
    }
    downloader()->Pause();
    setState(kPaused);
}

bool DownloadEntity::doResume()
{
    if (m_state != kPaused && m_state != kQueued)
    {
        return false;
    }
    if (QFile::exists(m_filepath))
    {
        downloader()->Resume(m_url, &TheQNetworkAccessManager::Instance(), QFileInfo(m_filepath).fileName());
        if (m_state != kFinished)  // it can happen so that all the file is downloaded already
        {
            setState(kDownloading);
            return true;
        }
    }
    else
    {
        setState(kFailed);
    }

    return false;
}

void DownloadEntity::doRemove()
{
    downloader()->Stop();
    m_downloadedSize = 0;
    setState(kQueued);
    if (visibilityState() != visTemp)
    {
        setVisibilityState(visLibOnly);
    }

    utilities::DeleteFileWithWaiting(m_filepath);
}

void DownloadEntity::doStop()
{
    if (m_state == kFinished)
    {
        return;
    }
    downloader()->Stop();
    m_downloadedSize = 0;
    if (m_state != kFailed)
    {
        setState(kCanceled);
    }
}

bool DownloadEntity::doRestart()
{
    if (m_state == kFinished)
    {
        emit fileRemoving();
        if (!utilities::DeleteFileWithWaiting(m_filepath))
        {
            qDebug() << __FUNCTION__ << ": file is locked: " << m_filepath;
            return false;
        }
        setVisibilityState(visNorm);
    }
    else if (m_state == kFailed || m_state == kQueued)
    {
        if (!m_url.isEmpty())
        {
            downloader()->Stop();
        }

        m_url = QString();
        getParent()->extractLinks();
    }
    else
    {
        downloader()->Stop();
    }
    m_downloadedSize = 0;
    setState(kQueued);
    return true;
}

QVariant DownloadEntity::progress() const
{
    return (m_downloadedSize > 0) ? m_downloadedSize * 100.F / totalFileSize() : QVariant(QVariant::Double);
}

void DownloadEntity::onFileToBeReleased(const QString& filename)
{
    Q_UNUSED(filename)
    emit fileRemoving();
}

QString DownloadEntity::stateToString(State s)
{
    switch (s)
    {
    case DownloadEntity::kQueued:
        return Tr::Tr(TREEVIEW_QUEUED_STATUS);
    case DownloadEntity::kDownloading:
        return Tr::Tr(TREEVIEW_DOWNLOADING_STATUS);
    case DownloadEntity::kPaused:
        return Tr::Tr(TREEVIEW_PAUSED_STATUS);
    case DownloadEntity::kFinished:
        return Tr::Tr(TREEVIEW_FINISHED_STATUS);
    case DownloadEntity::kFailed:
        return Tr::Tr(TREEVIEW_ERROR_STATUS);
    case DownloadEntity::kCanceled:
        return Tr::Tr(TREEVIEW_CANSELED_STATUS);
    }
    return {};
}

void DownloadEntity::doSetVisibilityState(VisibilityState value) { m_visibilityState = value; }

bool DownloadEntity::startDownloadWithHighestPriority()
{
    Q_ASSERT(m_visibilityState == visNorm);
    return m_state == kDownloading || m_state == kFinished || doDownload();
}

/* static */ bool DownloadEntity::confirmRestartDownload()
{
    return QMessageBox::question(
               nullptr, Tr::Tr(PROJECT_FULLNAME_TRANSLATION),
               QObject::tr("<b>NOTE: You have already downloaded this video. Would you like to download it again?</b>"),
               QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

void DownloadEntity::enqueueAndResetFailedState()
{
    m_isFailed = false;
    Downloadable::State previousState = m_state;
    m_state = kQueued;
    emit stateChanged(m_state, previousState);
}

DownloadEntity::DownloaderType* DownloadEntity::downloader()
{
    if (m_downloader.isNull())
    {
        m_downloader.reset(new DownloaderType(this));

        m_downloader->setObserver(this);
        m_downloader->setDownloadNamePolicy(m_isFileAssigned ? DownloaderType::kReplaceFile
                                                             : DownloaderType::kGenerateNewName);
#ifdef ALLOW_TRAFFIC_CONTROL
        m_downloader->setSpeedLimit(global_functions::GetTrafficLimitActual());
#endif  // ALLOW_TRAFFIC_CONTROL
        VERIFY(connect(this, SIGNAL(downloadStarted(const QString&)), &SearchManager::Instance(),
                       SLOT(onDownloadStarted(const QString&))));
        VERIFY(connect(this, SIGNAL(errorHappened(utilities::ErrorCode::ERROR_CODES, const QString&)),
                       &SearchManager::Instance(),
                       SLOT(onDownloadError(utilities::ErrorCode::ERROR_CODES, const QString&))));
        VERIFY(connect(this, SIGNAL(finished()), &SearchManager::Instance(), SLOT(onDownloadFinished())));

        m_downloader->setDestinationPath(m_destinationPath);
        m_downloader->setTotalFileSize(m_totalFileSize);
        m_downloader->setExpectedFileSize(m_totalFileSize);
    }

    return m_downloader.data();
}

void DownloadEntity::setState(State state)
{
    if (m_state != state)
    {
        if (kFailed == m_state)
        {
            m_isFailed = true;
        }
        Downloadable::State previousState = m_state;

        m_state = state;
        m_lastModified = nullDateTime;  // invalidate
        emit stateChanged(state, previousState);
    }
}
