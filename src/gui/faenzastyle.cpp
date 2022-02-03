#include "faenzastyle.h"
#include <QStyleOption>
#include <QPainter>

namespace {

#ifdef Q_OS_WIN32
QRect windowsClassicBug;
#endif

} // namespace

void FaenzaStyle::drawControl(ControlElement element,	const QStyleOption* option,	QPainter* painter,	const QWidget* widget) const
{
	if (element == QStyle::CE_ProgressBarContents)
	{
		painter->save();
		if (const auto* bar = qstyleoption_cast<const QStyleOptionProgressBar*>(option))
		{
			QRect rect = bar->rect;
            const bool indeterminate = (bar->minimum == 0 && bar->maximum == 0);

#ifdef Q_OS_WIN32
			rect = windowsClassicBug;
#endif
			int maxWidth = rect.width();
			int minWidth = 0;
			qreal progress = qMax(bar->progress, bar->minimum); // workaround for bug in QProgressBar
			int progressBarWidth = (progress - bar->minimum) * qreal(maxWidth) / qMax(qreal(1.0), qreal(bar->maximum) - bar->minimum);
			int width = indeterminate ? maxWidth : qMax(minWidth, progressBarWidth);

			QRect progressBar;

			if (!indeterminate)
			{
#ifdef Q_OS_WIN32
				int interlnalWidth = (((float)maxWidth - 4) / maxWidth) * width;
				progressBar.setRect(rect.left() + 2, rect.top() + 2, interlnalWidth, rect.height() - 4);
#else

				int interlnalWidth = (((float)maxWidth - 5) / maxWidth) * width;
				progressBar.setRect(rect.left() + 2, rect.top() + 2, interlnalWidth, rect.height() - 5);
#endif
			}

#if defined(Q_OS_MAC)
			painter->save();
			painter->setRenderHint(QPainter::HighQualityAntialiasing, true);
			QPen pen = QPen(QColor("#73ab36"));
			QRect rctGroove = option->rect.adjusted(0, 0, -1, -1);
			QBrush brush = QBrush(QColor(Qt::white));
			painter->setPen(pen);
			painter->setBrush(brush);
			painter->drawRoundedRect(rctGroove, 4, 4);
			painter->restore();

#endif

			QLinearGradient backgroundGradient(option->rect.topRight(), option->rect.bottomRight());
			backgroundGradient.setColorAt(0, QColor("#d0f2b4"));
			backgroundGradient.setColorAt(1, QColor("#9dc072"));

			painter->setPen(QColor("#649330"));
			painter->setBrush(backgroundGradient);

			if (width > 0)
			{
				painter->drawRoundedRect(progressBar, 3, 3, Qt::AbsoluteSize);
			}

			painter->save();
			painter->setRenderHint(QPainter::Antialiasing, true);
			painter->setClipRect(progressBar.adjusted(3, 3, -2, -2));
			painter->setPen(QPen(QColor("#d1e6bb"), 5));


			for (int x = progressBar.left() - 32; x < rect.right() ; x += 18)
			{
				painter->drawLine(x, progressBar.bottom() + 1, x + 12, progressBar.top() - 2);
			}
			painter->restore();
#if defined(Q_OS_MAC)
			const QStyleOptionProgressBar* progressOptions = qstyleoption_cast<const QStyleOptionProgressBar*>(option);
			painter->save();
			QFont font = painter->font();
			font.setPointSize(11);
			painter->setFont(font);
			pen = QPen(QColor("#4e6a31"));
			painter->setPen(pen);
			painter->drawText(progressOptions->rect, Qt::AlignCenter, progressOptions->text);
			painter->restore();
#endif
		}
		painter->restore();
	}
#if !defined(Q_OS_MAC)
	else if (element == QStyle::CE_ProgressBarGroove)
	{
		painter->save();

		QRect rctGroove = option->rect.adjusted(0, 0, -1, -1);
#ifdef Q_OS_WIN32
		windowsClassicBug = rctGroove;
#endif
		QPen pen = QPen(QColor("#73ab36"));
		QBrush brush = QBrush(QColor(Qt::white));
		painter->setPen(pen);
		painter->setBrush(brush);
		painter->drawRoundedRect(rctGroove, 3, 3);
		painter->restore();
	}
	else if (element == QStyle::CE_ProgressBarLabel)
	{
		const auto* progressOptions = qstyleoption_cast<const QStyleOptionProgressBar*>(option);
		painter->save();
		QFont font = painter->font();
		font.setPointSize(8);
		painter->setFont(font);
		QPen pen = QPen(QColor("#4e6a31"));
		painter->setPen(pen);
		painter->drawText(progressOptions->rect, Qt::AlignCenter, progressOptions->text);
		painter->restore();
	}
#endif // !defined(Q_OS_MAC)
	else
	{
		QProxyStyle::drawControl(element, option, painter, widget);
	}
}

void FaenzaStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const
{
	// we don't draw focus frame
	if (element == QStyle::PE_FrameFocusRect)
	{
		return;
	}

	// we don't draw progressbar chunks
	if (element == QStyle::PE_IndicatorProgressChunk)
	{
		return;
	}

	QProxyStyle::drawPrimitive(element, option, painter, widget);
}
