#pragma once

#include <QWidget>

#include "downloadentity.h"

namespace Ui
{
class LibraryForm;
}

class QSortFilterProxyModel;
class LibraryModel;
class QDeclarativeView;
class QQuickView;

class LibraryForm : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryForm(QWidget* parent = 0);
    ~LibraryForm();

    void sortModel(Qt::SortOrder order = Qt::DescendingOrder);
    QWidget* manageWidget() const;

    QObject* model();

    bool exists(const DownloadEntity* entity);

public slots:
    void onActivated(const DownloadEntity* selEntity);

private slots:
    void onSearch();
    void onShowModeChanged(int index);

private:
    Ui::LibraryForm* ui;
    LibraryModel* m_model;
    QSortFilterProxyModel* m_proxyModel;
    QQuickView* m_view;
};
