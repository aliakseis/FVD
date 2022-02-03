#ifndef DOWNLOADSORTFILTERMODEL_H
#define DOWNLOADSORTFILTERMODEL_H
#include <vector>
#include <QSortFilterProxyModel>

class QItemSelectionModel;
class QHeaderView;

class DownloadSortFilterModel : public QSortFilterProxyModel
{
	Q_OBJECT
public:
	typedef QSortFilterProxyModel base_class;
	explicit DownloadSortFilterModel(QObject* parent = 0);
	virtual QVariant data(const QModelIndex& index, int role) const override;
	virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
	virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const  override;
	virtual QMimeData* mimeData(const QModelIndexList& indexes) const override;
	virtual bool dropMimeData(const QMimeData* data, Qt::DropAction action,	int row, int column, const QModelIndex& parent) override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

	void moveItemUp(int row);
	void moveItemDown(int row);
	void moveItemsUp(QItemSelectionModel* selectionModel);
	void moveItemsDown(QItemSelectionModel* selectionModel);

	void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
	QModelIndex index(int row, int col, const QModelIndex& index = QModelIndex()) const;
	void setSourceModel(QAbstractItemModel* sourceModel);
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int mappedRow(const QModelIndex& proxyIndex)const;
	int mappedRow(int row)const;
	void setHeader(QHeaderView* headerView);

private:
	virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
	// show or hide sort indicator depends on m_isSorted
	void toggleSortIndicator();
	// vector of sorted indexes. The indexes can be not truly sorted if user moved items
	mutable std::vector<int> m_sortedIndexes;
	// true if we have sorted items, false otherwise (some items have been moved so sort order is not valid)
	mutable bool m_isSorted;
	QHeaderView* m_headerView;
};

#endif // DOWNLOADSORTFILTERMODEL_H
