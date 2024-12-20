#pragma once

#include <QAbstractItemModel>
#include <QFrame>

namespace Ui
{
class DownloadsForm;
}

class DownloadListModel;
class QModelIndex;
class QItemSelection;
class DownloadEntity;
class DownloadsControl;
class VideoPlayerWidget;
class DownloadSortFilterModel;

class DownloadsForm : public QFrame
{
    Q_OBJECT

public:
    explicit DownloadsForm(VideoPlayerWidget* video_widget, QWidget* parent = 0);
    ~DownloadsForm();

    QWidget* manageWidget() const;

    QObject* model();
    Q_INVOKABLE void hideFloatingControl();

    bool exists(const DownloadEntity* entity);

signals:
    void entityActivated(const DownloadEntity* entity, int rowNumber);

public slots:
    void onActivated(const DownloadEntity* selEntity);
    void onPreferencesChanged();
    void clearDownloadsList(bool silent = false);

private slots:
    void onDownloadsContextMenu(const QPoint&);
    void onDownloadsDoubleClick(const QModelIndex& index);
    void onSelectionChanged(const QItemSelection& selected, const QItemSelection&);

    void onReloadItem();
    void onRemoveItem();
    void onPauseItem();
    void onStartItem();
    void onStopItem();
    void onItemUp();
    void onItemDown();
    void onSetSpeedLimit(int);

    void onSearch();

    void removeDownload();
    void restartDownload();

    void onVerticalScrollChanged(int);

private:
    /// \brief	Deletes listed items.
    /// \param	deleteCompletely	true to delete downloaded files, false leaved files only if they completely
    /// downloaded.
    void deleteItems(QModelIndexList const& indexList, bool deleteCompletely);
    void moveUpSelectedItems();
    void moveDownSelectedItems();
    void copySelectionToClipboard();

protected:
    void hideEvent(QHideEvent*) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    Ui::DownloadsForm* ui;
    DownloadListModel* m_model;
    DownloadSortFilterModel* m_proxyModel;
    VideoPlayerWidget* videoPlayer;
    DownloadsControl* control;
};
