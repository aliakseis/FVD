#include "widgetdisplay.h"

#include <QDebug>

#include "fpicture.h"

WidgetDisplay::WidgetDisplay(QWidget* parent) : QLabel(parent), m_skipDisplay(false)
{
    connect(this, &WidgetDisplay::display, this, &WidgetDisplay::currentDisplay, Qt::BlockingQueuedConnection);
}

void WidgetDisplay::currentDisplay()
{
    if (!m_skipDisplay)
    {
        m_display = QPixmap::fromImage(m_image);
        setPixmap(m_display);
    }
    else
    {
        qDebug() << __FUNCTION__ << "display skipped";
    }
}

void WidgetDisplay::displayFrame(unsigned int videoGeneration)
{
    emit display();
    displayFrameFinished(videoGeneration);
}

void WidgetDisplay::renderFrame(const FPicture& frame, unsigned int videoGeneration)
{
    m_image = QImage(frame.data()[0], frame.width(), frame.height(), QImage::Format_RGB888);
}

AVPixelFormat WidgetDisplay::preferablePixelFormat() const { return AV_PIX_FMT_RGB24; }

bool WidgetDisplay::resizeWithDecoder() const { return true; }

void WidgetDisplay::showPicture(const QImage& picture) { showPicture(QPixmap::fromImage(picture)); }

void WidgetDisplay::showPicture(const QPixmap& picture) { setPixmap(picture); }
