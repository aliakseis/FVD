#include "customdockwidget.h"

#include <QCloseEvent>
#include <QEvent>
#include <QWidget>

#include "downloadsform.h"
#include "libraryform.h"
#include "mainwindow.h"
#include "playerheader.h"
#include "previewpanelbutton.h"
#include "searchresultform.h"
#include "videowidget.h"

CustomDockWidget::CustomDockWidget(QWidget* widget)
    : QDockWidget(widget), m_state(ShownDocked), m_prevState(ShownDocked)
{
    Q_ASSERT(qobject_cast<MainWindow*>(parent()) != nullptr);
    m_parent = static_cast<MainWindow*>(parent());
}

void CustomDockWidget::setDisplayForFullscreen(VideoDisplay* display)
{
    Q_ASSERT(display != nullptr);
    m_display = static_cast<VideoWidget*>(display);
    VERIFY(connect(this, SIGNAL(topLevelChanged(bool)), SLOT(onTopLevelChanged(bool))));
}

void CustomDockWidget::closeEvent(QCloseEvent* event) { event->ignore(); }

void CustomDockWidget::keyPressEvent(QKeyEvent* event)
{
    // Player fullscreen in
    if (VideoWidget::isFullScreenKeyboardShortcut(event))
    {
        setVisibilityState(FullScreen);
        event->ignore();
    }
    else if (event->modifiers() == Qt::AltModifier && event->nativeVirtualKey() == Qt::Key_X)
    {
        m_parent->closeApp();
        return;
    }
    else if (event->matches(QKeySequence::NextChild))
    {
        m_parent->nextTab();
        return;
    }
    else if (event->matches(QKeySequence::PreviousChild))
    {
        m_parent->prevTab();
        return;
    }
    QDockWidget::keyPressEvent(event);
}

bool CustomDockWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease)
        switch (m_state)
        {
        case ShownDocked:
            setVisibilityState(HiddenDocked);
            break;
        case HiddenDocked:
            setVisibilityState(ShownDocked);
            break;
        case Floating:
            setVisibilityState(FullScreen);
            break;
        }
    return QWidget::eventFilter(obj, event);
}

void CustomDockWidget::setTitleBarWidget(PlayerHeader* header)
{
    manageButton = header->label();
    manageButton->installEventFilter(this);
    QDockWidget::setTitleBarWidget(header);
}

void CustomDockWidget::onTopLevelChanged(bool topLevel)
{
    manageButton->setImages(topLevel ? PreviewPanelButton::Scale : PreviewPanelButton::RightArrow);
    m_prevState = m_state;
    setFocus();
    if (topLevel && isVisible())
    {
        m_state = Floating;
    }
    else
    {
        setVisibilityState(isVisible() ? ShownDocked : HiddenDocked);
    }
}

void CustomDockWidget::setTabsManageWidgetsVisible(bool visible /* = true*/)
{
    m_parent->searchForm()->manageWidget()->setVisible(visible);
    m_parent->libraryForm()->manageWidget()->setVisible(visible);
    m_parent->downloadsForm()->manageWidget()->setVisible(visible);
}

void CustomDockWidget::setVisibilityState(VisibilityState state)
{
    if (state == m_state)
    {
        return;
    }

    switch (state)
    {
    case ShownDocked:
        setVisible(true);
        setTabsManageWidgetsVisible(false);
        break;
    case HiddenDocked:
        setVisible(false);
        setTabsManageWidgetsVisible(true);
        break;
    case Floating:
        setFloating(true);
        setVisible(true);
        setTabsManageWidgetsVisible(false);
        break;
    case FullyHidden:
        setVisible(false);
        setTabsManageWidgetsVisible(false);
        break;
    case FullScreen:
    {
        bool prevIsFullScreen = m_display->isFullScreen();
        m_display->fullScreen(!prevIsFullScreen);
        if (m_display->isFullScreen() == prevIsFullScreen)
        {
            return;
        }
        // emit enterFullscreen(true);
    }
    break;
    }
    m_prevState = m_state;
    m_state = state;
}

void CustomDockWidget::onLeaveFullScreen()
{
    if (m_display->isFullScreen())
    {
        m_display->fullScreen(false);
    }
    setVisibilityState(m_prevState);
}
