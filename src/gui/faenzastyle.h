#pragma once

#include <QProxyStyle>

class FaenzaStyle : public QProxyStyle
{
    Q_OBJECT
public:
    FaenzaStyle() {}
    ~FaenzaStyle() {}

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget) const override;
};
