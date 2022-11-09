#pragma once

#include <QFrame>
#include <QModelIndex>

#include "customdockwidget.h"

namespace Ui
{
class SearchResultForm;
}
class SearchListModel;
class QAction;
class QItemSelection;
class RemoteVideoEntity;
class QSortFilterProxyModel;

class SearchResultForm : public QFrame, public ManageWidget
{
    Q_OBJECT

public:
    explicit SearchResultForm(QWidget* parent = 0);
    ~SearchResultForm();

    QWidget* manageWidget() const;
    void saveSitesList();

private:
    // if autoCheckEmpty is true and the combo box doesn't have checked sites, 'All Video Sites' group will be checked
    void populateSitesCombo(bool autoCheckEmpty);

    bool populateSitesComboHelper(const QStringList& strategiesNames, const QIcon& sectionIcon,
                                  const QString& sectionName);

    void createActions();
    void showSearchButton();
    void showStopButton();
    void updatePrevNextState();
    void doSearch(QString query, int page = 1);
    void copySelectionToClipboard();
    void requestStartDownload(int row);

public slots:
    void onActivated();
    void onPreferencesChanged();

private slots:
    void doSearch();
    void stopSearch();
    void onSearchFinished();
    static void onSearchFailed(const QString& error);
    void onDownload(int row);
    void onDownloadContextMenu(int row, QPoint pos);
    void onSearchContextMenu(const QPoint& point);
    void onSelectionChanged(const QItemSelection& selected, const QItemSelection&);
    void onViewDoubleClick(const QModelIndex& index);
    void on_btnPrevious_clicked();
    void on_btnNext_clicked();
    void onVerticalScrollChanged(int);

signals:
    void entityActivated(RemoteVideoEntity*);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    Ui::SearchResultForm* ui;
    SearchListModel* m_model;
    QSortFilterProxyModel* m_proxyModel;

    QAction* m_openUrl{};
    int m_searchPage;
    QString m_lastSearchString;
};
