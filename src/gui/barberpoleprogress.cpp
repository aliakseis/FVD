#include "barberpoleprogress.h"

#include <QStylePainter>

#include <chrono>

BarberPoleProgress::BarberPoleProgress(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    connect(&m_periodicUpdateTimer, &QTimer::timeout, this, static_cast<void (QWidget::*)()>(&QWidget::update));
}

void BarberPoleProgress::startAnimation()
{
    m_periodicUpdateTimer.start(40);
}

void BarberPoleProgress::stopAnimation()
{
    m_periodicUpdateTimer.stop();
}

void BarberPoleProgress::paintEvent(QPaintEvent *event)
{
    using namespace std::chrono;
    enum { PERIOD = 400 };
    const uint64_t ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    const double patternSize = 16.;

    const auto offset = (ms % PERIOD) * patternSize / PERIOD * 4;

    QStylePainter painter(this);
    QLinearGradient linearGradient(offset, 0., patternSize + offset, patternSize);
    linearGradient.setColorAt(0., QColor("#d1e6bb"));
    linearGradient.setColorAt(1., QColor("#649330"));
    linearGradient.setSpread(QRadialGradient::ReflectSpread);
    painter.fillRect(rect(), linearGradient);
}
