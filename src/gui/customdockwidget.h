#pragma once

#include <QDockWidget>
#include <QLabel>

class VideoDisplay;
class VideoWidget;
class PreviewPanelButton;
class PlayerHeader;
class MainWindow;


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
        FullyHidden,  // on library tab
        Floating,
        FullScreen
    };

    void setVisibilityState(VisibilityState state);
    VisibilityState currentState() const { return m_state; }
    VisibilityState previousState() const { return m_prevState; }

private:
    void setTabsManageWidgetsVisible(bool visible = true);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

signals:
    bool enterFullscreen(bool f);

public slots:
    void onLeaveFullScreen();

private slots:
    void onTopLevelChanged(bool);

private:
    PreviewPanelButton* manageButton{};
    VisibilityState m_state;
    VisibilityState m_prevState;
    VideoWidget* m_display{};
    MainWindow* m_parent;
};
