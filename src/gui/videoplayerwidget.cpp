#include "videoplayerwidget.h"

#include <QDesktopServices>
#include <QMessageBox>
#include <QPointer>
#include <QResizeEvent>

#include "descriptionpanel.h"
#include "downloadentity.h"
#include "global_functions.h"
#include "mainwindow.h"
#include "player/ffmpegdecoder.h"
#include "playerheader.h"
#include "spinner.h"
#include "videocontrol.h"
#include "videoprogressbar.h"
#include "videowidget.h"

VideoPlayerWidget* VideoPlayerWidgetInstance()
{
    if (auto mainWindow = MainWindow::Instance())
    {
        return mainWindow->getPlayer();
    }
    return nullptr;
}

bool fileIsInUse(VideoPlayerWidget* videoPlayer, DownloadEntity* entity)
{
    if (nullptr == videoPlayer->currentDownload() || nullptr == entity)
    {
        return false;
    }
    if (videoPlayer->currentDownload() == entity)
    {
        return true;
    }

    return QFileInfo(videoPlayer->currentFilename()) == QFileInfo(entity->filename());
}

enum
{
    PROGRESSBAR_VISIBLE_HEIGHT = 5
};

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent)
    : QFrame(parent),
      m_descriptionPanel(nullptr),
      m_controls(nullptr),
      m_progressBar(nullptr),
      m_currentDownload(nullptr),
      m_spinner(nullptr),
      m_videoWidget(new VideoWidget(this))
{
    qRegisterMetaType<QPointer<DownloadEntity> >("QPointer<DownloadEntity>");
    VERIFY(connect(this, SIGNAL(playDownloadEntityAsynchronously(const QPointer<DownloadEntity>&)),
                   SLOT(onPlayDownloadEntityAsynchronously(const QPointer<DownloadEntity>&)), Qt::QueuedConnection));
    VERIFY(connect(getDecoder(), SIGNAL(fileReleased(bool)), SLOT(updateViewOnVideoStop(bool))));
    VERIFY(connect(getDecoder(), SIGNAL(playingFinished()), SLOT(onPlayingFinished())));
    VERIFY(connect(getDecoder(), SIGNAL(fileProbablyNotFull()), SLOT(disableSeeking())));
    VERIFY(connect(getDecoder(), SIGNAL(downloadPendingStarted()), SLOT(showSpinner())));
    VERIFY(connect(getDecoder(), SIGNAL(downloadPendingFinished()), SLOT(hideSpinner())));

    getDecoder()->setDownloadBufferingAwait(1);

    setDisplay(m_videoWidget);
    m_spinner = new Spinner(m_videoWidget);
    m_spinner->hide();
    m_spinner->setObjectName("bufferingSpinner");
    m_videoWidget->installEventFilter(this);
}

void VideoPlayerWidget::setVideoFilename(const QString& fileName) { m_currentFile = fileName; }

VideoDisplay* VideoPlayerWidget::getCurrentDisplay() { return VideoPlayer::getCurrentDisplay(); }

void VideoPlayerWidget::processPreviewEntity()
{
    qDebug() << "--- VideoPlayerWidget::processPreviewEntity ---";
    Q_ASSERT(m_progressBar != nullptr);
    setState(PendingHeader);
    m_progressBar->seekingEnable(true);

    switch (m_currentDownload->state())
    {
    case DownloadEntity::kQueued:
        VERIFY(connect(m_currentDownload, SIGNAL(fileCreated(QString)), SLOT(setVideoFilename(QString))));
        VERIFY(connect(m_currentDownload, SIGNAL(progressChanged(qint64, qint64)),
                       SLOT(downloadingToPreview(qint64, qint64))));
        VERIFY(connect(m_currentDownload, SIGNAL(progressChanged(qint64, qint64)), getDecoder(),
                       SLOT(limitPlayback(qint64, qint64))));
        VERIFY(connect(m_currentDownload, SIGNAL(finished()), getDecoder(), SLOT(resetLimitPlayback())));
        VERIFY(connect(m_currentDownload, SIGNAL(stateChanged(Downloadable::State, Downloadable::State)),
                       SLOT(onDownloadStateChanged(Downloadable::State, Downloadable::State))));
        break;
    case DownloadEntity::kFinished:
        m_progressBar->displayDownloadProgress(m_currentDownload->totalFileSize(), m_currentDownload->totalFileSize());
        playFile(m_currentDownload->filename());
        break;
    case DownloadEntity::kDownloading:
    case DownloadEntity::kPaused:
        VERIFY(connect(m_currentDownload, SIGNAL(progressChanged(qint64, qint64)),
                       SLOT(downloadingToPreview(qint64, qint64))));
        VERIFY(connect(m_currentDownload, SIGNAL(progressChanged(qint64, qint64)), getDecoder(),
                       SLOT(limitPlayback(qint64, qint64))));
        VERIFY(connect(m_currentDownload, SIGNAL(finished()), getDecoder(), SLOT(resetLimitPlayback())));
        break;
    case DownloadEntity::kFailed:
        setState(InitialState);
        hideSpinner();
        emit showPlaybutton(true);
        break;
    }
}

void VideoPlayerWidget::downloadingToPreview(qint64 bytesReceived, qint64 bytesTotal)
{
    double factor = 0.02;  // 2%
    if (bytesTotal < 1024 * 1024 / 2)
    {
        factor = 0.5;
    }
    else if (bytesTotal < 1 * 1024 * 1024)
    {
        factor = 0.2;
    }
    else if (bytesTotal < 5 * 1024 * 1024)
    {
        factor = 0.1;
    }
    else if (bytesTotal < 10 * 1024 * 1024)
    {
        factor = 0.05;
    }
    else if (bytesTotal < 50 * 1024 * 1024)
    {
        factor = 0.04;
    }
    else if (bytesTotal < 100 * 1024 * 1024)
    {
        factor = 0.03;
    }

    if (bytesReceived > bytesTotal * factor)  // 1% startup
    {
        qDebug() << "Opening file: " << m_currentFile << " at " << bytesReceived << " byte of " << bytesTotal
                 << " bytes";
        VERIFY(disconnect((QObject*)sender(), SIGNAL(progressChanged(qint64, qint64)), this,
                          SLOT(downloadingToPreview(qint64, qint64))));
        qDebug() << "File open with ffmpeg decoder...";
        playFile(m_currentFile);
    }
    else
    {
        if (!m_spinner->isVisible() && state() == PendingHeader)
        {
            showSpinner();
            m_videoWidget->hidePlayButton();
        }
    }
}

void VideoPlayerWidget::setEntity(RemoteVideoEntity* entity, DownloadEntity* downloadEntity, int rowNumber)
{
    if (m_currentEntity == entity && downloadEntity == nullptr)
        return;

    m_currentEntity = entity;
    m_selectedDownload = downloadEntity;
    m_selectedRowNumber = rowNumber;
    if (state() == InitialState)
    {
        if ((m_currentDownload != nullptr) && m_currentDownload->visibilityState() == visTemp)
        {
            m_currentDownload->getParent()->deleteTempDE(m_currentDownload);  // delete m_currentDownload;
            hideSpinner();
            emit showPlaybutton(true);
        }
        if (entity != nullptr)
        {
            m_videoWidget->setPreviewPicture(entity);
            m_playerHeader->setVideoTitle(entity->m_videoInfo.videoTitle);
            m_descriptionPanel->setDescription(entity->m_videoInfo.strategyName, entity->m_videoInfo.description,
                                               entity->prefResolution(), rowNumber);
        }
        else
        {
            m_playerHeader->setVideoTitle({});
            m_descriptionPanel->resetDescription();
        }

        m_currentDownload = nullptr;
    }
}

void VideoPlayerWidget::openVideoInBrowser(bool alt)
{
    if (alt)
    {
        if (DownloadEntity* download = m_currentDownload ? m_currentDownload : m_selectedDownload)
        {
            utilities::SelectFile(download->filename(), global_functions::GetVideoFolder());
        }
    }
    else if (m_currentEntity != nullptr)
    {
        QDesktopServices::openUrl(m_currentEntity->m_videoInfo.originalUrl);
    }
}

void VideoPlayerWidget::playDownloadEntity(DownloadEntity* entity)
{
    Q_ASSERT(entity);
    m_currentEntity = entity->getParent();
    emit playDownloadEntityAsynchronously(entity);
}

void VideoPlayerWidget::onPlayDownloadEntityAsynchronously(const QPointer<DownloadEntity>& e)
{
    if (e.isNull())
    {
        return;
    }

    DownloadEntity* entity = e.data();

    if (m_currentDownload != nullptr && entity != m_currentDownload)
    {
        stopVideo(false);
    }

    m_currentDownload = entity;
    m_currentFile = m_currentDownload->filename();
    if (entity->state() == Downloadable::kPaused && entity->visibilityState() == visTemp)
    {
        RemoteVideoEntity::startTempDownloading(entity);
    }

    // make sure we don't connect to the progress bar
    disconnect(m_currentDownload, nullptr, m_progressBar, nullptr);

    // Change download position
    VERIFY(connect(m_currentDownload, SIGNAL(progressChanged(qint64, qint64)), m_progressBar,
                   SLOT(displayDownloadProgress(qint64, qint64)), Qt::UniqueConnection));

    // Process file opening
    processPreviewEntity();

    emit playingDownloadEntity(m_currentDownload);
}

void VideoPlayerWidget::startPreviewDownload()
{
    if (m_currentEntity != nullptr)
    {
        // Stop playing if we already have started player
        if (m_currentDownload != nullptr)
        {
            getDecoder()->close(true);
        }

        DownloadEntity* current = (m_selectedDownload != nullptr)
            ? m_selectedDownload.data() : m_currentEntity->actualDownload();

        if (current == nullptr)
        {
            current = m_currentEntity->requestStartDownload(visTemp);
            if (current->state() != Downloadable::kFailed)  // We don't resurrect preview downloads
            {
                m_videoWidget->hidePlayButton();
                showSpinner();
            }
            else
            {
                current->getParent()->deleteTempDE(current);  // delete m_currentDownload;
                return;
            }
        }
        else if (current->state() == Downloadable::kQueued)
        {
            current->startDownloadWithHighestPriority();
            m_videoWidget->hidePlayButton();
            showSpinner();
        }

        playDownloadEntity(current);
    }
}

void VideoPlayerWidget::setDefaultPreviewPicture() { m_videoWidget->setDefaultPreviewPicture(); }

QString VideoPlayerWidget::currentFilename() const { return m_currentFile; }

void VideoPlayerWidget::setProgressbar(VideoProgressBar* progressbar)
{
    Q_ASSERT(progressbar);
    m_progressBar = progressbar;
    VERIFY(connect(getDecoder(), SIGNAL(changedFramePosition(qint64, qint64)), m_progressBar,
                   SLOT(displayPlayedProgress(qint64, qint64))));
    progressbar->installEventFilter(this);
}

void VideoPlayerWidget::setVideoHeader(PlayerHeader* videoHeader)
{
    Q_ASSERT(videoHeader);
    m_playerHeader = videoHeader;
    videoHeader->installEventFilter(this);
}

void VideoPlayerWidget::setControl(VideoControl* controlWidget)
{
    Q_ASSERT(controlWidget);
    m_controls = controlWidget;
    controlWidget->installEventFilter(this);
}

void VideoPlayerWidget::stopVideo(bool showDefaultImage)
{
    hideSpinner();
    if (!getDecoder()->isFileLoaded())
    {
        updateViewOnVideoStop(showDefaultImage);
    }
    else
    {
        Q_ASSERT(m_currentDownload);
        getDecoder()->setDefaultImageOnClose(showDefaultImage);
        getDecoder()->close(true);
        RemoteVideoEntity::pauseTempDE(m_currentDownload);
        // signal will call stopVideoProcess()
    }
}

void VideoPlayerWidget::playFile(const QString& fileName)
{
    hideSpinner();
    if (QFile::exists(fileName))
    {
        Q_ASSERT(!fileName.isEmpty());
        m_currentFile = fileName;
        FFmpegDecoder* decoder = getDecoder();
        decoder->openFile(m_currentFile);
        const bool fromPendingHeaderPaused = state() == PendingHeaderPaused;
        if (fromPendingHeaderPaused)
        {
            decoder->play(true);
            setState(Paused);
        }
        else
        {
            decoder->play();
            setState(Playing);
            emit showPlaybutton(false);
        }

        if (m_videoWidget->isFullScreen())
        {
            decoder->setPreferredSize(m_videoWidget->width(), m_videoWidget->height());
        }
        else
        {
            updateLayout(fromPendingHeaderPaused);
        }
    }
    else
    {
        m_progressBar->setDownloadedCounter(0);
        setState(InitialState);
        emit showPlaybutton(true);
        QMessageBox::information(this, tr(PROJECT_NAME),
                                 tr("File %1 cannot be played as it doesn't exist.").arg(fileName));
    }
}

void VideoPlayerWidget::pauseVideo()
{
    if (state() == Playing && getDecoder()->pauseResume())
    {
        setState(Paused);
    }
}

bool VideoPlayerWidget::isPaused() { return (state() == Paused); }

void VideoPlayerWidget::seekByPercent(float percent, int64_t totalDuration)
{
    getDecoder()->seekByPercent(percent, totalDuration);
}

void VideoPlayerWidget::playPauseButtonAction()
{
    switch (state())
    {
    case InitialState:
        if (m_currentEntity == nullptr)
            return;
    case Paused:  // fall through
        emit showPlaybutton(false);
        if (isPaused())
        {
            resumeVideo();
        }
        else
        {
            startPreviewDownload();
        }
        break;
    case PendingHeaderPaused:
        emit showPlaybutton(false);
        setState(PendingHeader);
        break;
    case Playing:
        emit showPlaybutton(true);
        pauseVideo();
        break;
    case PendingHeader:
        emit showPlaybutton(true);
        setState(PendingHeaderPaused);
        break;
    }
}

void VideoPlayerWidget::resumeVideo()
{
    if (state() == Paused && getDecoder()->pauseResume())  // It actually resumes
    {
        setState(Playing);
    }
}

void VideoPlayerWidget::updateViewOnVideoStop(bool showDefaultImage /* = false*/)
{
    if (m_videoWidget->isFullScreen())
    {
        m_videoWidget->fullScreen(false);
    }

    emit showPlaybutton(true);

    if (m_currentDownload != nullptr)
    {
        disconnect(m_currentDownload, nullptr, m_progressBar, nullptr);
        disconnect(m_currentDownload, nullptr, getDecoder(), nullptr);
    }

    if ((m_currentDownload != nullptr) && (m_currentEntity != nullptr))
    {
        m_descriptionPanel->setDescription(m_currentEntity->m_videoInfo.strategyName,
                                           m_currentEntity->m_videoInfo.description,
                                           m_currentDownload->currentResolution(),
                                           m_selectedRowNumber);
        m_progressBar->resetProgress();
        m_playerHeader->setVideoTitle(m_currentEntity->m_videoInfo.videoTitle);
    }
    else
    {
        m_currentFile = QString();
        m_progressBar->resetProgress();
        m_playerHeader->setVideoTitle({});
        m_descriptionPanel->resetDescription();
    }
    setState(InitialState);

    if (showDefaultImage)
    {
        m_videoWidget->setPreviewPicture(m_currentEntity);
    }

    emit fileReleased();
}

VideoPlayerWidget::~VideoPlayerWidget() { stopVideo(); }

void VideoPlayerWidget::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    updateLayout();
}

bool VideoPlayerWidget::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::KeyRelease)
    {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Space)
        {
            playPauseButtonAction();
        }
    }
    return QFrame::eventFilter(object, event);
}

void VideoPlayerWidget::setDefaultDisplay() { setDisplay(new VideoWidget()); }

void VideoPlayerWidget::updateLayout(bool fromPendingHeaderPaused /* = false*/)
{
    int currWidth = width();
    int currHeight = height();

    const int controlsHeight = m_controls->getHeight();
    const int minDescHeight = m_descriptionPanel->minimumHeight();

    const int minPlayerHeight = currHeight - controlsHeight - minDescHeight;
    int playerWidth = currWidth;
    int playerHeight = 0;
    int yPos = 1;
    FFmpegDecoder* dec = getDecoder();
    Q_ASSERT(dec != nullptr);
    if (state() == InitialState || state() == PendingHeader || state() == PendingHeaderPaused)
    {
        double aspectRatio = (double)m_videoWidget->getPictureSize().height() / m_videoWidget->getPictureSize().width();
        playerHeight = aspectRatio * playerWidth;
        // Display too big: do recalculation
        if (playerHeight > minPlayerHeight)  // TODO: code refactoring
        {
            playerHeight = minPlayerHeight;
            playerWidth = (int)(minPlayerHeight / aspectRatio);
        }

        m_videoWidget->setGeometry(0, yPos, playerWidth, playerHeight - PROGRESSBAR_VISIBLE_HEIGHT);

        QImage previewPic = m_videoWidget->startImageButton().scaled(playerWidth, playerHeight - PROGRESSBAR_VISIBLE_HEIGHT,
                                                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (m_videoWidget->startImageButton() == m_videoWidget->noPreviewImage())
        {
            m_videoWidget->showPicture(previewPic);
        }
        else
        {
            m_videoWidget->updatePlayButton();
            m_videoWidget->showPicture(m_videoWidget->drawPreview(previewPic));
        }
    }
    else if (dec->isPlaying())
    {
#ifdef DEVELOPER_OPENGL
        QSize pictureSize = QSize(playerWidth, playerWidth);
        double aspectRatio = 1. / dec->aspectRatio();
#else
        QSize pictureSize = dec->getPreferredSize(playerWidth, playerWidth).size();
        double aspectRatio = (double)pictureSize.height() / pictureSize.width();
#endif
        playerHeight = pictureSize.width() * aspectRatio;
        // Display too big: do recalculation
        if (playerHeight > minPlayerHeight)
        {
            playerHeight = minPlayerHeight;
            playerWidth = (int)(minPlayerHeight / aspectRatio);
        }

        if (m_videoWidget->isFullScreen())
        {
            m_videoWidget->setGeometry(0, 0, currWidth, currHeight);
#ifndef DEVELOPER_OPENGL
            dec->setPreferredSize(currWidth, currHeight);
#endif
        }
        else
        {
            m_videoWidget->setGeometry(0, yPos, playerWidth, playerHeight - PROGRESSBAR_VISIBLE_HEIGHT);
#ifndef DEVELOPER_OPENGL
            dec->setPreferredSize(playerWidth, playerHeight);
#endif
        }
        // Not required by opengl
#ifndef DEVELOPER_OPENGL
        if (state() == Playing)
        {
            // FIXME: OpenGL full support
            m_videoWidget->setPixmap(
                m_videoWidget->pixmap()->scaledToHeight(m_videoWidget->height(), Qt::SmoothTransformation));
        }
        else if (state() == Paused && !fromPendingHeaderPaused)
        {
            m_videoWidget->showPicture(m_videoWidget->originalFrame().scaled(
                playerWidth, playerHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
#endif
    }

    yPos += playerHeight;

    m_spinner->move(0, 0);
    m_spinner->resize(playerWidth, yPos);

    if (m_progressBar != nullptr)
    {
        int progressHeight = m_progressBar->height();
        m_progressBar->move(0, yPos - (progressHeight + PROGRESSBAR_VISIBLE_HEIGHT) / 2);
        m_progressBar->resize(playerWidth, progressHeight);
    }

    int controlsPos = (playerWidth - m_controls->getWidth()) / 2;
    if (controlsPos < 0)
    {
        controlsPos = 0;
    }
    m_controls->move(controlsPos, yPos);
    m_controls->resize(m_controls->getWidth(), controlsHeight);
    yPos += controlsHeight;

    m_descriptionPanel->move(0, yPos);
    m_descriptionPanel->resize(playerWidth, currHeight - yPos);
}

void VideoPlayerWidget::exitFullScreen() { onPlayingFinished(); }

void VideoPlayerWidget::onPlayingFinished()
{
    if ((m_videoWidget != nullptr) && m_videoWidget->isFullScreen())
    {
        m_videoWidget->fullScreen(false);
    }
}

// TODO: not needed more, now seeking is using dynamic update
void VideoPlayerWidget::disableSeeking() { Q_ASSERT(m_progressBar != nullptr); }

void VideoPlayerWidget::onDownloadStateChanged(Downloadable::State newState, Downloadable::State prevState)
{
    Q_UNUSED(prevState)
    if (newState == Downloadable::kFailed && state() == PendingHeader)
    {
        disconnect(m_currentDownload, nullptr, m_progressBar, nullptr);
        disconnect(m_currentDownload, nullptr, getDecoder(), nullptr);
        setState(InitialState);
    }
}

void VideoPlayerWidget::showSpinner() { m_spinner->show(); }

void VideoPlayerWidget::hideSpinner() { m_spinner->hide(); }
