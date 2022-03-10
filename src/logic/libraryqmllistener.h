#pragma once

#include <QObject>
#include <QPointer>

class QSortFilterProxyModel;
class LibraryModel;
class DownloadEntity;

class LibraryQmlListener : public QObject
{
	Q_OBJECT
public:
	explicit LibraryQmlListener(QObject* parent = 0);

	LibraryModel* m_model;
	QSortFilterProxyModel* m_proxyModel;

public slots:
	static void onImageClicked(int index);
	void onDeleteClicked(int index);
	void onPlayClicked(int index) const;
	void onPlayInternal(int index) const;

private slots:
	static void onHandleDeleteAsynchronously(const QPointer<DownloadEntity>& entity);

signals:
	void handleDeleteAsynchronously(const QPointer<DownloadEntity>& entity);
};
