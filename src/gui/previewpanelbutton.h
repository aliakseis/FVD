#pragma once

#include <QLabel>

class PreviewPanelButton : public QLabel
{
    Q_OBJECT
public:
    enum Type
    {
        LeftArrow,
        RightArrow,
        Scale
    };

    PreviewPanelButton(QWidget* parent);
    virtual ~PreviewPanelButton();
    void setImages(Type type);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPixmap m_pressedPixmap;
    QPixmap m_pixmap;
};
