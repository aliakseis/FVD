#include "downloadsdelegate.h"

#include <QApplication>
#include <QDebug>

#include "downloadlistmodel.h"

DownloadsDelegate::DownloadsDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QWidget* DownloadsDelegate::createEditor(QWidget* /*parent*/, const QStyleOptionViewItem& /*option*/,
                                         const QModelIndex& /*index*/) const
{
    return nullptr;
}

void DownloadsDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    if (index.column() == DL_Progress)
    {
        QStyleOptionProgressBar progressBarOption;
        progressBarOption.state = QStyle::State_Enabled;
        progressBarOption.direction = QApplication::layoutDirection();
        progressBarOption.rect = option.rect.adjusted(3, 7, -3, -6);

        progressBarOption.fontMetrics = QApplication::fontMetrics();
        progressBarOption.minimum = 0;
        progressBarOption.maximum = 100;
        progressBarOption.textAlignment = Qt::AlignCenter;
        progressBarOption.textVisible = true;

        const float progressFloat = index.data().toFloat();
        const int progress = static_cast<int>(progressFloat);
        progressBarOption.progress = progress < 0 ? 0 : progress;
        progressBarOption.text = utilities::ProgressString(progressFloat);

        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);
    }
}

QSize DownloadsDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const
{
    if (index.row() == DL_Icon)
    {
        return {32, 32};
    }
    if (index.row() == DL_Progress)
    {
        return {100, 32};
    }

    return {90, 32};
}
