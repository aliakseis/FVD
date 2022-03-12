#pragma once

#include <QDockWidget>
#include <QLabel>

class VideoDisplay;
class VideoWidget;
class PreviewPanelButton;
class PlayerHeader;
class MainWindow;

// Base interface for widgets which have manage button
class ManageWidget
{
public:
	// return child widget which allow manage the widget (hide and show)
	virtual QWidget* manageWidget() const = 0;
};

class CustomDockWidget : public QDockWidget
{
	Q_OBJECT
public:
	CustomDockWidget(QWidget* widget);
	void setDisplayForFullscreen(VideoDisplay* display);
	void setTitleBarWidget(PlayerHeader* header);

	enum VisibilityState
	{
		ShownDocked = 0,
		HiddenDocked,
		FullyHidden,		// on library tab
		Floating,
		FullScreen
	};

	void setVisibilityState(VisibilityState state);
	VisibilityState currentState() const { return m_state; }
	VisibilityState previousState() const { return m_prevState; }
	void initState();

private:
	void setTabsManageWidgetsVisible(bool visible = true);


protected:
	virtual void closeEvent(QCloseEvent* event) override;
	virtual bool eventFilter(QObject* obj, QEvent* event) override;
	virtual void keyPressEvent(QKeyEvent* event) override;

signals:
	bool enterFullscreen(bool f);

public slots:
	void onLeaveFullScreen();

private slots:
	void onTopLevelChanged(bool);

private:
	PreviewPanelButton* manageButton;
	VisibilityState m_state;
	VisibilityState m_prevState;
    VideoWidget* m_display{};
	MainWindow* m_parent;
};
