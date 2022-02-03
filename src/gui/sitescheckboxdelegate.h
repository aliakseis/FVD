#ifndef CHECKBOXITEMDELEGATE_H
#define CHECKBOXITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QPen>

class SitesCheckboxDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	explicit SitesCheckboxDelegate(QObject* parent = 0);

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
	const QSize m_sizeHint;
	const QBrush m_backBrush;
	const QBrush m_itemBackLightBrush, m_itemBackDarkBrush;
	const QPen m_defPen;
	const QPen m_hoverPen;
	const int m_padding;
};

#endif // CHECKBOXITEMDELEGATE_H
