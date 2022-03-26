#pragma once

#include <QSortFilterProxyModel>
#include <vector>

class QItemSelectionModel;
class QHeaderView;

class DownloadSortFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    typedef QSortFilterProxyModel base_class;
    explicit DownloadSortFilterModel(QObject* parent = 0);
    QVariant data(const QModelIndex& index, int role) const override;
    QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;
    Qt::DropActions supportedDropActions() const override;
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

    void moveItemUp(int row);
    void moveItemDown(int row);
    void moveItemsUp(QItemSelectionModel* selectionModel);
    void moveItemsDown(QItemSelectionModel* selectionModel);

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
    QModelIndex index(int row, int col, const QModelIndex& index = QModelIndex()) const;
    void setSourceModel(QAbstractItemModel* sourceModel);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int mappedRow(const QModelIndex& proxyIndex) const;
    int mappedRow(int row) const;
    void setHeader(QHeaderView* headerView);

private:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
    // show or hide sort indicator depends on m_isSorted
    void toggleSortIndicator();
    // vector of sorted indexes. The indexes can be not truly sorted if user moved items
    mutable std::vector<int> m_sortedIndexes;
    // true if we have sorted items, false otherwise (some items have been moved so sort order is not valid)
    mutable bool m_isSorted;
    QHeaderView* m_headerView;
};
