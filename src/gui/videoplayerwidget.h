#pragma once

#include <QFrame>
#include <QPointer>

#include "downloadentity.h"
#include "player/videoplayer.h"

class VideoControl;
class VideoProgressBar;
class RemoteVideoEntity;
class VideoBlackScreen;
class DescriptionPanel;
class VideoWidget;
class Spinner;
class PlayerHeader;

class VideoPlayerWidget : public QFrame, public VideoPlayer
{
    Q_OBJECT
public:
    explicit VideoPlayerWidget(QWidget* parent = 0);
    virtual ~VideoPlayerWidget();

    void pauseVideo();
    void resumeVideo();

    /// \fn	void VideoPlayerWidget::stopVideo(bool showDefaultImage = false);
    ///
    /// \brief	Stops a video.
    ///
    /// \param	showDefaultImage	(Optional) the show default image.

    void stopVideo(bool showDefaultImage = false);
    bool isPaused();
    void seekByPercent(float percent, int64_t totalDuration = -1);

    VideoDisplay* getCurrentDisplay();
    VideoWidget* videoWidget() { return m_videoWidget; }

    void setProgressbar(VideoProgressBar* progressbar);
    void setVideoHeader(PlayerHeader* videoHeader);
    void setControl(VideoControl* controlWidget);

    void startPreviewDownload();  // TODO: rename and change logic
    void setDefaultPreviewPicture();

    QString currentFilename() const;

    void updateLayout(bool fromPendingHeaderPaused = false);

    void setEntity(RemoteVideoEntity* entity, DownloadEntity* downloadEntity, int rowNumber);
    DownloadEntity* currentDownload() const { return m_currentDownload; }
    RemoteVideoEntity* entity() const { return m_currentEntity;  }

    void exitFullScreen();

private:
    void playFile(QString fileName);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;
    void setDefaultDisplay() override;

public slots:
    void playDownloadEntity(DownloadEntity* entity);
    void playPauseButtonAction();

private slots:
    void processPreviewEntity();
    void downloadingToPreview(qint64 bytesReceived, qint64 bytesTotal);
    void setVideoFilename(const QString& fileName);
    void openVideoInBrowser(bool alt);
    void onPlayDownloadEntityAsynchronously(const QPointer<DownloadEntity>& entity);
    void onDownloadStateChanged(Downloadable::State newState, Downloadable::State prevState);
    void onCustomContextMenuRequested(const QPoint& pos);

    /// \fn	void VideoPlayerWidget::updateViewOnVideoStop(bool showDefaultImage = true);
    ///
    /// \brief	Unties video widget from DE and RWE, updates internal controls according new state. Emits \ref
    /// fileReleased()
    ///
    /// \param	showDefaultImage	(Optional) true to update preview image to the currently selected item.
    void updateViewOnVideoStop(bool showDefaultImage = true);

    void onPlayingFinished();
    void disableSeeking();

    void showSpinner();
    void hideSpinner();

signals:
    void fileReleased();
    void playingDownloadEntity(const DownloadEntity* entity);
    void playDownloadEntityAsynchronously(const QPointer<DownloadEntity>& entity);

    void showPlaybutton(bool show);

private:
    friend class VideoControl;
    friend class VideoBlackScreen;
    friend class VideoWidget;
    friend class DescriptionPanel;

    DescriptionPanel* m_descriptionPanel;
    VideoControl* m_controls;
    VideoProgressBar* m_progressBar;
    QString m_currentFile;
    QPointer<DownloadEntity> m_currentDownload;
    QPointer<DownloadEntity> m_selectedDownload;
    Spinner* m_spinner;
    VideoWidget* m_videoWidget;
    PlayerHeader* m_playerHeader{};
    RemoteVideoEntity* m_currentEntity{};
    int m_selectedRowNumber = 0;
    bool m_repeatedPlaying = false;
};

VideoPlayerWidget* VideoPlayerWidgetInstance();

bool fileIsInUse(VideoPlayerWidget* videoPlayer, DownloadEntity* entity);
