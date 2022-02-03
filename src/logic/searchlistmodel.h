#ifndef VIDEOSLISTMODEL_H
#define VIDEOSLISTMODEL_H

#include "basefacademodel.h"
#include "remotevideoentity.h"

#include <QAbstractItemModel>

class QHeaderView;

enum SearchResultHeaders
{
	SR_Index,
	SR_Icon,
	SR_Title,
	SR_Description,
	SR_Length,
	SR_Status,
	SR_LastColumn
};

class SearchListModel : public QAbstractListModel, public BaseFacadeModel<RemoteVideoEntity>
{
	Q_OBJECT
	typedef QSharedPointer<RemoteVideoEntity> EntitiesSetItem_t;
public:
	explicit SearchListModel(QObject* parent = 0);
	virtual ~SearchListModel();

	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int columnCount(const QModelIndex& parent) const override;
	virtual QVariant data(const QModelIndex& index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	virtual QMimeData* mimeData(const QModelIndexList& indexes) const override;

	void update();
	void clear();

	void setHeaderView(QHeaderView* headerView);

Q_SIGNALS:
	void updateCurrentRow();

public Q_SLOTS:
	void onRowUpdated(int row);
	void onAppendEntities(const QList<RemoteVideoEntity* >& list);

private Q_SLOTS:
	void updateEntityRow();
	void updateProgressEntityRow();

private:
	QHeaderView* m_headerView;
};

#endif // VIDEOSLISTMODEL_H
