#include "videocontrol.h"

#include <QtWidgets/qdrawutil.h>

#include <QMouseEvent>
#include <QPainter>

#include "player/ffmpegdecoder.h"
#include "ui_videocontrol.h"
#include "utilities/translation.h"
#include "utilities/utils.h"
#include "videoplayerwidget.h"
#include "videowidget.h"

#include <algorithm>

VideoControl::VideoControl(VideoPlayerWidget* parent)
    : QWidget(parent), ui(new Ui::VideoControl), m_isVolumeOn(false), m_prevVolumeValue(0)
{
    ui->setupUi(this);

    ui->progressBar->installEventFilter(this);

    // Link it with VideoPlayerWidget
    parent->setControl(this);
    videoPlayer = parent;

    Q_ASSERT(videoPlayer->metaObject()->className() == QString("VideoPlayerWidget"));

    // Default value from the decoder
    int volume = videoPlayer->getDecoder()->volume() * ui->progressBar->maximum();
    m_isVolumeOn = (volume == 0);

    setVolume(volume, true);

    VERIFY(connect(videoPlayer->getDecoder(), SIGNAL(volumeChanged(double)), SLOT(onProgramVolumeChange(double))));
    ui->btnPause->hide();

    // preview start
    VERIFY(connect(this, SIGNAL(browse()), parent, SLOT(openVideoInBrowser())));

    connect(parent, &VideoPlayerWidget::showPlaybutton, this, &VideoControl::onShowPlaybutton);

    m_height = geometry().height();

    background.load(":/control/background");
    backgroundfs.load(":/control/backgroundfs");

    utilities::Tr::MakeRetranslatable(this, ui);
}

VideoControl::~VideoControl() { delete ui; }

bool VideoControl::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->progressBar)
    {
        QEvent::Type eventType = event->type();
        switch (eventType)
        {
        case QEvent::MouseMove:
        {
            auto* mEvent = static_cast<QMouseEvent*>(event);
            if (mEvent->buttons() & Qt::LeftButton)
            {
                float percent = (mEvent->x() * 1.0) / ui->progressBar->width();
                percent *= 100;
                setVolume(qBound(0, (int)percent, 100));
            }
        }
        break;

        case QEvent::MouseButtonRelease:
        {
            auto* mEvent = static_cast<QMouseEvent*>(event);
            if (mEvent->button() == Qt::LeftButton)
            {
                float percent = (mEvent->x() * 1.0) / ui->progressBar->width();
                percent *= 100;
                setVolume(qBound(0, (int)percent, 100));
            }
        }
        break;
        default:;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void VideoControl::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    qDrawBorderPixmap(&painter, this->rect(), QMargins(0, 0, 0, 0), (parent() == nullptr) ? backgroundfs : background);
}

void VideoControl::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    // separators on background image (got from the image)
    const int leftSeparator = 94;
    const int rightSeparator = 137;

    // margin by X between the controls
    const int margin = (leftSeparator - ui->btnPlay->width() - ui->btnStop->width()) / 3;

    // position by Y for the buttons
    const int posY = (background.height() - ui->btnPlay->height()) / 2;

    ui->btnPlay->move(2 * margin, posY);
    ui->btnPause->move(2 * margin, posY);

    ui->btnStop->move(3 * margin + ui->btnPlay->width(), posY);

    ui->btnBrowser->move(((rightSeparator - leftSeparator) - ui->btnBrowser->width()) / 2 + leftSeparator, posY);

    ui->btnVolume->move(rightSeparator + margin, posY);

    ui->progressBar->move(ui->btnVolume->pos().x() + ui->btnVolume->width(),
                          (height() - ui->progressBar->height()) / 2 + 1);  // +1 for just more nice looking
    ui->progressBar->setFixedSize(width() - ui->progressBar->pos().x() - 3 * margin, ui->progressBar->height());
}

void VideoControl::wheelEvent(QWheelEvent* event)
{
    if (event->orientation() != Qt::Horizontal)
    {
        if (auto decoder = videoPlayer->getDecoder())
        {
            const int numDegrees = event->delta() / 8;
            const int numSteps = numDegrees / 15;
            const double newVolume = std::clamp(decoder->volume() + ((double)numSteps / 20), 0., 1.);
            decoder->setVolume(newVolume);
        }
    }
    event->accept();
}

int VideoControl::getWidth() const { return background.width(); }

void VideoControl::on_btnVolume_clicked()
{
    if (ui->progressBar->value() > 0)
    {
        setVolume(0);
    }
    else
    {
        int volume = m_prevVolumeValue;
        if (volume <= 0)
        {
            volume = 50;
        }
        setVolume(volume);
    }
}

void VideoControl::setVolume(int volume, bool onlyWidget)
{
    ui->progressBar->setValue(volume);

    switchVolumeButton(volume != 0);

    int prevVolumeValue = videoPlayer->getDecoder()->volume() * ui->progressBar->maximum();
    if ((videoPlayer != nullptr) && !onlyWidget)
    {
        videoPlayer->getDecoder()->setVolume(volume, ui->progressBar->maximum());
    }
    m_prevVolumeValue = prevVolumeValue;
}

void VideoControl::switchVolumeButton(bool volumeOn)
{
    QIcon progressBarIcon;
    if (m_isVolumeOn && !volumeOn)
    {
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_off_default"), QSize(), QIcon::Normal, QIcon::Off);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_off_clicked"), QSize(), QIcon::Normal, QIcon::On);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_off_default"), QSize(), QIcon::Disabled, QIcon::Off);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_off_default"), QSize(), QIcon::Disabled, QIcon::On);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_off_hover"), QSize(), QIcon::Active, QIcon::Off);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_off_hover"), QSize(), QIcon::Active, QIcon::On);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_off_default"), QSize(), QIcon::Selected, QIcon::Off);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_off_default"), QSize(), QIcon::Selected, QIcon::On);
        ui->btnVolume->setIcon(progressBarIcon);
        m_isVolumeOn = volumeOn;
    }
    else if (!m_isVolumeOn && volumeOn)
    {
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_on_default"), QSize(), QIcon::Normal, QIcon::Off);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_on_clicked"), QSize(), QIcon::Normal, QIcon::On);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_on_default"), QSize(), QIcon::Disabled, QIcon::Off);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_on_default"), QSize(), QIcon::Disabled, QIcon::On);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_on_hover"), QSize(), QIcon::Active, QIcon::Off);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_on_hover"), QSize(), QIcon::Active, QIcon::On);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_on_default"), QSize(), QIcon::Selected, QIcon::Off);
        progressBarIcon.addFile(QString::fromUtf8(":/control/sound_on_default"), QSize(), QIcon::Selected, QIcon::On);
        ui->btnVolume->setIcon(progressBarIcon);
        m_isVolumeOn = volumeOn;
    }
}

void VideoControl::on_btnPlay_clicked() { videoPlayer->playPauseButtonAction(); }

void VideoControl::on_btnPause_clicked() { videoPlayer->playPauseButtonAction(); }

void VideoControl::on_btnStop_clicked()
{
    Q_ASSERT(videoPlayer);
    emit videoPlayer->showPlaybutton(true);
    if (videoPlayer->state() != VideoPlayerWidget::InitialState)
    {
        videoPlayer->stopVideo(true);
    }
}

void VideoControl::on_btnBrowser_clicked() { emit browse(); }

void VideoControl::onProgramVolumeChange(double volume) { setVolume(volume * ui->progressBar->maximum(), true); }

void VideoControl::onShowPlaybutton(bool show)
{
    ui->btnPlay->setVisible(show);
    ui->btnPause->setVisible(!show);
}
