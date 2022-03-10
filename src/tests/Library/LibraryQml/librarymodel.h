#pragma once

#include <QAbstractListModel>
#include <QFileSystemModel>
#include <QStringList>

class SearchList;

class LibraryModel : public QAbstractListModel
{
	Q_OBJECT
public:
	explicit LibraryModel(QObject* parent = 0);

	enum ElementRoles
	{
		RoleThumbnail = Qt::UserRole + 1,
		RoleTitle,
		RoleDate,
		RoleSize
	};

	void update();
	void addFakeRow();

public:
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
	virtual bool removeRows(int row, int count, const QModelIndex&  parent = QModelIndex());

	virtual QHash<int, QByteArray> roleNames() const override;

	QStringList m_urls;
	const SearchList* m_searches;
	QFileSystemModel m_fileModel;
	QDir m_dir;
	QFileInfoList m_filesList;
	QList<int> m_items;
	QList<QString> m_itemsName;
	static int counter;
	int m_fakeRows;
};
