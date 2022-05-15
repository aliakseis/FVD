#include "videowidget.h"

#include <QPainter>
#include <QResizeEvent>

#include "customdockwidget.h"
#include "downloadentity.h"
#include "mainwindow.h"
#include "player/ffmpegdecoder.h"
#include "videoplayerwidget.h"

static const int HEIGHT_FIX = 1;

VideoWidget::VideoWidget(VideoPlayerWidget* parent)
#ifdef DEVELOPER_OPENGL
    : OpenGLDisplay(parent),
#else
    : WidgetDisplay(parent),
#endif
      m_playIndicator(false),
      m_defPlayButton(":/images/video___btn_play___default___(94x94).png"),
      m_hoverPlayButton(":/images/video___btn_play___hover___(94x94).png"),
      m_clickedPlayButton(":/images/video___btn_play___clicked___(94x94).png"),
      m_selImage(&m_defPlayButton),
      m_isMousePressed(false),
      m_playBtnRadius(29),
#ifdef Q_OS_LINUX
      m_resizeIndicator(false),
#endif
      m_lastMouseTime(0)
{
    m_noPreviewImg = QImage(":/images/fvd_banner.png");
    setMouseTracking(true);
    setStyleSheet("background: black;");
    // FIXME: OpenGL full support
#ifndef DEVELOPER_OPENGL
    setAlignment(Qt::AlignCenter);
#endif

    m_cursorTimer.setInterval(1000);
    VERIFY(connect(&m_cursorTimer, SIGNAL(timeout()), SLOT(onCursorTimer())));
    connect(&m_imageCache, &ImageCache::getImageFinished, this, &VideoWidget::getImageFinished);
    m_cursorTimer.start();
}

VideoWidget::~VideoWidget()
{
    if (auto videoPlayerWidgetInstance = VideoPlayerWidgetInstance())
    {
        videoPlayerWidgetInstance->m_videoWidget = nullptr;
    }
}

void VideoWidget::setDefaultPreviewPicture()
{
    m_startImgButton = m_noPreviewImg;
    m_pictureSize = m_startImgButton.size();

    VideoPlayerWidgetInstance()->updateLayout();

    m_fromImage = m_startImgButton;

    showPicture(
        m_startImgButton.scaled(width(), height() + HEIGHT_FIX, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    setContentsMargins(0, 0, 0, 0);
}

void VideoWidget::setPreviewPicture(const RemoteVideoEntity* entity)
{
    if (entity != nullptr)
    {
        if (entity->m_videoInfo.thumbnailUrl.isEmpty())
        {
            if (auto download = entity->actualDownload())
            {
                m_imageCache.getAsync(download->filename(), "", download->filename());
            }
            else
            {
                setDefaultPreviewPicture();
            }
        }
        else if (entity->id() == entity->m_videoInfo.thumbnailUrl)  // if file was added by synchronization to library
        {
            m_imageCache.getAsync(entity->m_videoInfo.thumbnailUrl, "", entity->m_videoInfo.thumbnailUrl);
        }
        else
        {
            m_imageCache.getAsync(entity->id(), entity->m_videoInfo.thumbnailUrl, "");
        }
    }
    else
    {
        setDefaultPreviewPicture();
    }
}

QPixmap VideoWidget::drawPreview(const QImage& fromImage)
{
    Q_ASSERT(m_selImage);
    if (VideoPlayerWidgetInstance()->state() == VideoPlayerWidget::InitialState)
    {
        QPixmap preview_with_button(fromImage.width(), fromImage.height());
        QPainter painter(&preview_with_button);
        painter.drawImage(0, 0, fromImage);
        painter.drawImage(fromImage.width() / 2 - m_selImage->width() / 2,
                          fromImage.height() / 2 - m_selImage->height() / 2, *m_selImage);
        painter.end();
        return preview_with_button;
    }
    return QPixmap::fromImage(fromImage);
}

void VideoWidget::hidePlayButton()
{
    showPicture(
        m_startImgButton.scaled(width(), height() + HEIGHT_FIX, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void VideoWidget::getImageFinished(const QImage& image)
{
    m_startImgButton = m_fromImage = image.isNull() ? m_noPreviewImg : image;

    m_pictureSize = m_startImgButton.size();

    if (!isFullScreen())
    {
        // This will set image to layout
        VideoPlayerWidgetInstance()->updateLayout();
    }
}

void VideoWidget::showElements()
{
    m_lastMouseTime = QDateTime::currentMSecsSinceEpoch();
    setCursor(Qt::ArrowCursor);
    MainWindow::Instance()->videoControlWidget()->show();
}

void VideoWidget::hideElements()
{
    setCursor(Qt::BlankCursor);
    MainWindow::Instance()->videoControlWidget()->hide();
}

void VideoWidget::keyPressEvent(QKeyEvent* event)
{
    showElements();
    // Full screen exiting
    if (((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && (event->modifiers() & Qt::AltModifier)) ||
        event->key() == Qt::Key_Escape)
    {
        if (isFullScreen())
        {
            fullScreen(false);
        }
    }
}

void VideoWidget::mousePressEvent(QMouseEvent* event)
{
    showElements();

    if (event->button() == Qt::LeftButton && VideoPlayerWidgetInstance()->state() == VideoPlayerWidget::InitialState &&
        pointInButton(event->pos()) && VideoPlayerWidgetInstance()->entity() != nullptr)
    {
        m_isMousePressed = true;

        m_selImage = &m_clickedPlayButton;
        m_startImgButton = m_fromImage;

        m_pictureSize = m_startImgButton.size();
        showPicture(drawPreview(
            m_startImgButton.scaled(width(), height() + HEIGHT_FIX, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
    }
}

void VideoWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        showElements();

        VideoPlayerWidget::VideoState state = VideoPlayerWidgetInstance()->state();

        if (state == VideoPlayerWidget::Paused || state == VideoPlayerWidget::Playing)
        {
            VideoPlayerWidgetInstance()->playPauseButtonAction();
        }
        else if (m_isMousePressed)
        {
            m_isMousePressed = false;

            m_selImage = pointInButton(event->pos()) ? &m_hoverPlayButton : &m_defPlayButton;
            m_startImgButton = m_fromImage;

            m_pictureSize = m_startImgButton.size();

            if (m_selImage == &m_hoverPlayButton && state == VideoPlayerWidget::InitialState)
            {
                VideoPlayerWidgetInstance()->playPauseButtonAction();
            }
        }
    }
    else if (event->button() == Qt::MiddleButton)
    {
        isFullScreen() ? fullScreen(false)
                       : MainWindow::Instance()->dockWidget()->setVisibilityState(CustomDockWidget::FullScreen);
    }
}

void VideoWidget::mouseMoveEvent(QMouseEvent* event)
{
    showElements();

    if (VideoPlayerWidgetInstance()->state() == VideoPlayerWidget::InitialState &&
        VideoPlayerWidgetInstance()->entity() != nullptr)
    {
        QImage* selImage = m_isMousePressed ? &m_clickedPlayButton
                                            : (pointInButton(event->pos()) ? &m_hoverPlayButton : &m_defPlayButton);
        if (m_selImage != selImage)
        {
            m_selImage = selImage;
            m_startImgButton = m_fromImage;

            m_pictureSize = m_startImgButton.size();
            showPicture(drawPreview(m_startImgButton.scaled(width(), height() + HEIGHT_FIX, Qt::IgnoreAspectRatio,
                                                            Qt::SmoothTransformation)));
        }
    }
}

void VideoWidget::wheelEvent(QWheelEvent* event)
{
    showElements();
    VideoPlayerWidgetInstance()->wheelEvent(event);
}

bool VideoWidget::pointInButton(const QPoint& point)
{
    QPoint centerPoint(width() / 2, height() / 2);

    if (point.x() >= centerPoint.x() - m_playBtnRadius && point.x() <= centerPoint.x() + m_playBtnRadius &&
        point.y() >= centerPoint.y() - m_playBtnRadius && point.y() <= centerPoint.y() + m_playBtnRadius)
    {
        double xDistance = abs(centerPoint.x() - point.x());
        double yDistance = abs(centerPoint.y() - point.y());

        return sqrt(xDistance * xDistance + yDistance * yDistance) < m_playBtnRadius;
    }
    return false;
}

void VideoWidget::updatePlayButton()
{
    if (VideoPlayerWidgetInstance()->state() == VideoPlayerWidget::InitialState)
    {
        m_selImage = pointInButton(mapFromGlobal(QCursor::pos())) ? &m_hoverPlayButton : &m_defPlayButton;
    }
}

void VideoWidget::fullScreen(bool isEnable)
{
    if (isEnable)
    {
        if (VideoPlayerWidgetInstance()->entity() == nullptr)
        {
            return;
        }

        this->setWindowFlags(Qt::Window);
        setContentsMargins(0, 0, 0, 0);

#ifdef Q_OS_LINUX
        Q_ASSERT(!m_resizeIndicator);
        m_resizeIndicator = true;
#endif

        showFullScreen();

#ifndef Q_OS_LINUX
        fullScreenProcess();
#endif
        // FIXME: OpenGL full support
#ifndef DEVELOPER_OPENGL
        m_skipDisplay = true;
#endif
    }
    else
    {
        this->setWindowFlags(Qt::SubWindow);
        showNormal();
        VideoPlayerWidgetInstance()->updateLayout();
        emit leaveFullScreen();

        QWidget* videoControl = MainWindow::Instance()->videoControlWidget();
        videoControl->setParent(MainWindow::Instance()->getPlayer(), Qt::SubWindow);
        videoControl->lower();
        videoControl->show();
    }
}

void VideoWidget::fullScreenProcess()
{
    if (VideoPlayerWidget::InitialState == VideoPlayerWidgetInstance()->state())
    {
        updatePlayButton();
        setPreviewPicture(VideoPlayerWidgetInstance()->entity());
    }
    else
    {
        // FIXME: OpenGL full support
#ifndef DEVELOPER_OPENGL
        setPixmap(pixmap()->scaledToHeight(height(), Qt::SmoothTransformation));
#endif
    }

    m_decoder->setPreferredSize(width(), height(), 1);

    QWidget* videoControl = MainWindow::Instance()->videoControlWidget();
    videoControl->setParent(nullptr,
                            Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);  // Qt::WindowStaysOnTopHint
    videoControl->setAttribute(Qt::WA_TranslucentBackground);
    videoControl->move(this->width() / 2 - videoControl->width() / 2, this->height() - videoControl->height() - 20);
    videoControl->show();
    auto* spinner = findChild<QWidget*>("bufferingSpinner");
    Q_ASSERT(spinner);
    spinner->resize(this->width(), this->height());
}

void VideoWidget::resizeEvent(QResizeEvent* event)
{
#ifdef Q_OS_LINUX
    if (m_resizeIndicator)
    {
        fullScreenProcess();
        m_resizeIndicator = false;
    }
#endif
    // FIXME: OpenGL full support
#ifdef DEVELOPER_OPENGL
    OpenGLDisplay::resizeEvent(event);
#endif
}

void VideoWidget::currentDisplay()
{
    // FIXME: OpenGL full support
#ifndef DEVELOPER_OPENGL
    WidgetDisplay::currentDisplay();
    m_originalFrame = *pixmap();
#endif
}

void VideoWidget::onCursorTimer()
{
    if (isFullScreen() && (QDateTime::currentMSecsSinceEpoch() - m_lastMouseTime > 2000))
    {
        hideElements();
    }
}

VideoPlayerWidget* VideoWidget::VideoPlayerWidgetInstance() { return static_cast<VideoPlayerWidget*>(parent()); }
