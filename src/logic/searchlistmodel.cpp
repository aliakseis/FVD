#include "searchlistmodel.h"
#include "searchmanager.h"
#include "mainwindow.h"
#include "globals.h"

#include <QAction>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QTextDocument>
#include <QMimeData>

SearchListModel::SearchListModel(QObject* parent) :
	QAbstractListModel(parent), m_headerView(nullptr)
{
	VERIFY(connect(&SearchManager::Instance(), SIGNAL(searchResultFound(QList<RemoteVideoEntity*>)), this, SLOT(onAppendEntities(QList<RemoteVideoEntity*>))));
}

SearchListModel::~SearchListModel()
= default;

int SearchListModel::rowCount(const QModelIndex& parent) const
{
	Q_UNUSED(parent);
	return numEntities();
}

int SearchListModel::columnCount(const QModelIndex& parent) const
{
	Q_UNUSED(parent);
	return SR_LastColumn;
}

QVariant SearchListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return {};
	}

	if (role == Qt::DisplayRole)
	{
		if (const RemoteVideoEntity* entity = item(index.row()))
		{
			switch (index.column())
			{
			case SR_Index:
				return {index.row() + 1};
			case SR_Title:
				return QVariant(entity->m_videoInfo.videoTitle);
			case SR_Description:
			{
				QString s = entity->m_videoInfo.description;
				return QVariant(s.remove(QRegExp("<[^>]*>")));
			}
			case SR_Status:
			{
				if (entity->lastError() == Errors::NoError)
				{
					return entity->progress();
				}
				return {entity->lastError()};
			}
			case SR_Length:
				return utilities::secondsToString(entity->m_videoInfo.duration);
            default:
                return {};
            }
		}

        return {};
	}
	if (role == Qt::EditRole)
	{
		if (m_headerView != nullptr)
		{
			m_headerView->setSortIndicatorShown(true);
		}
		if (const RemoteVideoEntity* entity = item(index.row()))
		{
			switch (index.column())
			{
			case SR_Index:
				return {index.row()};
			case SR_Icon:
				return entity->m_videoInfo.strategyName;
			case SR_Title:
				return QVariant(entity->m_videoInfo.videoTitle);
			case SR_Description:
				return entity->m_videoInfo.description;
			case SR_Status:
				return entity->progress();
			case SR_Length:
				return {(int)entity->m_videoInfo.duration};
            default:
                return {};
            }
		}
	}
	else if (role == Qt::ToolTipRole)
	{
		const RemoteVideoEntity* entity = item(index.row());
		if (auto* view = qobject_cast<QAbstractItemView*>(this->QObject::parent()))
		{
			switch (index.column())
			{
			case SR_Index:
			case SR_Icon:
			{
				return entity->strategyName();
			}
			case SR_Title:
			case SR_Description:
			{
				QString text = (index.column() == SR_Title) ? entity->m_videoInfo.videoTitle : entity->m_videoInfo.description;
				QRegExp tag("<[a-zA-Z][a-zA-Z0-9]*\\s*/?>");
				if (tag.indexIn(text) < 0)
				{
					text = text.toHtmlEscaped();
				}
				if (text.length() > 0)
				{
					text.prepend("<html><body>");
					text.append("</body></html>");
				}
				return text;
			}
			case SR_Length:
			{
				return utilities::secondsToString(entity->m_videoInfo.duration);
			}
			case SR_Status:
			{
				if (entity->lastError() != Errors::NoError)
				{
					return Errors::description(entity->lastError());
				}
			}
			default:
                return {};
			}
		}
	}
	else if (role == Qt::UserRole)
	{
		if (index.column()==  SR_Icon)
        {
			return item(index.row())->m_videoInfo.strategyName;
		}
	}
	else if (role == Qt::TextAlignmentRole)
	{
		if (index.column() == SR_Index)
        {
			return int(Qt::AlignRight | Qt::AlignVCenter);
		}
	}

    return {};
}

QVariant SearchListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
	{
		QVariant res;
		switch (section)
		{
		case SR_Index:
			return {"#"};
		case SR_Title:
            return utilities::Tr::Tr(TREEVIEW_TITLE_HEADER);
		case SR_Description:
            return utilities::Tr::Tr(TREEVIEW_DESCRIPTION_HEADER);
		case SR_Status:
            return utilities::Tr::Tr(SEARCH_TREEVIEW_STATUS_HEADER);
		case SR_Length:
            return utilities::Tr::Tr(TREEVIEW_LENGTH_HEADER);
		}
        return {};
	}
	if (role == Qt::DecorationRole && section == SR_Icon)
	{
		return QVariant(QIcon(":/header_arrow_down"));
	}
	if (role == Qt::DisplayRole && orientation == Qt::Vertical)
	{
		return {section + 1};
	}
	return {};
}

Qt::ItemFlags SearchListModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return  Qt::ItemIsDropEnabled;
	}

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QMimeData* SearchListModel::mimeData(const QModelIndexList& indexes) const
{
	QMimeData* mimeData = QAbstractListModel::mimeData(indexes);
	mimeData->setText("test");
	QList<QUrl> urls;
	QString oneUrl;
	QString html;
	Q_FOREACH(const QModelIndex & idx, indexes)
	{
		if (idx.column() == 0)
		{
			const RemoteVideoEntity* entity = item(idx.row());
			urls.append(QUrl(entity->originalUrl()));
			if (!oneUrl.isEmpty())
			{
				oneUrl += "\n";
				html += "<br>";
			}
			oneUrl += entity->originalUrl();
			html += QString("<a href=\"%1\">%2</a>").arg(entity->originalUrl(), entity->videoTitle());
		}
	}
	mimeData->setUrls(urls);
	mimeData->setText(oneUrl);
	mimeData->setHtml(html);

	return  mimeData;
}

void SearchListModel::update()
{
	auto* view = qobject_cast<QTreeView*>(QObject::parent());
	view->selectionModel()->clearSelection();
	beginResetModel();
	endResetModel();
}

void SearchListModel::setHeaderView(QHeaderView* headerView)
{
	m_headerView = headerView;
}

void SearchListModel::onRowUpdated(int row)
{
	if (row >= 0 && row < rowCount())
	{
		QModelIndex ind = index(row, SR_Status);
		emit dataChanged(ind, ind);
	}
}

void SearchListModel::onAppendEntities(const QList<RemoteVideoEntity*>& list)
{
	if (!list.empty())
	{
		beginInsertRows(QModelIndex(), numEntities(), numEntities() + list.size() - 1);
		addEntities_impl(list);
		Q_FOREACH(RemoteVideoEntity * rve, list)
		{
			VERIFY(connect(rve, SIGNAL(signRVEUpdated()), this, SLOT(updateEntityRow())));
			VERIFY(connect(rve, SIGNAL(signRVEProgressUpdated()), this, SLOT(updateProgressEntityRow())));
		}
		endInsertRows();
	}
}

void SearchListModel::clear()
{
	beginResetModel();

	iterateEntities([this](RemoteVideoEntity * const entity) {entity->disconnect(this);});
	clear_impl();

	endResetModel();
}

void SearchListModel::updateEntityRow()
{
	if (auto* entity = qobject_cast<RemoteVideoEntity*>(sender()))
	{
		onRowUpdated(entityRow(entity));
		if (MainWindow::Instance()->getCurrTabIndex() == 0)
		{
			emit updateCurrentRow();
		}
	}
}

void SearchListModel::updateProgressEntityRow()
{
	if (auto* entity = qobject_cast<RemoteVideoEntity*>(sender()))
	{
		onRowUpdated(entityRow(entity));
	}
}
