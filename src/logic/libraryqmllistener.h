#pragma once

#include <QObject>
#include <QPointer>

class QSortFilterProxyModel;
class LibraryModel;
class DownloadEntity;

class LibraryQmlListener : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY darkThemeChanged)
public:
    LibraryQmlListener(QObject* parent, LibraryModel* model, QSortFilterProxyModel* proxyModel);
    bool darkTheme();

private:
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
    void darkThemeChanged();
};
