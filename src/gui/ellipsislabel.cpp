#include "ellipsislabel.h"

EllipsisLabel::EllipsisLabel(QWidget* parent) : QLabel(parent) {}

EllipsisLabel::~EllipsisLabel() = default;

void EllipsisLabel::setText(const QString& text)
{
    title = text;
    cutText();
}

QString EllipsisLabel::text() { return title; }

void EllipsisLabel::resizeEvent(QResizeEvent* event)
{
    cutText();
    QLabel::resizeEvent(event);
}

void EllipsisLabel::cutText()
{
    QFontMetrics metrics(font());
    QLabel::setText(metrics.elidedText(title, Qt::ElideRight, width() - 2));
}
