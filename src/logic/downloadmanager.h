#pragma once

#include "basefacademodel.h"
#include "download/downloader.h"

#include <QObject>

class DownloadEntity;

class DownloadManager : public BaseFacadeModel<DownloadEntity>
{
public:
	DownloadManager();
	void entityStateChanged(DownloadEntity* dle, Downloadable::State newState, Downloadable::State oldState);
	void considerStartNextDownload();

	static void stopDownload(DownloadEntity* de);
	static bool canStopDownload(DownloadEntity* de);
	static void pauseDownload(DownloadEntity* de);
	void resumeDownload(DownloadEntity* de);
	void restartDownload(DownloadEntity* de);

	static void setSpeedLimit(DownloadEntity* de, int speedLimit);

	void clear_impl();
private:
	int activeDownloadsCount() const;

protected:
	virtual void notifyDownloadRestarted(DownloadEntity*) {}
};
