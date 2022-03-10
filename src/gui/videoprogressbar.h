#pragma once

#include <QProgressBar>

class VideoProgressBar : public QProgressBar
{
	Q_OBJECT
public:
	explicit VideoProgressBar(QWidget* parent = 0);

	virtual ~VideoProgressBar();
	int getScale() const;
	void resetProgress();

protected:
	virtual void paintEvent(QPaintEvent* event) override;
	bool eventFilter(QObject* obj, QEvent* event) override;

private:

	/**
	 * The downloaded count (in scale)
	 */
	int m_downloaded;

	/**
	 * The played count (in scale)
	 */
	int m_played;

	/**
	 * One scale delimeter for m_downloaded and m_played
	 */
	int m_scale;

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
	void setDownloadedCounter(int downloaded);
	void setPlayedCounter(int played);
	void seekingEnable(bool enable = true);
public slots:
	void displayDownloadProgress(qint64 downloaded, qint64 total);
	void displayPlayedProgress(qint64 frame, qint64 total);


};
