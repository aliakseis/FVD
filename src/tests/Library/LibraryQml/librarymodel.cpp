#include "librarymodel.h"
#include <QFileInfo>
#include <QDebug>
#ifndef DEVELOPER_DISABLE_SETTINGS
#include "settings_declaration.h"
#else
#include <QDesktopServices>
#endif
#include <QDateTime>
int LibraryModel::counter = 0;

LibraryModel::LibraryModel(QObject* parent) :
	QAbstractListModel(parent),
	m_fakeRows(0)
{
	m_urls.append("http://tuxpaint.org/stamps/stamps/animals/birds/cartoon/tux.png");
	m_urls.append("http://upload.wikimedia.org/wikipedia/commons/4/47/Moon.png");
	m_urls.append("http://2lx.ru/uploads/2012/05/qtcreator.png");
	m_urls.append("http://www.wpclipart.com/space/moon/far_side_of_the_Moon.png");
	m_urls.append("http://www.google.by/images/srpr/logo3w.png");

	update();
}

void LibraryModel::update()
{
	qDebug() << "Update library model";
	m_items.clear();
	m_itemsName.clear();
	QStringList videoFilesMask;
	videoFilesMask << "*.mp4" << "*.flv";

	beginResetModel();
#ifndef DEVELOPER_DISABLE_SETTINGS
	m_dir = GET_SETTING(saveVideoPath);
#else

	QStringList paths = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation);
	m_dir = !paths.empty() ? paths.at(0) : QString();

#endif
	m_filesList = m_dir.entryInfoList(videoFilesMask);
	foreach(QFileInfo fi, m_filesList)
	{
		m_items.append(++counter);
		m_itemsName.append(fi.fileName());
	}

	endResetModel();
}

void LibraryModel::addFakeRow()
{
	qDebug() << "addFakeRow";
	//beginInsertRows(QModelIndex(), m_items.count(), m_items.count());
	beginInsertRows(QModelIndex(), 0, 0);
	m_items.prepend(++counter);
	m_itemsName.append(QString("Name %1").arg(counter));
	endInsertRows();
}

int LibraryModel::rowCount(const QModelIndex& /*parent*/) const
{
	return m_items.count();
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	QVariant result;
	switch (role)
	{
	case Qt::DisplayRole:
	{
		if (index.row() < m_items.count())
		{
			result = m_itemsName.at(index.row());
		}
		else
		{
			result = "random name";
		}
	}
	break;
	case RoleThumbnail:
	{

		if (index.row()  < m_urls.size())
		{
			result = m_urls.at(index.row());
		}
		else
		{
			result = "http://tcuttrissweb.files.wordpress.com/2012/02/youtube_logo.png";
		}
	}
	break;
	case RoleTitle:
	{
		if (index.row() < m_items.count())
		{
			result = m_itemsName.at(index.row());
		}
		else
		{
			result = "random text";
		}
	}
	break;
	case RoleDate:
	{
		result = QDateTime::currentDateTime().toString(Qt::SystemLocaleShortDate).left(10);
	}
	break;
	case RoleSize:
	{
		result = m_items.at(index.row());
	}
	break;
	}
	return result;
}

bool LibraryModel::removeRows(int row, int count, const QModelIndex& parent)
{
	qDebug() << "remove Rows LibraryModel " << row;
	if (row < 0)
	{
		return false;
	}

	int rowCount = this->rowCount(parent);
	if (row < rowCount)
	{
		int lastRow = row + count - 1;

		beginRemoveRows(parent, row, lastRow);

		for (int i = lastRow; i >= row; i--)
		{
			m_items.removeAt(i);
			m_itemsName.removeAt(i);
		}

		endRemoveRows();

		return true;
	}
	return false;
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[RoleThumbnail] = "thumb";
	roles[RoleTitle] = "title";
	roles[RoleDate] = "fileCreated";
	roles[RoleSize] = "fileSize";
	return roles;
}
