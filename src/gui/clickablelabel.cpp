#include "clickablelabel.h"

ClickableLabel::ClickableLabel(QWidget* parent) : QLabel(parent) {}

ClickableLabel::~ClickableLabel() = default;

void ClickableLabel::mousePressEvent(QMouseEvent* event)
{
    QLabel::mousePressEvent(event);
    emit clicked();
}
