#include "videoqualitydialog.h"

#include <QDesktopWidget>
#include <QKeyEvent>
#include <QMovie>

#include "mainwindow.h"
#include "remotevideoentity.h"
#include "searchmanager.h"
#include "ui_videoqualitydialog.h"

VideoQualityDialog::VideoQualityDialog(RemoteVideoEntity* entity, QWidget* parent)
    : QDialog(parent), ui(new Ui::VideoQualityDialog), m_entity(entity)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
    Q_ASSERT(entity);

    if (!entity->m_resolutionLinks.empty())
    {
        onLinksExtracted();
    }
    else
    {
        ui->listWidget->hide();

        ui->listWidget->installEventFilter(this);

        auto* movie = new QMovie(":/images/spinner.gif", QByteArray(), ui->label);
        ui->label->setMovie(movie);
        movie->start();

        entity->extractLinks();
        VERIFY(connect(entity, SIGNAL(linksExtracted()), SLOT(onLinksExtracted())));
    }
    VERIFY(connect(ui->listWidget, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(onItemClicked(QListWidgetItem*))));
}

VideoQualityDialog::~VideoQualityDialog() { delete ui; }

int VideoQualityDialog::exec()
{
    QDialog::show();
    return localEventLoop.exec();
}

void VideoQualityDialog::move(const QPoint& pos)
{
    m_initPos = pos;
    QDialog::move(pos);
}

void VideoQualityDialog::onLinksExtracted()
{
    ui->label->hide();
    ui->listWidget->show();
    ui->listWidget->clear();
    int dialogWidth = 120;
    foreach (LinkInfo link, m_entity->m_resolutionLinks)
    {
        int row = ui->listWidget->count();
        QString displayData = link.resolution + " " + link.extension;
        auto* item = new QListWidgetItem(displayData, ui->listWidget);
        item->setData(Qt::UserRole, link.resolutionId);
        ui->listWidget->insertItem(row, item);
        QFontMetrics fontMetrics = ui->listWidget->fontMetrics();
        if (fontMetrics.width(displayData) + 10 > dialogWidth)
        {
            dialogWidth = fontMetrics.width(displayData) + 10;
        }
    }
    if (!m_entity->m_resolutionLinks.empty())
    {
        int shforRow = ui->listWidget->sizeHintForRow(0);
        int countItem = ui->listWidget->count();
        int heightView = countItem * shforRow;
        resize(dialogWidth, heightView + 2);
    }
    else
    {
        close();
        localEventLoop.quit();
    }
}

void VideoQualityDialog::onItemClicked(QListWidgetItem* item)
{
    Q_ASSERT(item);
    Q_ASSERT(m_entity);
    SearchManager::Instance().download(m_entity, item->data(Qt::UserRole).toInt(), visNorm);
    close();
}

bool VideoQualityDialog::event(QEvent* event)
{
    if ((event->type() == QEvent::WindowDeactivate) || (event->type() == QEvent::HideToParent))
    {
        close();
        localEventLoop.quit();
    }
    else if (event->type() == QEvent::KeyPress)
    {
        auto* keyEvent = (QKeyEvent*)event;
        if (keyEvent->matches(QKeySequence::NextChild))
        {
            close();
            localEventLoop.quit();
            MainWindow::Instance()->nextTab();
            return true;
        }
        if (keyEvent->matches(QKeySequence::PreviousChild))
        {
            close();
            localEventLoop.quit();
            MainWindow::Instance()->prevTab();
            return true;
        }
        if ((keyEvent->nativeVirtualKey() == Qt::Key_X) && (keyEvent->modifiers() == Qt::AltModifier))
        {
            MainWindow::Instance()->closeApp();
            return true;
        }
    }

    return QWidget::event(event);
}

void VideoQualityDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    QPoint pos = m_initPos;
    if (pos.x() + width() > QApplication::desktop()->width())
    {
        pos.setX(pos.x() - width());
    }
    if (pos.y() + height() > QApplication::desktop()->height())
    {
        pos.setY(pos.y() - height());
    }
    QDialog::move(pos);
}
