#pragma once

// https://github.com/MasterAler/SampleYUVRenderer

#include <QException>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QScopedPointer>

#include "videodisplay.h"

class OpenGLDisplay : public QOpenGLWidget, public QOpenGLFunctions, public VideoDisplay
{
    Q_OBJECT
public:
    explicit OpenGLDisplay(QWidget* parent = nullptr);
    ~OpenGLDisplay() override;

    void InitDrawBuffer(unsigned bsize);

    void showPicture(const QImage& img) override;
    void showPicture(const QPixmap& picture) override;

    void renderFrame(const FPicture& frame) override;
    void displayFrame() override;

    AVPixelFormat preferablePixelFormat() const override;
    bool resizeWithDecoder() const override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    struct OpenGLDisplayImpl;
    QScopedPointer<OpenGLDisplayImpl> impl;
};

/***********************************************************************/

class OpenGlException : public QException
{
public:
    void raise() const override { throw *this; }
    OpenGlException* clone() const override { return new OpenGlException(*this); }
};
