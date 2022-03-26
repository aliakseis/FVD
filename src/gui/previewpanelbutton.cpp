#include "previewpanelbutton.h"

PreviewPanelButton::PreviewPanelButton(QWidget* parent) : QLabel(parent) {}

PreviewPanelButton::~PreviewPanelButton() = default;

void PreviewPanelButton::setImages(Type type)
{
    switch (type)
    {
    case LeftArrow:
        m_pixmap.load(QString::fromUtf8(":/previewpanel/left_default"));
        m_pushedPixmap.load(QString::fromUtf8(":/previewpanel/left_clicked"));
        break;
    case RightArrow:
        m_pixmap.load(QString::fromUtf8(":/previewpanel/right_default"));
        m_pushedPixmap.load(QString::fromUtf8(":/previewpanel/right_clicked"));
        break;
    default:  // Scale
        m_pixmap.load(QString::fromUtf8(":/previewpanel/scale_default"));
        m_pushedPixmap.load(QString::fromUtf8(":/previewpanel/scale_clicked"));
    }
    setPixmap(m_pixmap);
}

void PreviewPanelButton::mousePressEvent(QMouseEvent* event)
{
    if (!m_pushedPixmap.isNull())
    {
        setPixmap(m_pushedPixmap);
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
