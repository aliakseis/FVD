#include "previewpanelbutton.h"

PreviewPanelButton::PreviewPanelButton(QWidget* parent) : QLabel(parent) {}

PreviewPanelButton::~PreviewPanelButton() = default;

void PreviewPanelButton::setImages(Type type)
{
    switch (type)
    {
    case LeftArrow:
        m_pixmap.load(":/previewpanel/left_default");
        m_pressedPixmap.load(":/previewpanel/left_clicked");
        break;
    case RightArrow:
        m_pixmap.load(":/previewpanel/right_default");
        m_pressedPixmap.load(":/previewpanel/right_clicked");
        break;
    default:  // Scale
        m_pixmap.load(":/previewpanel/scale_default");
        m_pressedPixmap.load(":/previewpanel/scale_clicked");
    }
    setPixmap(m_pixmap);
}

void PreviewPanelButton::mousePressEvent(QMouseEvent* event)
{
    if (!m_pressedPixmap.isNull())
    {
        setPixmap(m_pressedPixmap);
    }
    QLabel::mousePressEvent(event);
}

void PreviewPanelButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_pixmap.isNull())
    {
        setPixmap(m_pixmap);
    }
    QLabel::mouseReleaseEvent(event);
}
