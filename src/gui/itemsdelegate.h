#pragma once

#include <QStyledItemDelegate>

class ItemsDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ItemsDelegate(QObject* parent = 0);

    enum PressedElement
    {
        None,
        DownloadButton,
        MenuIndicator
    };

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

signals:
    void downloadClicked(int row);
    void downloadMenuClicked(int row, QPoint point);

protected:
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index);

private:
    QPixmap m_pixmap;
    QPixmap m_btnDisabled;
    QPixmap m_pixmapHover;
    QPixmap m_pixmapDown;
    QPixmap m_pixmapDown2;
    QPixmap m_pixmapDropdown;
    int m_mouseRow;
    int m_mouseColumn;
    int m_mouseDownRow;
    int m_mouseDownColumn;
    bool m_mouseDown;
    PressedElement m_btnElement;
};
