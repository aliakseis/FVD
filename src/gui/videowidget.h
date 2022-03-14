#pragma once

#include <QImage>
#include <QRegion>
#include <QTimer>

#ifdef DEVELOPER_OPENGL
#include "player/opengldisplay.h"
#else
#include "player/widgetdisplay.h"
#endif
#include "remotevideoentity.h"
#include "imagecache.h"

class VideoPlayerWidget;

#ifdef DEVELOPER_OPENGL
class VideoWidget : public OpenGLDisplay
#else
class VideoWidget : public WidgetDisplay
#endif
{
	Q_OBJECT
	friend class VideoBlackScreen;
	Q_PROPERTY(QImage m_noPreviewImg READ noPreviewImage WRITE setNoPreviewImage);
public:
	explicit VideoWidget(VideoPlayerWidget* parent = 0);
	virtual ~VideoWidget();

	void setDefaultPreviewPicture();
	QSize getPictureSize() const { return m_pictureSize; }
	QPixmap originalFrame() const { return m_originalFrame; }
	QImage startImageButton() const { return m_startImgButton; }

	QImage noPreviewImage() const { return m_noPreviewImg; }
	void setNoPreviewImage(const QImage& noImage) { m_noPreviewImg = noImage; }

	QPixmap drawPreview(const QImage& fromImage);
	void hidePlayButton();

	void updatePlayButton();

protected:
	void keyPressEvent(QKeyEvent* event) override;

	void mousePressEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	void wheelEvent(QWheelEvent* event);
	void resizeEvent(QResizeEvent* event);

    VideoPlayerWidget* VideoPlayerWidgetInstance();

	QImage m_startImgButton;
	QImage m_noPreviewImg;
	QPixmap m_originalFrame;

private:
	bool m_playIndicator;
	QSize m_pictureSize;
	QImage m_defPlayButton, m_hoverPlayButton, m_clickedPlayButton;
	QImage* m_selImage;
	QImage m_fromImage;
	bool m_isMousePressed;
	const int m_playBtnRadius;
#ifdef Q_OS_LINUX
	bool m_resizeIndicator;
#endif
	ImageCache m_imageCache;

	qint64 m_lastMouseTime;

	QTimer m_cursorTimer;

	bool pointInButton(const QPoint& point);

	void showElements();
	void hideElements();

public Q_SLOTS:
	void fullScreen(bool isEnable = true);
	void setPreviewPicture(const RemoteVideoEntity* entity);

protected Q_SLOTS:
	virtual void currentDisplay();

private Q_SLOTS:
	void getImageFinished(const QImage& image);
	void onCursorTimer();
	void fullScreenProcess();
Q_SIGNALS:
	void leaveFullScreen();
	void mouseClicked();
};
