#ifndef DOWNLOADSCONTROL_H
#define DOWNLOADSCONTROL_H

#include <QWidget>
#include <QTimer>
#include <QEventLoop>

class QPropertyAnimation;
class DownloadSortFilterModel;

namespace Ui
{
class DownloadsControl;
}

class DownloadsControl : public QWidget
{
	Q_OBJECT

public:
	explicit DownloadsControl(QWidget* parent = 0, DownloadSortFilterModel* model = nullptr);
	~DownloadsControl();

	void showAtCursor(int affectedRow = -1);
	int affectedRow() const;
	enum State { Started, Paused };

	void setState(State state, bool isUpEnabled, bool isDownEnabled, bool isStopEnabled, bool isStartEnabled, int speedLimit);

public Q_SLOTS:
	void show();
	void hide();

Q_SIGNALS:
	void start();
	void stop();
	void pause();
	void reload();
	void up();
	void down();
	void remove();
	void setSpeedLimit(int);

private Q_SLOTS:
	void on_btnStart_clicked();
	void on_btnPause_clicked();
	void on_btnStop_clicked();
	void on_btnRemove_clicked();
	void on_btnRestart_clicked();
	void on_btnUp_clicked();
	void on_btnDown_clicked();

	void on_cbTrafficLimit_stateChanged(int);
	void on_sbTrafficLimit_valueChanged(int);

	void onTimeout();

protected:
	virtual void resizeEvent(QResizeEvent* event)  override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void focusInEvent(QFocusEvent* event)  override;
	virtual void focusOutEvent(QFocusEvent* event) override;

private:
	Ui::DownloadsControl* ui;
	QTimer timer;
	const int timeout;
	int m_affectedRow;
	QPropertyAnimation* m_animation;
	DownloadSortFilterModel* m_proxyModel;
	bool dontStopTimer;
	QEventLoop exitWaitLoop;

	bool isUpdatingState;
};

#endif // DOWNLOADSCONTROL_H
