#pragma once

#include <QHBoxLayout>
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "ui_utils/mainwindowwithtray.h"
#include "ui_utils/taskbar.h"
#include "utilities/singleton.h"

namespace Ui
{
class MainWindow;
}

class QEvent;
class QAction;
class QStackedWidget;
class DownloadsForm;
class SearchResultForm;
class MainToolBar;
class LibraryForm;
class DownloadEntity;
class VideoPlayerWidget;
class RemoteVideoEntity;
class QFileSystemWatcher;
class CustomDockWidget;
class VideoControl;

class MainWindow : public ui_utils::MainWindowWithTray
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = 0);

    ~MainWindow();

    CustomDockWidget* dockWidget();
    QWidget* videoControlWidget();
    VideoPlayerWidget* getPlayer();

    // Getters
    SearchResultForm* searchForm();
    DownloadsForm* downloadsForm();
    LibraryForm* libraryForm();

    void nextTab();
    void prevTab();

    void openDownloadsTab(DownloadEntity* selEntity);
    void openLibraryTab(DownloadEntity* selEntity);

    int getCurrTabIndex();

    static MainWindow* Instance();

public Q_SLOTS:
    void onSearchItemActivated(RemoteVideoEntity* entity,
        const DownloadEntity* dEntity = nullptr, int rowNumber = 0);
    void askForSavingModel();

private Q_SLOTS:
    void condsiderSavingModel();
    void activateDownloadsForm(const DownloadEntity* selEntity);
    void activateSearchForm();
    void activateLibraryForm(const DownloadEntity* selEntity);
    void about();
    void openPreferences();
    void onDownloadItemActivated(const DownloadEntity* entity, int rowNumber);
    void prepareToExit();
    void resetFileSystemWatcher();
    void onDownloadProgressChanged(int progress);
    void onDownloadFinished(const QString& videoTitle);

    void onShowPlaybutton(bool show);

    void onPeriodicUpdate();

protected:
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

    void showHideNotify() override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void loadModelData();
    void saveModelData();

private:
    Ui::MainWindow* ui;

    QWidget m_dockTitleBar;
    QHBoxLayout m_dockTitleBarLayout;
    QVBoxLayout m_dockWidgetLayout;

    DownloadsForm* m_downloadsForm;

    QStackedWidget* m_centralWidget;
    MainToolBar* m_mainToolbar;
    SearchResultForm* m_searchResultFrom;
    LibraryForm* m_libraryForm;

    QFileSystemWatcher* libraryWatcher;
    static bool m_isInitialized;
    bool m_isModelFileOutdated;

#if defined(Q_OS_WIN)
    ui_utils::TaskBar m_taskBar;

    HICON m_hPlay{};
    HICON m_hPause{};

    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
#endif

    QTimer m_askForSavingModelTimer;

    bool m_ShowSysTrayNotification = true;

    QString m_gigsAvailable;

    QTimer m_periodicUpdateTimer;
};  // class MainWindow
