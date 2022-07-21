#include "mainwindow.h"

#include <QAction>
#include <QDebug>
#include <QDesktopWidget>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QPushButton>

#include "branding.hxx"
#include "downloadentity.h"
#include "downloadsform.h"
#include "global_functions.h"
#include "globals.h"
#include "libraryform.h"
#include "librarymodel.h"
#include "maintoolbar.h"
#include "playerheader.h"
#include "preferences.h"
#include "searchmanager.h"
#include "searchresultform.h"
#include "settings_declaration.h"
#include "ui_mainwindow.h"
#include "utilities/customutf8codec.h"
#include "utilities/event_timer.h"
#include "utilities/filesaveguard.h"
#include "utilities/loggertag.h"
#include "utilities/modeldeserializer.h"
#include "utilities/modelserializer.h"
#include "utilities/translation.h"
#include "version.hxx"
#include "videoplayerwidget.h"
#include "videowidget.h"


#if defined (Q_OS_WIN)
#include <windows.h>
#elif defined (Q_OS_MAC)
#include "darwin/DarwinSingleton.h"
#endif  // Q_OS_MAC
#include "ui_utils/uiutils.h"

namespace {

#if defined(Q_OS_WIN)

HICON LoadIcon(const wchar_t* idr)
{
    const auto appInstance = static_cast<HINSTANCE>(GetModuleHandle(nullptr));

    int const cxButton = GetSystemMetrics(SM_CXSMICON);
    return (HICON) LoadImageW(
        appInstance,
        idr,
        IMAGE_ICON,
        cxButton, cxButton, // use actual size
        LR_DEFAULTCOLOR);
}

#endif

} // namespace

const char WindowGeometry[] = "WindowGeometry";
const char WindowState[] = "WindowState";

bool MainWindow::m_isInitialized = false;


MainWindow::MainWindow(QWidget* parent)
    : ui_utils::MainWindowWithTray(parent, QIcon(":/images/fvdownloader.png"), PROJECT_FULLNAME_TRANSLATION),
      ui(new Ui::MainWindow),
      m_centralWidget(new QStackedWidget(this)),
      m_playerHeader(new PlayerHeader(this)),
      m_mainToolbar(new MainToolBar(this)),
      m_searchResultFrom(new SearchResultForm(this)),
      m_libraryForm(new LibraryForm(this)),
      libraryWatcher(nullptr),
      m_isModelFileOutdated(false)
{
    ui->setupUi(this);
    setContextMenuPolicy(Qt::NoContextMenu);
    Tr::SetTr(this, &QMainWindow::setWindowTitle, PROJECT_FULLNAME_TRANSLATION);

    m_player = ui->dockFrame;

    m_player->setProgressbar(ui->videoProgress);
    m_player->setVideoHeader(m_playerHeader);
    m_downloadsForm = new DownloadsForm(m_player, this);

    ui->dockWidget->installEventFilter(m_player);
    ui->dockWidget->setTitleBarWidget(m_playerHeader);
    ui->dockWidget->setDisplayForFullscreen(m_player->getCurrentDisplay());

    ui->mainToolBar->addWidget(m_mainToolbar);

    m_centralWidget->addWidget(m_searchResultFrom);
    m_searchResultFrom->manageWidget()->installEventFilter(ui->dockWidget);
    m_centralWidget->addWidget(m_downloadsForm);
    m_downloadsForm->manageWidget()->installEventFilter(ui->dockWidget);
    m_centralWidget->addWidget(m_libraryForm);
    m_libraryForm->manageWidget()->installEventFilter(ui->dockWidget);
    setCentralWidget(m_centralWidget);

    VERIFY(connect(m_player->videoWidget(), SIGNAL(leaveFullScreen()), ui->dockWidget, SLOT(onLeaveFullScreen())));
    VERIFY(connect(m_mainToolbar, SIGNAL(settings()), SLOT(openPreferences())));
    VERIFY(connect(m_mainToolbar, SIGNAL(search()), SLOT(activateSearchForm())));
    VERIFY(connect(m_mainToolbar, SIGNAL(downloads(const DownloadEntity*)),
                   SLOT(activateDownloadsForm(const DownloadEntity*))));
    VERIFY(connect(m_mainToolbar, SIGNAL(library(const DownloadEntity*)),
                   SLOT(activateLibraryForm(const DownloadEntity*))));
    VERIFY(connect(m_mainToolbar, SIGNAL(help()), SLOT(about())));
    VERIFY(connect(m_searchResultFrom, SIGNAL(entityActivated(RemoteVideoEntity*)),
                   SLOT(onSearchItemActivated(RemoteVideoEntity*))));
    VERIFY(connect(m_downloadsForm, SIGNAL(entityActivated(const DownloadEntity*)),
                   SLOT(onDownloadItemActivated(const DownloadEntity*))));
    VERIFY(connect(qApp, SIGNAL(aboutToQuit()), SLOT(prepareToExit())));

    VERIFY(connect(ui->actionAbout, SIGNAL(triggered()), SLOT(about())));
    VERIFY(connect(ui->actionConfigure, SIGNAL(triggered()), SLOT(openPreferences())));

    VERIFY(connect(&SearchManager::Instance(), SIGNAL(downloadProgressChanged(int)),
                   SLOT(onDownloadProgressChanged(int))));
    VERIFY(connect(&SearchManager::Instance(), SIGNAL(downloadFinished(const QString&)),
                   SLOT(onDownloadFinished(const QString&))));

    m_askForSavingModelTimer.setSingleShot(true);
    VERIFY(connect(&m_askForSavingModelTimer, SIGNAL(timeout()), SLOT(condsiderSavingModel())));

    connect(m_player, &VideoPlayerWidget::showPlaybutton, this, &MainWindow::onShowPlaybutton);

#ifdef Q_OS_MAC
    VERIFY(connect(&DarwinSingleton::Instance(), SIGNAL(showPreferences()), SLOT(openPreferences())));
    VERIFY(connect(&DarwinSingleton::Instance(), SIGNAL(showAbout()), SLOT(about())));
#endif  // Q_OS_MAC

    loadModelData();

    resetFileSystemWatcher();

    restoreGeometry(QSettings().value(WindowGeometry, saveGeometry()).toByteArray());
    // adjust window size to screen if low resolution
    QSize screenSize = QApplication::desktop()->availableGeometry().size();
    if (size().width() > screenSize.width() || size().height() > screenSize.height())
    {
        resize(screenSize);
    }

    ui->dockWidget->initState();

    ui_utils::ensureWidgetIsOnScreen(this);

    // add tray menu actions
    addTrayMenuItem(TrayMenu::Show);
    addTrayMenuItem(ui->actionAbout);
    addTrayMenuItem(ui->actionConfigure);
    addTrayMenuItem(TrayMenu::Separator);
    addTrayMenuItem(TrayMenu::Exit);

    VERIFY(QMetaObject::invokeMethod(m_mainToolbar, "search", Qt::QueuedConnection));

    setAcceptDrops(true);

#if defined(Q_OS_WIN)
    m_hPlay = LoadIcon(L"IDI_ICON_PLAY");
    m_hPause = LoadIcon(L"IDI_ICON_PAUSE");
#endif

    m_isInitialized = true;
}

MainWindow::~MainWindow()
{
    SearchManager::Instance().clearScriptStrategies();
    delete ui;
}

#if defined(Q_OS_WIN)
bool MainWindow::nativeEvent(const QByteArray& /*eventType*/, void* message, long* /*result*/)
{
    if (auto msg = static_cast<MSG*>(message))
    {
        if (msg->message == ui_utils::TaskBar::InitMessage())
        {
            m_taskBar.Init(winId());
            m_taskBar.setButton(m_hPlay, tr("Play"));
        }
        else if (msg->message == WM_COMMAND 
            && LOWORD(msg->wParam) == ui_utils::BUTTON_HIT_MESSAGE && HIWORD(msg->wParam) == 0x1800)
        {
            m_player->playPauseButtonAction();
        }
    }
    return false;
}
#endif

void MainWindow::closeEvent(QCloseEvent* event)
{
    QSettings settings;
    settings.setValue(WindowGeometry, saveGeometry());
    settings.setValue(WindowState, saveState());
    if (settings.value(app_settings::QuitAppWhenClosingWindow, app_settings::QuitAppWhenClosingWindow_Default).toBool())
    {
        closeApp();
    }
    else
    {
        ui_utils::MainWindowWithTray::closeEvent(event);
    }
}

void MainWindow::prepareToExit()
{
    m_player->exitFullScreen();
    if ((m_downloadsForm != nullptr) &&
        QSettings().value(app_settings::ClearDownloadsOnExit, app_settings::ClearDownloadsOnExit_Default).toBool())
    {
        m_downloadsForm->clearDownloadsList(true /*silent*/);
    }

    m_searchResultFrom->saveSitesList();

    saveModelData();

#ifdef Q_OS_WIN
    m_taskBar.Uninit();
#endif
}

void MainWindow::onDownloadProgressChanged(int progress)
{
#ifdef Q_OS_WIN
    m_taskBar.setProgress(progress);
#endif
}

void MainWindow::onDownloadFinished(const QString& videoTitle)
{
    if (videoTitle.length() > 0)
    {
        const int maxLen = 150;
        if (videoTitle.length() > maxLen)
        {
            QString truncateTitle(videoTitle);
            truncateTitle.truncate(maxLen);
            truncateTitle.append("...");
            showTrayMessage("'" + truncateTitle + tr("' has been downloaded successfully"));
        }
        else
        {
            showTrayMessage("'" + videoTitle + tr("' has been downloaded successfully"));
        }
    }
}

void MainWindow::activateDownloadsForm(const DownloadEntity* selEntity)
{
    if (ui->dockWidget->currentState() == CustomDockWidget::FullyHidden)
    {
        ui->dockWidget->setVisibilityState(ui->dockWidget->previousState());
    }

    m_centralWidget->setCurrentWidget(m_downloadsForm);
    m_downloadsForm->onActivated(selEntity);
}

void MainWindow::activateSearchForm()
{
    if (ui->dockWidget->currentState() == CustomDockWidget::FullyHidden)
    {
        ui->dockWidget->setVisibilityState(ui->dockWidget->previousState());
    }

    m_centralWidget->setCurrentWidget(m_searchResultFrom);
    m_searchResultFrom->onActivated();
}

void MainWindow::activateLibraryForm(const DownloadEntity* selEntity)
{
    ui->dockWidget->setVisibilityState(CustomDockWidget::FullyHidden);
    m_centralWidget->setCurrentWidget(m_libraryForm);
    m_libraryForm->onActivated(selEntity);
}

void MainWindow::about()
{
    raise();
    activateWindow();

    QMessageBox::about(this, QString(::Tr::Tr(ABOUT_TITLE)).arg(PROJECT_NAME),
                       '\n' + ::Tr::Tr(PROJECT_FULLNAME_TRANSLATION) +
                           " " PROJECT_VERSION "\n" PROJECT_DOMAIN "/" PROJECT_COMPANYNAME "/" PROJECT_NAME);
}

void MainWindow::openPreferences()
{
    static QPointer<Preferences> pPrefs;
    if (pPrefs.isNull())
    {
        pPrefs = new Preferences(this);

        VERIFY(connect(pPrefs, SIGNAL(newPreferencesApply()), m_downloadsForm, SLOT(onPreferencesChanged())));
        VERIFY(connect(pPrefs, SIGNAL(newPreferencesApply()), m_searchResultFrom, SLOT(onPreferencesChanged())));
        VERIFY(connect(pPrefs, SIGNAL(newPreferencesApply()), SLOT(resetFileSystemWatcher())));
    }

    raise();
    activateWindow();

    pPrefs->show();
}

void MainWindow::onSearchItemActivated(RemoteVideoEntity* entity, const DownloadEntity* dEntity)
{
    m_player->setEntity(entity);
    // TODO: move this logic to setEntity(...)
    if (entity != nullptr)
    {
        if (m_player->state() == VideoPlayerWidget::InitialState || sender() == m_player)
        {
            if (dEntity != nullptr)
            {
                ui->descriptionWidget->setDescription(entity->m_videoInfo.strategyName, entity->m_videoInfo.description,
                                                      dEntity->currentResolution());
            }
        }
    }
    else
    {
        if (m_player->state() == VideoPlayerWidget::InitialState || sender() == m_player)
        {
            resetDockWidgetPlayer();
        }
    }
}

void MainWindow::onDownloadItemActivated(const DownloadEntity* entity)
{
    if (entity != nullptr)
    {
        onSearchItemActivated(entity->getParent(), entity);
    }
    else if (m_player->state() == VideoPlayerWidget::InitialState)
    {
        resetDockWidgetPlayer();
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        utilities::Tr::RetranslateAll(this);
    }
    else if (event->type() == QEvent::ActivationChange && !isActiveWindow())
    {
        QMetaObject::invokeMethod(m_downloadsForm, "hideFloatingControl", Qt::QueuedConnection);
    }

    QMainWindow::changeEvent(event);
}

CustomDockWidget* MainWindow::dockWidget() { return ui->dockWidget; }

QWidget* MainWindow::videoControlWidget() { return ui->videoControl; }

VideoPlayerWidget* MainWindow::getPlayer() { return m_player; }

void MainWindow::keyPressEvent(QKeyEvent* event)
{
#ifdef DEVELOPER_FEATURES
    if (event->key() == Qt::Key_F5)
    {
        QFile file(QApplication::applicationDirPath() + "/style.css");
        qDebug() << "Apply styleshet " << file.fileName();
        file.open(QFile::ReadOnly);
        QString style = file.readAll();
        qApp->setStyleSheet(style);
        file.close();
    }
#endif  // DEVELOPER_FEATURES

#ifdef DEVELOPER_FEATURES
    if ((event->key() == Qt::Key_T) && (event->modifiers() == Qt::AltModifier))
    {
        EventTimer timer;
        for (int i = 0; i < 100; ++i)
        {
            m_player->startPreviewDownload();
            timer.exec(200);
            m_player->stopVideo(true);
            timer.exec(200);
        }
    }
#endif  // DEVELOPER_FEATURES
    if ((event->nativeVirtualKey() == Qt::Key_X) && (event->modifiers() == Qt::AltModifier))
    {
        closeApp();
    }
    else
    {
        // Player fullscreen in
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
            ((event->modifiers() & Qt::AltModifier) != 0))
        {
            ui->dockWidget->setVisibilityState(CustomDockWidget::FullScreen);
        }
        else if (event->matches(QKeySequence::NextChild))
        {
            nextTab();
        }
        else if (event->matches(QKeySequence::PreviousChild))
        {
            prevTab();
        }
        else
        {
            QMainWindow::keyPressEvent(event);
        }
    }
}

SearchResultForm* MainWindow::searchForm() { return m_searchResultFrom; }

DownloadsForm* MainWindow::downloadsForm() { return m_downloadsForm; }

LibraryForm* MainWindow::libraryForm() { return m_libraryForm; }

const char modelDataFileName[] = "modelState.xml";

const char documentNodeName[] = "document";
const char modelNodeName[] = "model";
const char downloadsNodeName[] = "downloads";
const char libraryNodeName[] = "library";

void MainWindow::loadModelData()
{
    qDebug() << __FUNCTION__;

    QElapsedTimer timer;
    timer.start();

    const QString filePath = utilities::PrepareCacheFolder() + modelDataFileName;
    const QString bakFile = filePath + "-";

    QFile input(bakFile);
    const bool bakFileUsed = input.open(QIODevice::ReadOnly);  // try opening at once
    if (bakFileUsed || (input.setFileName(filePath), input.open(QIODevice::ReadOnly)))
    {
        QXmlStreamReader stream(&input);

        bool foundXmlDocumentBegin = stream.readNextStartElement() && documentNodeName == stream.name();
        Q_UNUSED(foundXmlDocumentBegin)
        Q_ASSERT(foundXmlDocumentBegin);

        utilities::ModelDeserializer serializer(stream);
        serializer.deserialize(&SearchManager::Instance(), modelNodeName);
        serializer.deserialize(m_downloadsForm->model(), downloadsNodeName);
        serializer.deserialize(m_libraryForm->model(), libraryNodeName);

        input.close();
    }

    if (bakFileUsed)
    {
        utilities::DeleteFileWithWaiting(filePath);
        input.rename(filePath);
    }
    else
    {
        utilities::DeleteFileWithWaiting(bakFile);
    }

    qDebug() << __FUNCTION__ << "time spent, milliseconds:" << timer.restart();
}

void MainWindow::askForSavingModel()
{
    if (m_isInitialized)
    {
        m_isModelFileOutdated = true;
        m_askForSavingModelTimer.start();
    }
}

void MainWindow::condsiderSavingModel()
{
    if (m_isModelFileOutdated)
    {
        saveModelData();
    }
}

void MainWindow::saveModelData()
{
    const QString filePath = utilities::PrepareCacheFolder() + modelDataFileName;
    qDebug() << __FUNCTION__ << "Model data save path: " << filePath;

    QElapsedTimer timer;
    timer.start();

    utilities::FileSaveGuard fileSafer(filePath);

    QFile output(filePath);
    if (fileSafer.isTempFileNoError() && output.open(QIODevice::WriteOnly))
    {
        QXmlStreamWriter stream(&output);

        stream.setCodec(utilities::CustomUtf8Codec::Instance());

        stream.setAutoFormatting(true);
        stream.writeStartDocument();

        stream.writeStartElement(documentNodeName);

        utilities::ModelSerializer serializer(stream);
        serializer.serialize(&SearchManager::Instance(), modelNodeName);
        serializer.serialize(m_downloadsForm->model(), downloadsNodeName);
        serializer.serialize(m_libraryForm->model(), libraryNodeName);

        stream.writeEndDocument();
        const bool failed = stream.hasError();
        output.close();
        if (!failed)
        {
            fileSafer.ok();
        }
        m_isModelFileOutdated = false;
    }
    else
    {
        qWarning() << "Could not save model data to the file: " << filePath;
    }

    qDebug() << __FUNCTION__ << "time spent, milliseconds:" << timer.restart();
}

void MainWindow::resetFileSystemWatcher()
{
    delete libraryWatcher;
    libraryWatcher = new QFileSystemWatcher(this);
    if (global_functions::SaveVideoPathHasWildcards())
    {
        QString strSite = QSettings().value(app_settings::Sites, app_settings::Sites_Default).toString();
        QStringList listSites = strSite.split(";", QString::SkipEmptyParts);
        foreach (const QString& str, listSites)
        {
            libraryWatcher->addPath(global_functions::getSaveFolder(str));
        }
    }
    else
    {
        libraryWatcher->addPath(global_functions::GetVideoFolder());
    }
    VERIFY(connect(libraryWatcher, SIGNAL(directoryChanged(const QString&)), m_libraryForm->model(),
                   SLOT(onVideoDirectoryChanged())));
    auto* libraryModel = dynamic_cast<LibraryModel*>(m_libraryForm->model());
    if (libraryModel != nullptr)
    {
        libraryModel->synchronize(true);
    }
}

void MainWindow::nextTab() { m_mainToolbar->switchTab(true); }

void MainWindow::prevTab() { m_mainToolbar->switchTab(false); }

void MainWindow::openDownloadsTab(DownloadEntity* selEntity) { m_mainToolbar->openDownloadsTab(selEntity); }

void MainWindow::openLibraryTab(DownloadEntity* selEntity) { m_mainToolbar->openLibraryTab(selEntity); }

int MainWindow::getCurrTabIndex() { return m_mainToolbar->getCurrTabIndex(); }

void MainWindow::resetDockWidgetPlayer()
{
    VideoPlayerWidgetInstance()->setEntity(nullptr);
    VideoPlayerWidgetInstance()->setDefaultPreviewPicture();
}

void MainWindow::showHideNotify()
{
    if (m_ShowSysTrayNotification)
    {
        m_ShowSysTrayNotification = false;
        showTrayMessage(
            tr("%1 continues running. Click this button to open it").arg(Tr::Tr(PROJECT_FULLNAME_TRANSLATION)));
    }
}

MainWindow* MainWindow::Instance()
{
    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (auto* mainWindow = qobject_cast<MainWindow*>(widget))
        {
            return mainWindow;
        }
    }
    return nullptr;
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat("text/plain") || event->mimeData()->hasFormat("text/uri-list"))
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (auto mimeData = event->mimeData())
    {
        if (SearchManager::Instance().addLinks(*mimeData))
        {
            event->acceptProposedAction();
        }
    }
}

void MainWindow::onShowPlaybutton(bool show)
{
#ifdef Q_OS_WIN
    m_taskBar.updateButton(show ? m_hPlay : m_hPause, show ? tr("Play") : tr("Pause"));
#endif
}
