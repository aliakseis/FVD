#pragma once

#include <QAbstractItemModel>
#include <set>

#include "downloadmanager.h"
#include "download/downloader.h"

enum DownloadListHeaders
{
	DL_All = -1,
	DL_Index = 0,
	DL_Icon,
	DL_Title,
	DL_Length,
	DL_Size,
	DL_Progress,
	DL_Speed,
	DL_Status,
	DL_LastColumn
};

class DownloadListModel : public QAbstractListModel, public DownloadManager
{
	Q_OBJECT
public:
	explicit DownloadListModel(QObject* parent = 0);
	virtual ~DownloadListModel();

	Q_PROPERTY(QObjectList entities READ entities WRITE setEntities)

	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int columnCount(const QModelIndex& parent) const override;
	virtual QVariant data(const QModelIndex& index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

	void removeRowsSet(const std::set<int>& rows);

	void clear();

Q_SIGNALS:
	void notifyRemoveItemsFromModel(const QList<DownloadEntity*>& list);
	void notifyFinishedDownloading(const QList<DownloadEntity*>& list);

	void notifyRemoveItemsFromModel(const QList<DownloadEntity*>& list, QObject* obj);

public Q_SLOTS:
	void onAppendEntities(const QList<DownloadEntity*>& list);
	void onLinkExtracted();

private Q_SLOTS:
	void updateEntityRow();
	void entityStateChanged(Downloadable::State newState, Downloadable::State oldState);
	void onAddItemsInModel(const QList<DownloadEntity*>& list, QObject* obj);
	void onRemoveItemsFromModel(const QList<DownloadEntity*>& list, QObject* obj);

private:
	// serialization helpers
	QObjectList entities()const;
	void setEntities(const QObjectList& ents);
	bool isReportSM() {return m_reportSM;}
	void rowUpdated(int row, DownloadListHeaders header);
	void notifyDownloadRestarted(DownloadEntity* de) override;
	void mapSignalToHeader(const char* signal, DownloadListHeaders header);
private:
	bool m_reportSM;
	QMap<int, DownloadListHeaders> m_mapSignalsToHeaders;
};
