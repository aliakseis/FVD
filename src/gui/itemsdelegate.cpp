#include "itemsdelegate.h"

#include <qdrawutil.h>

#include <QApplication>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QTreeView>

#include "globals.h"
#include "searchlistmodel.h"
#include "utilities/translation.h"

enum
{
    MAX_BTN_WIDTH = 200,
    MIN_BTN_WIDTH = 100
};

ItemsDelegate::ItemsDelegate(QObject* parent)
    : QStyledItemDelegate(parent),
      m_pixmap(QPixmap(":/downloadm")),
      m_btnDisabled(QPixmap(":/downloadm_disabled")),
      m_pixmapHover(QPixmap(":/downloadm_hover")),
      m_pixmapDown(QPixmap(":/downloadm_down")),
      m_pixmapDown2(QPixmap(":/downloadm_down2")),
      m_pixmapDropdown(QPixmap(":/images/style/progress_dropdown.png")),
      m_mouseRow(-1),
      m_mouseColumn(-1),
      m_mouseDownRow(-1),
      m_mouseDownColumn(-1),
      m_mouseDown(false),
      m_btnElement(None)
{
}

QWidget* ItemsDelegate::createEditor(QWidget* /*parent*/, const QStyleOptionViewItem& /*option*/,
                                     const QModelIndex& /*index*/) const
{
    return nullptr;
}

void ItemsDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    if (index.column() == SR_Icon)
    {
        QString iconName = index.data(Qt::UserRole).toString();
        QImage image(QString(":/sites/") + iconName + "-logo");
        if (!image.isNull())
        {
            QRect rect = option.rect;
            rect.adjust(1, 1, -1, -1);
            rect.setWidth(30);
            rect.setHeight(30);
            painter->drawImage(rect, image);
        }
    }
    else if (index.column() == SR_Status)
    {
        const auto* model = qobject_cast<const QSortFilterProxyModel*>(index.model());
        QVariant progress = model->data(index);
        if (!progress.isValid() || progress.type() == QVariant::Int)
        {
            QRect rect = option.rect;
            if (rect.width() > MAX_BTN_WIDTH)
            {
                rect.setWidth(MAX_BTN_WIDTH);
            }
            else if (rect.width() < MIN_BTN_WIDTH)
            {
                rect.setWidth(MIN_BTN_WIDTH);
            }

            rect.adjust(3, 4, -4, -4);
            int row = index.row();
            int column = index.column();

            if (progress.type() == QVariant::Int)
            {
                qDrawBorderPixmap(painter, rect, QMargins(2, 2, 28, 2), m_btnDisabled);
            }
            else if (m_mouseDown && m_mouseDownRow == m_mouseRow && m_mouseDownColumn == m_mouseColumn &&
                     row == m_mouseRow && column == m_mouseColumn)
            {
                if (m_btnElement == DownloadButton)
                {
                    qDrawBorderPixmap(painter, rect, QMargins(2, 2, 28, 2), m_pixmapDown);
                }
                else if (m_btnElement == MenuIndicator)
                {
                    qDrawBorderPixmap(painter, rect, QMargins(2, 2, 28, 2), m_pixmapDown2);
                }
                else
                {
                    qDrawBorderPixmap(painter, rect, QMargins(2, 2, 28, 2), m_pixmapHover);
                }
            }
            else
            {
                if (option.state & QStyle::State_MouseOver && m_mouseColumn == SR_Status)
                {
                    qDrawBorderPixmap(painter, rect, QMargins(2, 2, 28, 2), m_pixmapHover);
                }
                else
                {
                    qDrawBorderPixmap(painter, rect, QMargins(2, 2, 28, 2), m_pixmap);
                }
            }

            QRectF rectf(rect);
            rectf.adjust(0, 0, -24, 0);
            painter->save();
            painter->setFont(QFont("Segoe UI", 10));
            painter->setPen(QColor("#4e6a31"));
            painter->drawText(rectf, utilities::Tr::Tr(DOWNLOAD_BUTTON), QTextOption(Qt::AlignCenter));
            painter->restore();
        }
        else
        {
            Q_ASSERT(progress.type() == QVariant::Double || progress.userType() == QMetaType::Float);

            QStyleOptionProgressBar progressBarOption;
            progressBarOption.state = QStyle::State_Enabled;
            progressBarOption.direction = QApplication::layoutDirection();
            progressBarOption.rect = option.rect.adjusted(3, 5, -3, -4);
            if (progressBarOption.rect.width() > MAX_BTN_WIDTH)
            {
                progressBarOption.rect.setWidth(MAX_BTN_WIDTH - 8);  // duno why -8
            }

            progressBarOption.fontMetrics = QApplication::fontMetrics();
            progressBarOption.minimum = 0;
            progressBarOption.maximum = 100;
            progressBarOption.textAlignment = Qt::AlignCenter;
            progressBarOption.textVisible = true;

            QModelIndex sourceIndex = model->mapToSource(index);
            const auto* sourceModel = qobject_cast<const SearchListModel*>(sourceIndex.model());
            RemoteVideoEntity* entity = sourceModel->item(sourceIndex.row());

            float progressFloat = progress.toFloat();
            if (progress.isNull())
            {
                progressBarOption.text = Tr::Tr(TREEVIEW_PPEPARING_STATUS);
            }
            else
            {
                progressBarOption.progress = static_cast<int>(progressFloat);
                progressBarOption.text = QString::number(progress.toFloat(), 'f', 2) + "%";
            }

            switch (entity->state())
            {
            case Downloadable::kFinished:
                progressBarOption.progress = progressBarOption.maximum;
                progressBarOption.text = tr("Completed");
                break;
            case Downloadable::kPaused:
                progressBarOption.text = tr("Paused");
                break;
            case Downloadable::kCanceled:
                progressBarOption.text = tr("Canceled");
                break;
            case Downloadable::kFailed:
                progressBarOption.text = tr("Failed");
                break;
            case Downloadable::kQueued:
                progressBarOption.text = tr("Queued");
                break;
            default:;
            }

            QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);

            painter->setOpacity(0.7);
            painter->drawPixmap(option.rect.right() - 31, option.rect.top() + 9, m_pixmapDropdown.width(),
                                m_pixmapDropdown.height(), m_pixmapDropdown);
            painter->setOpacity(1);
        }
    }
}

QSize ItemsDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const
{
    if (index.row() == SR_Icon)
    {
        return {32, 32};
    }
    if (index.row() == SR_Status)
    {
        return {MIN_BTN_WIDTH, 32};
    }

    return {90, 32};
}

bool ItemsDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                                const QModelIndex& index)
{
    if (event->type() == QEvent::MouseMove)
    {
        m_mouseRow = index.row();
        m_mouseColumn = index.column();
    }
    else if (event->type() == QEvent::MouseButtonPress)
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);

        m_mouseDownRow = index.row();
        m_mouseDownColumn = index.column();
        m_mouseRow = m_mouseDownRow;
        m_mouseColumn = m_mouseDownColumn;
        m_btnElement = None;

        if (m_mouseColumn == SR_Status && mouseEvent->button() == Qt::LeftButton)
        {
            auto* tw = qobject_cast<QTreeView*>(this->parent());
            tw->update(index);
            m_mouseDown = true;
            QRect rect = option.rect;

            if (rect.width() > MAX_BTN_WIDTH)
            {
                rect.setWidth(MAX_BTN_WIDTH);
            }
            else if (rect.width() < MIN_BTN_WIDTH)
            {
                rect.setWidth(MIN_BTN_WIDTH);
            }
            if (index.data().toString().length() != 0)
            {
                if (utilities::IsInBounds(rect.top() + 9, mouseEvent->pos().y(), rect.bottom() - 5) &&
                    utilities::IsInBounds(5, rect.right() - mouseEvent->pos().x(), 25))
                {
                    m_btnElement = MenuIndicator;
                    return true;
                }

                return false;
            }

            if (rect.right() - mouseEvent->pos().x() > 30)
            {
                qDebug() << "DownloadButton";
                m_btnElement = DownloadButton;
                return true;
            }
            if (rect.right() - mouseEvent->pos().x() > 0)
            {
                qDebug() << "MenuIndicator";
                m_btnElement = MenuIndicator;
                return true;
            }

            m_btnElement = None;
            return false;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        m_mouseRow = index.row();
        m_mouseColumn = index.column();
        m_mouseDown = false;

        if (m_mouseColumn == SR_Status && m_mouseDownColumn == m_mouseColumn && m_mouseDownRow == m_mouseRow)
        {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                const auto* model = qobject_cast<const QSortFilterProxyModel*>(index.model());
                int row = model->mapToSource(index).row();
                if (m_btnElement == DownloadButton)
                {
                    emit downloadClicked(row);
                }
                else if (m_btnElement == MenuIndicator)
                {
                    emit downloadMenuClicked(row, mouseEvent->globalPos());
                }
            }
        }

        auto* tw = qobject_cast<QTreeView*>(this->parent());
        tw->update(index);
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
