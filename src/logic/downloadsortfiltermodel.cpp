#include "downloadsortfiltermodel.h"

#include <QAbstractItemView>
#include <QDataStream>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMimeData>

#include "downloadentity.h"
#include "downloadlistmodel.h"

DownloadSortFilterModel::DownloadSortFilterModel(QObject* parent)
    : QSortFilterProxyModel(parent), m_isSorted(true), m_headerView(nullptr)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void DownloadSortFilterModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    base_class::setSourceModel(sourceModel);
}

bool DownloadSortFilterModel::filterAcceptsRow(int source_row, const QModelIndex& /*source_parent*/) const
{
    QRegExp regExp = filterRegExp();
    auto* model = qobject_cast<DownloadListModel*>(sourceModel());
    DownloadEntity* downloadEntity =
        model->item(source_row);  // SearchManager::Instance().getViewFacade()->getDownloadsItem(source_row);

    return regExp.isValid() && (regExp.indexIn(downloadEntity->videoTitle()) >=
                                0 /*|| regExp.indexIn(downloadEntity->description()) >= 0*/);
}

void DownloadSortFilterModel::sort(int column, Qt::SortOrder order)
{
    for (int i = 0; i < rowCount(); ++i)
    {
        m_sortedIndexes[i] = i;
    }
    m_isSorted = true;
    toggleSortIndicator();
    base_class::sort(column, order);
}

void DownloadSortFilterModel::moveItemUp(int row)
{
    std::swap(m_sortedIndexes[row], m_sortedIndexes[row - 1]);
    m_isSorted = false;
    toggleSortIndicator();

    auto* model = qobject_cast<QAbstractItemView*>(QObject::parent());
    Q_ASSERT(model);
    QItemSelection selection;
    selection << QItemSelectionRange(index(row - 1, 0), index(row - 1, DL_LastColumn - 1));
    model->selectionModel()->select(selection, QItemSelectionModel::ToggleCurrent);

    emit dataChanged(index(row, 0), index(row - 1, DL_LastColumn - 1));
}

void DownloadSortFilterModel::moveItemDown(int row)
{
    std::swap(m_sortedIndexes[row], m_sortedIndexes[row + 1]);
    m_isSorted = false;
    toggleSortIndicator();

    auto* model = qobject_cast<QAbstractItemView*>(QObject::parent());
    Q_ASSERT(model);
    QItemSelection selection;
    selection << QItemSelectionRange(index(row + 1, 0), index(row + 1, DL_LastColumn - 1));
    model->selectionModel()->select(selection, QItemSelectionModel::ToggleCurrent);

    emit dataChanged(index(row, 0), index(row + 1, DL_LastColumn - 1));
}

void DownloadSortFilterModel::moveItemsUp(QItemSelectionModel* selectionModel)
{
    QModelIndexList indexList = selectionModel->selectedRows();
    if (!indexList.empty())
    {
        m_isSorted = false;
        toggleSortIndicator();
        std::set<int> affectedRows;
        std::set<int> newSelectionRows;
        std::set<int> prevSelectedRows;
        Q_FOREACH (const QModelIndex& idx, indexList)
        {
            prevSelectedRows.insert(idx.row());
        }

        for (int row : prevSelectedRows)
        {
            if (row > 0 && newSelectionRows.find(row - 1) == newSelectionRows.end())
            {
                affectedRows.insert(row);
                newSelectionRows.insert(row - 1);

                std::swap(m_sortedIndexes[row], m_sortedIndexes[row - 1]);
            }
            else
            {
                newSelectionRows.insert(row);
            }
        }

        if (!newSelectionRows.empty() && !affectedRows.empty())
        {
            QItemSelection selection;
            for (int row : newSelectionRows)
            {
                selection << QItemSelectionRange(index(row, 0), index(row, DL_LastColumn - 1));
            }
            selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
            emit dataChanged(index(*newSelectionRows.cbegin(), 0), index(*affectedRows.rbegin(), DL_LastColumn - 1));
        }
    }
}

void DownloadSortFilterModel::moveItemsDown(QItemSelectionModel* selectionModel)
{
    QModelIndexList indexList = selectionModel->selectedRows();
    if (!indexList.empty())
    {
        QItemSelection slection = selectionModel->selection();
        m_isSorted = false;
        toggleSortIndicator();
        std::set<int> affectedRows;
        std::set<int> newSelectionRows;
        std::set<int> prevSelectedRows;
        Q_FOREACH (const QModelIndex& idx, indexList)
            prevSelectedRows.insert(idx.row());

        for (auto it = prevSelectedRows.rbegin(); it != prevSelectedRows.rend(); ++it)
        {
            unsigned int row = *it;
            if (row < (m_sortedIndexes.size() - 1) && newSelectionRows.find(row + 1) == newSelectionRows.end())
            {
                affectedRows.insert(row);
                newSelectionRows.insert(row + 1);
                std::swap(m_sortedIndexes[row], m_sortedIndexes[row + 1]);
            }
            else
            {
                newSelectionRows.insert(row);
            }
        }

        if (!newSelectionRows.empty() && !affectedRows.empty())
        {
            QItemSelection selection;
            for (int row : newSelectionRows)
            {
                selection << QItemSelectionRange(index(row, 0), index(row, DL_LastColumn - 1));
            }
            selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
            emit dataChanged(index(*newSelectionRows.cbegin(), 0), index(*affectedRows.rbegin(), DL_LastColumn - 1));
        }
    }
}

QModelIndex DownloadSortFilterModel::index(int row, int col, const QModelIndex& index) const
{
    return base_class::index(row, col, index);
}

int DownloadSortFilterModel::mappedRow(int row) const
{
    QModelIndex ind;
    if (!m_isSorted)
    {
        ind = index(m_sortedIndexes[row], 0);
    }
    else
    {
        ind = index(row, 0);
    }
    return base_class::mapToSource(ind).row();
}

int DownloadSortFilterModel::mappedRow(const QModelIndex& proxyIndex) const
{
    int start = proxyIndex.row();
    return mappedRow(start);
}

QModelIndex DownloadSortFilterModel::mapToSource(const QModelIndex& proxyIndex) const
{
    return base_class::mapToSource(proxyIndex);
}

QModelIndex DownloadSortFilterModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    return base_class::mapFromSource(sourceIndex);
}

QVariant DownloadSortFilterModel::data(const QModelIndex& index, int role) const
{
    QModelIndex idx = index;
    if (!m_isSorted)
    {
        idx = createIndex(m_sortedIndexes[index.row()], index.column(), index.internalPointer());
    }
    return base_class::data(idx, role);
}

int DownloadSortFilterModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    uint rows = base_class::rowCount();
    if (m_sortedIndexes.size() != rows)
    {
        m_sortedIndexes.resize(rows);
        for (uint i = 0; i < rows; ++i)
        {
            m_sortedIndexes[i] = i;
        }
    }
    return rows;
}

Qt::ItemFlags DownloadSortFilterModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return Qt::ItemIsDropEnabled;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QMimeData* DownloadSortFilterModel::mimeData(const QModelIndexList& indexes) const
{
    m_isSorted = false;
    QMimeData* datat = base_class::mimeData(indexes);
    QSet<int> rows;
    QList<QUrl> urls;
    QString oneUrl;
    QString html;
    Q_FOREACH (const QModelIndex& idx, indexes)
    {
        rows.insert(m_sortedIndexes[idx.row()]);
        if (idx.column() == 0)  // Column 0 contains data for Qt::UserRole - URL
        {
            urls.append(QUrl(data(idx, Qt::UserRole).toUrl()));
            if (!oneUrl.isEmpty())
            {
                oneUrl += "\n";
                html += "<br>";
            }
            oneUrl += data(idx, Qt::UserRole).toString();
            html += QString("<a href=\"%1\">%2</a>")
                        .arg(data(idx, Qt::UserRole).toString(),
                             data(index(idx.row(), DL_Title), Qt::DisplayRole).toString());
        }
    }

    QByteArray ba;
    {
        QDataStream ds(&ba, QIODevice::WriteOnly);
        ds << rows;
    }
    datat->setData("application/x-rows", ba);
    datat->setUrls(urls);
    datat->setText(oneUrl);
    datat->setHtml(html);

    return datat;
}

bool DownloadSortFilterModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                           const QModelIndex& parent)
{
    Q_UNUSED(parent)
    Q_UNUSED(action)
    Q_UNUSED(column)
    const int originalSize = (int)m_sortedIndexes.size();
    if (-1 == row)
    {
        row = originalSize;
    }

    QByteArray ba = data->data("application/x-rows");

    QSet<int> rowsSet;
    QDataStream ds(ba);
    ds >> rowsSet;

    std::vector<int> rows{rowsSet.begin(), rowsSet.end()};

    int decreaseRowCounter = 0;
    for (int r : rows)
    {
        auto it = std::find(m_sortedIndexes.begin(), m_sortedIndexes.end(), r);
        int rr = it - m_sortedIndexes.begin();
        m_sortedIndexes.erase(std::remove(m_sortedIndexes.begin(), m_sortedIndexes.end(), r), m_sortedIndexes.end());
        if (rr < row)
        {
            ++decreaseRowCounter;
        }
    }

    row -= decreaseRowCounter;

    for (int r : rows)
    {
        auto insertPosition = m_sortedIndexes.begin() + row;
        m_sortedIndexes.insert(insertPosition, r);
    }

    m_sortedIndexes.resize(originalSize);

    m_isSorted = false;
    toggleSortIndicator();

    auto* model = qobject_cast<QAbstractItemView*>(QObject::parent());
    Q_ASSERT(model);
    QItemSelectionModel* selectionModel = model->selectionModel();
    QItemSelection selection;
    selection << QItemSelectionRange(index(row, 0), index(row + (int)rows.size() - 1, DL_LastColumn - 1));
    selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);

    emit dataChanged(index(0, 0), index((int)m_sortedIndexes.size(), DL_LastColumn - 1));

    return true;
}

Qt::DropActions DownloadSortFilterModel::supportedDropActions() const { return Qt::MoveAction; }

void DownloadSortFilterModel::setHeader(QHeaderView* headerView) { m_headerView = headerView; }

void DownloadSortFilterModel::toggleSortIndicator()
{
    if (m_headerView != nullptr)
    {
        m_headerView->setSortIndicatorShown(m_isSorted);
    }
}

bool DownloadSortFilterModel::removeRows(int row, int count, const QModelIndex& parent)
{
    Q_UNUSED(row)
    Q_UNUSED(count)
    Q_UNUSED(parent)
    return true;  // we prevent drag'n'drop operation from deletion our DownloadEntities
}
