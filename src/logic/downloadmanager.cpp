#include "downloadmanager.h"

#include "downloadentity.h"
#include "settings_declaration.h"

DownloadManager::DownloadManager() = default;

void DownloadManager::entityStateChanged(DownloadEntity* dle, DownloadState state, DownloadState oldState)
{
    Q_UNUSED(dle)
    if ((oldState == kDownloading && state != kDownloading) ||
        (kQueued == state))
    {
        considerStartNextDownload();
    }
}

void DownloadManager::considerStartNextDownload()
{
    int numActive = activeDownloadsCount();

    const int maxDownloads =
        QSettings().value(app_settings::maximumNumberLoads, app_settings::maximumNumberLoads_Default).toInt();

    for (int i = 0; numActive < maxDownloads && i < numEntities(); ++i)
    {
        DownloadEntity* nextDl = item(i);
        if (nextDl->state() == kQueued && !nextDl->directUrl().isEmpty() && nextDl->download())
        {
            ++numActive;
        }
    }
}

void DownloadManager::clear_impl() { BaseFacadeModel<DownloadEntity>::clear_impl(); }

void DownloadManager::stopDownload(DownloadEntity* de)
{
    Q_ASSERT(de);
    if (de->state() == kDownloading || de->state() == kPaused)
    {
        de->stop();
    }
    else
    {
        qDebug() << "Trying to stop task from state " << (int)de->state();
    }
}

bool DownloadManager::canStopDownload(DownloadEntity* de)
{
    Q_ASSERT(de);
    return (de->state() == kDownloading || de->state() == kPaused);
}

void DownloadManager::pauseDownload(DownloadEntity* de)
{
    Q_ASSERT(de);
    if (de->state() == kDownloading || de->state() == kQueued)
    {
        de->pause();
    }
    else
    {
        qDebug() << "Trying to stop task from state " << (int)de->state();
    }
}

void DownloadManager::resumeDownload(DownloadEntity* de)
{
    Q_ASSERT(de);
    de->setState(kQueued);
    int numActiveDownloads = activeDownloadsCount();
    if (numActiveDownloads <
        QSettings().value(app_settings::maximumNumberLoads, app_settings::maximumNumberLoads_Default).toInt())
    {
        de->resume();
    }
}

void DownloadManager::restartDownload(DownloadEntity* de)
{
    Q_ASSERT(de);

    if (de->restart())
    {
        notifyDownloadRestarted(de);
    }
}

void DownloadManager::setSpeedLimit(DownloadEntity* de, int speedLimit) { de->setSpeedLimit(speedLimit); }

int DownloadManager::activeDownloadsCount() const
{
    int numActive = 0;
    iterateEntities(
        [&numActive](const DownloadEntity* e)
        {
            if (kDownloading == e->state())
            {
                ++numActive;
            }
        });
    return numActive;
}
