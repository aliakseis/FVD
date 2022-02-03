#include "uiutils.h"

#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QMargins>
#include <QApplication>
#include <QDesktopWidget>


namespace ui_utils
{

void drawStableXAxisBorderPixmap(QPainter* painter, const QRect& target, const QMargins& margins, const QPixmap& pixmap, double scaler)
{
	int part_first = margins.left();
	int part_second = margins.right();
	int downloaded_pixels = (target.width() - part_first - part_second) * scaler;

	// we can draw
	if (downloaded_pixels > 0)
	{
		painter->drawPixmap(
			QRect(target.left(), target.top(), part_first, pixmap.height()),
			pixmap,
			QRect(0, 0, part_first, pixmap.height())
		);
		int stx = 0;
		int stable_part_size = pixmap.width() - part_first - part_second;
		Q_ASSERT(stable_part_size >= 0);
		for (stx = part_first;
				stx + stable_part_size < downloaded_pixels + part_first;
				stx += stable_part_size
			)
		{
			painter->drawPixmap(
				QRect(target.left() + stx, target.top(), stable_part_size, pixmap.height()),
				pixmap,
				QRect(part_first, 0, stable_part_size, pixmap.height())
			);
		}
		if (downloaded_pixels - (stx - part_first) > 0)
		{
			painter->drawPixmap(
				QRect(target.left() + stx, target.top(), downloaded_pixels - (stx - part_first), pixmap.height()),
				pixmap,
				QRect(part_first, 0, downloaded_pixels - (stx - part_first), pixmap.height())
			);
		}

		painter->drawPixmap(
			QRect(target.left() + part_first + downloaded_pixels, target.top(), part_second, pixmap.height()),
			pixmap,
			QRect(pixmap.width() - part_second, 0, part_second, pixmap.height())
		);
	}
}


QRect adjustToAvailableScreenRect(QRect rect)
{
	const QRect availableGeom = QApplication::desktop()->availableGeometry();

	if (!availableGeom.contains(rect, true/*==entirely*/))
	{
		// adjust size
		const QSize newSize = rect.size().boundedTo(availableGeom.size());
		rect.setSize(newSize);

		// adjust position
		if (rect.top() < availableGeom.top())
		{
			rect.moveTop(availableGeom.top());
		}
		else if (rect.bottom() > availableGeom.bottom())
		{
			rect.moveBottom(availableGeom.bottom());
		}
		if (rect.left() < availableGeom.left())
		{
			rect.moveLeft(availableGeom.left());
		}
		else if (rect.right() > availableGeom.right())
		{
			rect.moveRight(availableGeom.right());
		}
	}
	return rect;
}

void ensureWidgetIsOnScreen(QWidget* widget)
{
	const QRect oldGeom = widget->geometry();
	const QRect newGeom = adjustToAvailableScreenRect(oldGeom);
	if (newGeom != oldGeom)
	{
		widget->setGeometry(newGeom);
	}
}


}
