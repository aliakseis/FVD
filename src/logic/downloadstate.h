#pragma once

enum DownloadState
{
    kQueued,        // initial state, download is in queue to start
    kDownloading,    // download in progress
    kPaused,        // Paused means freeze, and able to be resumed, if supported by server
    kFinished,        // Final state of normal download. File should exist, network reply closed
    kFailed,        // Error happened, see error details. Can be redownloaded by placing in queue.
    kCanceled        // Cancelled by user, file deleted. Can be redownloaded by placing in queue.
};
