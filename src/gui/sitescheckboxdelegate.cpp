#include "sitescheckboxdelegate.h"

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QStyle>

SitesCheckboxDelegate::SitesCheckboxDelegate(QObject* parent)
    : QStyledItemDelegate(parent),
      m_sizeHint(32, 32),
      m_backBrush(QColor(216, 216, 216)),
      m_itemBackLightBrush(QColor(246, 246, 246)),
      m_itemBackDarkBrush(QColor(229, 229, 229)),
      m_defPen(QColor(202, 204, 206)),
      m_hoverPen(QBrush(QColor(132, 132, 132)), 1, Qt::DotLine),
      m_padding(3)
{
}

void SitesCheckboxDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    bool isLight = index.model()->data(index, Qt::UserRole).toBool();
    bool hover = option.state & QStyle::State_MouseOver;

    painter->save();
    // *** Draw item background ***

    painter->setPen(m_defPen);

    if (isLight)
    {
        painter->setBrush(m_itemBackLightBrush);
    }
    else
    {
        painter->setBrush(m_itemBackDarkBrush);
    }
    painter->drawRect(opt.rect);

    // *****************************

    // ***** Draw checkbox *********
    QStyleOptionButton checkboxStyleOption;

    checkboxStyleOption.state |= QStyle::State_Enabled;
    if (opt.checkState == Qt::Checked)
    {
        checkboxStyleOption.state |= QStyle::State_On;
    }
    else if (opt.checkState == Qt::PartiallyChecked)
    {
        checkboxStyleOption.state |= QStyle::State_NoChange;
    }
    else
    {
        checkboxStyleOption.state |= QStyle::State_Off;
    }
    if (hover)
    {
        checkboxStyleOption.state |= QStyle::State_MouseOver;
    }

    QRect checkBoxRect = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option);
    checkboxStyleOption.rect = checkBoxRect;
    checkboxStyleOption.rect.adjust(m_padding, 0, m_padding, 0);

    painter->setPen(Qt::NoPen);
    painter->setBrush(m_backBrush);
    int paintAtPos = checkBoxRect.width() + m_padding * 2;

    QRect bgr1Rect =
        QRect(opt.rect.left() + 1, opt.rect.top() + 1, checkBoxRect.width() + m_padding * 2, opt.rect.height() - 1);
    painter->drawRect(bgr1Rect);

    QApplication::style()->drawControl(QStyle::CE_CheckBox, &checkboxStyleOption, painter);
    // *****************************

    // ******** Draw icon **********
    QVariant decorationRole = index.model()->data(index, Qt::DecorationRole);
    if (!decorationRole.isNull() && decorationRole.canConvert(QVariant::Icon))
    {
        int iconWidth = m_sizeHint.width();
        paintAtPos += m_padding;
        opt.icon.paint(painter, opt.rect.x() + paintAtPos + 2, opt.rect.y() + 2, iconWidth - 4, opt.rect.height() - 4,
                       Qt::AlignCenter);
        paintAtPos += iconWidth;
    }
    // *****************************
    painter->restore();

    paintAtPos += m_padding;

    if (hover)
    {
        painter->save();
        painter->setPen(m_hoverPen);
        painter->drawRect(opt.rect.adjusted(0, 0, -1, -1));
        painter->restore();
    }
    QApplication::style()->drawItemText(painter, opt.rect.adjusted(paintAtPos, 0, 0, 0), Qt::AlignVCenter, opt.palette,
                                        true, opt.text);
}

QSize SitesCheckboxDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const
{
    return m_sizeHint;
}
