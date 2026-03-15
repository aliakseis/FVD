#pragma once

#include <QImage>
#include <QLabel>
#include <QPixmap>

#include "videodisplay.h"

/// Display that use QLabel to generate frames.
class WidgetDisplay : public QLabel, public VideoDisplay
{
    Q_OBJECT
public:
    WidgetDisplay(QWidget* parent = 0);
    ~WidgetDisplay() {}
    void renderFrame(const FPicture& frame, unsigned int videoGeneration) override;
    void displayFrame(unsigned int videoGeneration) override;
    AVPixelFormat preferablePixelFormat() const override;
    QPixmap toQPixmap() const override { return m_display; }

    void showPicture(const QImage& picture) override;
    void showPicture(const QPixmap& picture) override;

protected:
    QImage m_image;
    QPixmap m_display;

protected slots:
    virtual void currentDisplay();
signals:
    void display();
};
