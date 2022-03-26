#include "libraryqmllistener.h"

#include <QDebug>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <utility>

#include "branding.hxx"
#include "customdockwidget.h"
#include "gui/videoplayerwidget.h"
#include "librarymodel.h"
#include "logic/searchmanager.h"
#include "mainwindow.h"
#include "settings_declaration.h"
#include "utilities/filesystem_utils.h"
#include "utilities/notify_helper.h"

using namespace utilities;

class FileReleasedDeleteCallback : public NotifyHelper
{
public:
    FileReleasedDeleteCallback(QPointer<DownloadEntity> entity) : m_entity(std::move(entity)) {}

    void slotNoParams() override
    {
        if (!m_entity.isNull())
        {
            SearchManager::Instance().onItemsDeletedNotify(QList<DownloadEntity*>() << m_entity.data());
        }

        deleteLater();
    }

private:
    QPointer<DownloadEntity> m_entity;
};

LibraryQmlListener::LibraryQmlListener(QObject* parent) : QObject(parent), m_model(nullptr)
{
    qRegisterMetaType<QPointer<DownloadEntity> >("QPointer<DownloadEntity>");
    VERIFY(connect(this, SIGNAL(handleDeleteAsynchronously(const QPointer<DownloadEntity>&)),
                   SLOT(onHandleDeleteAsynchronously(const QPointer<DownloadEntity>&)), Qt::QueuedConnection));
}

void LibraryQmlListener::onImageClicked(int index) { qDebug() << __FUNCTION__ << " index = " << index; }

void LibraryQmlListener::onDeleteClicked(int index)
{
    qDebug() << __FUNCTION__ << " index = " << index;
    if (index >= 0)
    {
        int sourceRow = m_proxyModel->mapToSource(m_proxyModel->index(index, 0)).row();
        QPointer<DownloadEntity> entity =
            qvariant_cast<DownloadEntity*>(m_model->index(sourceRow).data(LibraryModel::RoleEntity));

        // We need to invoke delete functionality asynchronously because QMessageBox::question() runs message loop
        emit handleDeleteAsynchronously(entity);
    }
}

void LibraryQmlListener::onPlayClicked(int index) const
{
    qDebug() << __FUNCTION__ << " index = " << index;
    if (index >= 0)
    {
        int sourceRow = m_proxyModel->mapToSource(m_proxyModel->index(index, 0)).row();
        auto* entity = qvariant_cast<DownloadEntity*>(m_model->index(sourceRow).data(LibraryModel::RoleEntity));
        Q_ASSERT(entity->getParent() != nullptr);
        const QFileInfo fileInfo(entity->filename());
        const auto filename = fileInfo.canonicalFilePath();
        if (QSettings().value(app_settings::OpenInFolder, app_settings::OpenInFolder_Default).toBool())
        {
            auto downloadDirectory = fileInfo.dir().canonicalPath();
            utilities::SelectFile(filename, downloadDirectory);
        }
        else
        {
            const bool isOk = QDesktopServices::openUrl(QUrl::fromUserInput(filename));
            if (!isOk)
            {
                qDebug() << "Failed to play '" << QFileInfo(entity->filename()).canonicalFilePath()
                         << "' with default player";

                if (QFile::exists(entity->filename()))
                {
                    if (QMessageBox::Yes ==
                        QMessageBox::question(nullptr, Tr::Tr(PROJECT_FULLNAME_TRANSLATION),
                                              tr("Video file cannot be played with default video player.\nDo you want "
                                                 "to watch it with internal player?"),
                                              QMessageBox::Yes, QMessageBox::No))
                    {
                        VideoPlayerWidgetInstance()->playDownloadEntity(entity);
                        MainWindow::Instance()->dockWidget()->setVisibilityState(CustomDockWidget::ShownDocked);
                    }
                }
                else
                {
                    QMessageBox::critical(nullptr, Tr::Tr(PROJECT_FULLNAME_TRANSLATION), tr("File missing"));
                }
            }
        }
    }
}

void LibraryQmlListener::onPlayInternal(int index) const
{
    if (index >= 0)
    {
        int sourceRow = m_proxyModel->mapToSource(m_proxyModel->index(index, 0)).row();
        auto* entity = qvariant_cast<DownloadEntity*>(m_model->index(sourceRow).data(LibraryModel::RoleEntity));
        Q_ASSERT(entity->getParent() != nullptr);
        VideoPlayerWidgetInstance()->playDownloadEntity(entity);
        MainWindow::Instance()->dockWidget()->setVisibilityState(CustomDockWidget::ShownDocked);
    }
}

void LibraryQmlListener::onHandleDeleteAsynchronously(const QPointer<DownloadEntity>& entity)
{
    if (!entity.isNull())
    {
        QString title = entity.data()->videoTitle();
        int res = QMessageBox::question(utilities::getMainWindow(), Tr::Tr(PROJECT_FULLNAME_TRANSLATION),
                                        tr("Are you sure you want to delete '%1'?").arg(title), QMessageBox::Yes,
                                        QMessageBox::No);

        if (res == QMessageBox::Yes && !entity.isNull())
        {
            if (fileIsInUse(VideoPlayerWidgetInstance(), entity.data()))
            {
                auto* fileReleasedCallback = new FileReleasedDeleteCallback(entity);
                VERIFY(connect(VideoPlayerWidgetInstance(), SIGNAL(fileReleased()), fileReleasedCallback,
                               SLOT(slotNoParams())));
                VideoPlayerWidgetInstance()->stopVideo(true);
            }
            else
            {
                SearchManager::Instance().onItemsDeletedNotify(QList<DownloadEntity*>() << entity.data());
            }
        }
    }
}
