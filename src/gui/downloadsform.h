#pragma once

#include <QFrame>
#include <QAbstractItemModel>
#include "customdockwidget.h"

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

class DownloadsForm : public QFrame, public ManageWidget
{
	Q_OBJECT

public:
	explicit DownloadsForm(VideoPlayerWidget* video_player, QWidget* parent = 0);
	~DownloadsForm();

	QWidget* manageWidget() const;

	QObject* model();
	Q_INVOKABLE void hideFloatingControl();

	bool exists(const DownloadEntity* entity);

signals:
	void entityActivated(const DownloadEntity* entity);

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
	static inline void openFolder(const QString& fileName);

	/// \brief	Deletes listed items.
	/// \param	deleteCompletely	true to delete downloaded files, false leaved files only if they completely downloaded.
	void deleteItems(QModelIndexList const& indexList, bool deleteCompletely);
	void moveUpSelectedItems();
	void moveDownSelectedItems();
	void copySelectionToClipboard();

protected:
	virtual void hideEvent(QHideEvent*) override;
	virtual bool eventFilter(QObject* obj, QEvent* event) override;

private:
	Ui::DownloadsForm* ui;
	DownloadListModel* m_model;
	DownloadSortFilterModel* m_proxyModel;
	VideoPlayerWidget* videoPlayer;
	DownloadsControl* control;
};
