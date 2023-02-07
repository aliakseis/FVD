#include "videoprogressbar.h"

#include "player/ffmpegdecoder.h"
#include "utilities/utils.h"
#include "videoplayerwidget.h"

#include <qdrawutil.h>

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <algorithm>
#include <cmath>

VideoProgressBar::VideoProgressBar(QWidget* parent)
    : QProgressBar(parent),
    m_clicker(":/images/video_seek_cursor.png"),
    m_btn_down(false), m_seekDisabled(false), m_downloadedTotalOriginal(0)
{
    setWindowTitle("Video Progress Bar");
    resize(500, 200);
    installEventFilter(this);
}

VideoProgressBar::~VideoProgressBar() = default;

void VideoProgressBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    QLinearGradient gradient(0, 0, 0, height());

    int lineheight = height() / 3;
    int margintop = (height() - lineheight) / 2;

    // Draw background line
    gradient.setColorAt(0, QColor(67, 67, 67));
    gradient.setColorAt(1, QColor(49, 49, 50));
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRect(0, margintop, width(), lineheight);

    // Downloaded line
    gradient.setColorAt(0, QColor(235, 235, 235));
    gradient.setColorAt(1, QColor(207, 213, 217));
    painter.setBrush(gradient);
    painter.drawRect(0, margintop, std::lround(m_downloadedRatio * width()), lineheight);

    // 3 step : playing line
    gradient.setColorAt(0, QColor(209, 63, 70));
    gradient.setColorAt(1, QColor(177, 10, 11));
    painter.setBrush(gradient);
    painter.drawRect(0, margintop, std::lround(m_playedRatio * width()), lineheight);

    painter.drawPixmap({ getClickerOffset(), 1, m_clicker.width(), m_clicker.height() },
        m_clicker, m_clicker.rect());
}

void VideoProgressBar::setDownloadedCounter(double downloaded)
{
    downloaded = std::clamp(downloaded, 0., 1.);

    const auto prevDownloadedOffset = std::lround(m_downloadedRatio * width());

    m_downloadedRatio = downloaded;

    if (prevDownloadedOffset != std::lround(downloaded * width()))
    {
        repaint();
    }
}

void VideoProgressBar::setPlayedCounter(double played)
{
    played = std::clamp(played, 0., 1.);

    const auto prevClickerOffset = getClickerOffset();

    // FIXME: when file is broken, decoder can take part of it as a full part.

    m_playedRatio = played;

    if (prevClickerOffset != getClickerOffset())
    {
        repaint();
    }
}

void VideoProgressBar::resetProgress()
{
    m_downloadedRatio = 0;
    m_playedRatio = 0;
    repaint();

    setToolTip({});
    if (underMouse())
    {
        QToolTip::hideText();
    }
}

bool VideoProgressBar::eventFilter(QObject* obj, QEvent* event)
{
    QEvent::Type eventType = event->type();
    switch (eventType)
    {
    case QEvent::MouseMove:
    {
        auto* mevent = static_cast<QMouseEvent*>(event);
        float percent = (mevent->x() * 1.0) / width();

        if (m_btn_down)
        {
            percent = std::clamp(percent, 0.F, 1.F);

            if (!m_seekDisabled)
            {
                VideoPlayerWidgetInstance()->seekByPercent(percent);
            }
        }
    }
    break;
    case QEvent::MouseButtonPress:
    {
        m_btn_down = true;
    }
    break;
    case QEvent::MouseButtonRelease:
    {
        auto* mevent = static_cast<QMouseEvent*>(event);
        float percent = (mevent->x() * 1.0) / width();

        percent = std::clamp(percent, 0.F, 1.F);

        if (!m_seekDisabled)
        {
            const FFmpegDecoder* decoder = VideoPlayerWidgetInstance()->getDecoder();
            int64_t fileSize = m_downloadedTotalOriginal - decoder->headerSize();
            int64_t limiterDuration = decoder->limiterDuration();
            int64_t limiterBytes = decoder->limiterBytes() - decoder->headerSize();
            int64_t durationAssumption = ((double)fileSize / limiterBytes) * limiterDuration;
            if (decoder->isBrokenDuration() && decoder->isRunningLimitation() && limiterDuration > 0 &&
                limiterBytes > 0 && m_downloadedTotalOriginal > 0)
            {
                VideoPlayerWidgetInstance()->seekByPercent(percent, durationAssumption);
            }
            else
            {
                VideoPlayerWidgetInstance()->seekByPercent(percent);
            }
        }
        m_btn_down = false;
    }
    break;
    default:;
    }

    return QWidget::eventFilter(obj, event);
}

void VideoProgressBar::displayDownloadProgress(qint64 downloaded, qint64 total)
{
    m_downloadedTotalOriginal = total;
    // download progress must represents seeking value
    downloaded = (downloaded >= total) ? downloaded : qMax<qint64>(downloaded - PLAYBACK_AVPACKET_MAX, 0);
    setDownloadedCounter((double)downloaded / total);
}

void VideoProgressBar::displayPlayedProgress(qint64 frame, qint64 total)
{
    // total duration chance determination
    const auto videoPlayerWidgetInstance = VideoPlayerWidgetInstance();
    if (nullptr == videoPlayerWidgetInstance)
    {
        return;
    }
    const auto decoder = videoPlayerWidgetInstance->getDecoder();
    if (nullptr == decoder || !decoder->isPlaying())
    {
        return;
    }
    int64_t fileSize = m_downloadedTotalOriginal - decoder->headerSize();
    int64_t limiterDuration = decoder->limiterDuration();
    int64_t limiterBytes = decoder->limiterBytes() - decoder->headerSize();
    int64_t durationAssumption = ((double)fileSize / limiterBytes) * limiterDuration;
    if (decoder->isBrokenDuration() && decoder->isRunningLimitation() && limiterDuration > 0 && limiterBytes > 0 &&
        m_downloadedTotalOriginal > 0)
    {
        // changing total duration to assumption value
        total = durationAssumption;
    }

    setPlayedCounter(m_seekDisabled? 0. : (double)frame / total);

    const auto durationSecs = std::lround(decoder->getDurationSecs(total));
    if (durationSecs > 0)
    {
        const auto currentSecs = std::lround(decoder->getDurationSecs(frame));

        QString text = utilities::secondsToString(currentSecs) + " / " + utilities::secondsToString(durationSecs);
        setToolTip(text);
        if (underMouse() && QToolTip::isVisible())
        {
            QToolTip::showText(QCursor::pos(), text, this);
        }
    }
}

void VideoProgressBar::seekingEnable(bool enable /* = true*/) { m_seekDisabled = !enable; }

int VideoProgressBar::getClickerOffset()
{
    static const double scale = 1.;
    return (m_playedRatio / (scale - m_clicker.width() * scale / width())) * width() - 1 -
        m_clicker.width() * m_playedRatio / scale * 1.8;
}
