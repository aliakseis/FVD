#pragma once

#include <QProgressBar>
#include <QPixmap>

class VideoProgressBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit VideoProgressBar(QWidget* parent = 0);

    virtual ~VideoProgressBar();
    void resetProgress();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    int getClickerOffset();

    QPixmap m_clicker;

    double m_downloadedRatio{};
    double m_playedRatio{};

    /**
     * True if mouse button pressed
     */
    bool m_btn_down;

    /**
     * Indicator of disabled seeking
     */
    bool m_seekDisabled;

    /**
     * Total original size of the file
     */
    qint64 m_downloadedTotalOriginal;
public slots:
    void setDownloadedCounter(double downloaded);
    void setPlayedCounter(double played);
    void seekingEnable(bool enable = true);
public slots:
    void displayDownloadProgress(qint64 downloaded, qint64 total);
    void displayPlayedProgress(qint64 frame, qint64 total);
};
