#pragma once

#include <QWidget>

namespace Ui
{
class VideoControl;
}

class VideoPlayerWidget;

class VideoControl : public QWidget
{
    Q_OBJECT

public:
    explicit VideoControl(VideoPlayerWidget* parent = 0);
    ~VideoControl();
    void setVolume(int volume, bool onlyWidget = false);

    int getHeight() const { return m_height; };
    int getWidth() const;

signals:
    void download();
    void browse(bool alt);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

public slots:
    void on_btnPlay_clicked();
    void on_btnPause_clicked();

private slots:
    void onProgramVolumeChange(double volume);
    void on_btnVolume_clicked();
    void on_btnStop_clicked();
    void on_btnBrowser_clicked();

    void onShowPlaybutton(bool show);

private:
    Ui::VideoControl* ui;
    VideoPlayerWidget* videoPlayer;
    int m_height;
    QPixmap background;
    QPixmap backgroundfs;
    bool m_isVolumeOn;
    int m_prevVolumeValue;

    void switchVolumeButton(bool volumeOn);
};
