#pragma once

#include <QAbstractListModel>
#include <QFileSystemModel>
#include <QPointer>
#include <QStringList>

#include "basefacademodel.h"
#include "downloadentity.h"
#include "entityfilenames.h"

class SearchList;

class LibraryModel : public QAbstractListModel, public BaseFacadeModel<DownloadEntity>
{
    Q_OBJECT
public:
    explicit LibraryModel(QObject* parent = 0);

    enum ElementRoles
    {
        RoleThumbnail = Qt::UserRole + 1,
        RoleTitle,
        RoleDate,
        RoleSize,
        RoleEntity,
        RoleTimeDownload,
        RoleFileName,
    };

    void update();
    void clear();

    void synchronize(bool mandatory);

    Q_PROPERTY(QObjectList entities READ entities WRITE setEntities)

Q_SIGNALS:
    void notifyRemoveItemsFromModel(const QList<DownloadEntity*>& list);

public Q_SLOTS:
    void onAppendEntities(const QList<DownloadEntity*>& list);

private Q_SLOTS:
    void onAddItemsInModel(const QList<DownloadEntity*>& list, QObject* obj);
    void onRemoveItemsFromModel(const QList<DownloadEntity*>&, QObject*);

    void onLibraryUpdateTimer();
    void onVideoDirectoryChanged();
    void onLibraryAddWorkerDestroyed();
    void onLibraryRemoveWorkerDestroyed();
    void onLibraryFileAdded(const QString& path);
    void onLibraryFileDeleted(const QPointer<DownloadEntity>& entity);
    void onLibraryFileMissing(const QString& filePath);

    void entityStateChanged(Downloadable::State newState, Downloadable::State oldState);

    void onIdle();

private:
    // serialization helpers
    QObjectList entities() const;
    void setEntities(const QObjectList& ents);

    bool isReportSM() { return m_reportSM; }
    void removeRowDontReportSM(DownloadEntity* ent);

    void scheduleOnIdle();
    bool hasWorkToDoOnIdle();

public:
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

    QHash<int, QByteArray> roleNames() const override;

private:
    bool m_reportSM;

    bool m_isLibraryAddWorkerRunning;
    bool m_isLibraryRemoveWorkerRunning;
    bool m_videoDirectoryChanged;

    EntityFileNames m_allEntitiesFiles;
    QList<QPointer<DownloadEntity> > m_missingFiles;
};
