#pragma once

#include <QRect>

class QWidget;
class QPainter;
class QPixmap;
class QMargins;


namespace ui_utils
{

// returns rect to fit the available screen geometry
QRect adjustToAvailableScreenRect(QRect rect);
// makes widget geometry fit the available screen geometry (considering taskbar)
void ensureWidgetIsOnScreen(QWidget* widget);

void drawStableXAxisBorderPixmap(QPainter* painter, const QRect& target, const QMargins& margins, const QPixmap& pixmap, double scaler = 1.);

}
