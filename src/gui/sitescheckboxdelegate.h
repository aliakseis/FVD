#pragma once

#include <QStyledItemDelegate>

class SitesCheckboxDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SitesCheckboxDelegate(QObject* parent = 0);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    const QSize m_sizeHint;
    const int m_padding;
};
