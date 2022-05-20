#include "libraryform.h"

#include <QtQml/qqml.h>

#include <QLineEdit>
#include <QMessageBox>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlNetworkAccessManagerFactory>
#include <QtQuick/QQuickView>

#include "librarymodel.h"
#include "libraryqmllistener.h"
#include "qml/qdeclarativetoplevelitem.h"
#include "qml/qrangemodel.h"
#include "qmlimageprovider.h"
#include "searchmanager.h"
#include "ui_libraryform.h"
#include "utilities/utils.h"
#define QNetworkAccessManagerFactory QQmlNetworkAccessManagerFactory

#include <QDebug>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QSortFilterProxyModel>

#include "globals.h"
#include "utilities/translation.h"

class NetworkAccessManagerFactory : public QObject, public QNetworkAccessManagerFactory
{
public:
    NetworkAccessManagerFactory(QObject* parent = nullptr) : QObject(parent) {}
    QNetworkAccessManager* create(QObject* parent) override
    {
        auto* result = new QNetworkAccessManager(parent);

        auto* diskCache = new QNetworkDiskCache(result);
        QString cacheFolder = utilities::PrepareCacheFolder(QString("webcache%1").arg(QT_VERSION >> 16));
        qDebug() << "cacheFolder: " << cacheFolder << "maximumCacheSize: " << diskCache->maximumCacheSize();
        diskCache->setCacheDirectory(cacheFolder);

        result->setCache(diskCache);

        return result;
    }
};

LibraryForm::LibraryForm(QWidget* parent) : QWidget(parent), ui(new Ui::LibraryForm)
{
    m_model = new LibraryModel(this);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setSortRole(LibraryModel::RoleTimeDownload);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    auto* qmlListener = new LibraryQmlListener(this, m_model, m_proxyModel);

    ui->setupUi(this);
    ui->manageLabel->setVisible(false);
    ui->manageLabel->setImages(PreviewPanelButton::LeftArrow);

    Tr::SetTr(ui->cbSearch, &QLineEdit::setPlaceholderText, LIBRARY_SEARCH_INVITATION);

    VERIFY(connect(ui->btnSearch, SIGNAL(clicked()), SLOT(onSearch())));
    VERIFY(connect(ui->cbSearch, SIGNAL(returnPressed()), SLOT(onSearch())));

    ui->cbSearch->installEventFilter(qApp);

    m_view = new QQuickView();
    m_view->setResizeMode(QQuickView::SizeRootObjectToView);
    QQmlContext* ctxt = m_view->rootContext();
    ctxt->setContextProperty("qmllistener", qmlListener);
    ctxt->setContextProperty("libraryModel", m_proxyModel);
    ctxt->engine()->addImageProvider(QLatin1String("imageprovider"), new QmlImageProvider());
    m_view->setSource(QUrl("qrc:/qml2/LibraryView.qml"));
    ui->verticalLayout->addWidget(QWidget::createWindowContainer(m_view, this));

    utilities::Tr::MakeRetranslatable(this, ui);
}

LibraryForm::~LibraryForm() { delete ui; }

void LibraryForm::onActivated(const DownloadEntity* selEntity)
{
    ui->cbSearch->clear();
    onSearch();

    if (selEntity != nullptr)
    {
        int row = m_model->entityRow(selEntity);
        int proxyIndex = m_proxyModel->mapFromSource(m_model->index(row, 0)).row();
        QMetaObject::invokeMethod(m_view->rootObject(), "selectItem", Q_ARG(QVariant, proxyIndex));
    }

    m_model->synchronize(true);
}

void LibraryForm::onSearch() { m_proxyModel->setFilterRegExp(QRegExp(ui->cbSearch->text(), Qt::CaseInsensitive)); }

void LibraryForm::sortModel(Qt::SortOrder order)
{
    QElapsedTimer timer;
    timer.start();
    m_proxyModel->sort(0, order);
    qDebug() << __FUNCTION__ << "time spent, milliseconds:" << timer.restart();
}

QWidget* LibraryForm::manageWidget() const { return ui->manageLabel; }

QObject* LibraryForm::model() { return m_model; }

bool LibraryForm::exists(const DownloadEntity* entity)
{
    Q_ASSERT(m_model);
    if (entity != nullptr)
    {
        return (m_model->entityRow(entity) >= 0);
    }
    return false;
}
