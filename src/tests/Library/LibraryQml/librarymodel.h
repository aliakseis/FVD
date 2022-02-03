#ifndef LIBRARYMODEL_H
#define LIBRARYMODEL_H

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

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
	virtual QHash<int, QByteArray> roleNames() const override;
#endif

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

#endif // LIBRARYMODEL_H
