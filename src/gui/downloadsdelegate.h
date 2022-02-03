#ifndef DOWNLOADSDELEGATE_H
#define DOWNLOADSDELEGATE_H

#include <QStyledItemDelegate>

class DownloadsDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	explicit DownloadsDelegate(QObject* parent = 0);

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

};

#endif // DOWNLOADSDELEGATE_H
