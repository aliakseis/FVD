#include "preferences.h"

#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QtCore/QDir>
#include <QtCore/QTranslator>
#include <algorithm>

#include "branding.hxx"
#include "global_functions.h"
#include "globals.h"
#include "mainwindow.h"
#include "qtsingleapplication/qtsingleapplication.h"
#include "searchmanager.h"
#include "settings_declaration.h"
#include "ui_preferences.h"
#include "utilities/translatable.h"

namespace {

void setContentSize(QListWidget* wdt)
{
    QSize size = wdt->size();
    int shforCcol = wdt->sizeHintForColumn(0);
    int shforRow = wdt->sizeHintForRow(0);
    int countItem = wdt->count();
    int heightView = ((countItem * shforCcol - 1) / size.width() + 1) * shforRow;
    wdt->setMinimumHeight(heightView + 5);
}

void setListSites(const QString& strSite, QListWidget* listSites, const QStringList& langs)
{
    listSites->clear();
    QStringList sites = strSite.split(";", QString::SkipEmptyParts);
    foreach (const QString& str, langs)
    {
        auto* item = new QListWidgetItem(str, listSites);
        item->setCheckState(sites.contains(str) ? Qt::Checked : Qt::Unchecked);
    }

    setContentSize(listSites);
}

QString getCheckedSites(QListWidget* lWidg)
{
    QStringList sites;

    for (int i = 0; i < lWidg->count(); i++)
    {
        QListWidgetItem* item = lWidg->item(i);
        if (item->checkState() == Qt::Checked)
        {
            sites << item->text();
        }
    }

    return sites.join(";");
}


} // namespace

Preferences::Preferences(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::Preferences)
#ifdef DEVELOPER_FEATURES
      ,
      debugOutput(nullptr),
      m_debugListHandled(true)
#endif
{
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

    ui->listLang->setWrapping(true);
    ui->listSites->setWrapping(true);
    ui->listSites->setFlow(QListView::LeftToRight);

    QSettings settings;

#ifdef DEVELOPER_FEATURES
    setModal(false);

    utilities::LoggerTag::setHandler(this);
    utilities::setLogHandler(this);
    QWidget* debugWidget = new QWidget(this);
    ui->tabWidget->addTab(debugWidget, "Debug");
    QVBoxLayout* debugLayout = new QVBoxLayout();
    debugWidget->setLayout(debugLayout);

    debugList = new QListWidget();
    debugList->setStyleSheet("border: 1px solid grey;");

    QPushButton* updateVarButton = new QPushButton("Use it");
    QHBoxLayout* useVarLayout = new QHBoxLayout();
    useVarLayout->addWidget(new QLabel("Debug output filter: "));
    debugListVar = new QLineEdit(debugWidget);
    useVarLayout->addWidget(debugListVar);
    useVarLayout->addWidget(updateVarButton);

    debugOutput = new QTextEdit();
    debugOutput->setReadOnly(true);

    debugLayout->addLayout(useVarLayout);
    debugLayout->addWidget(debugList);
    debugLayout->addWidget(debugOutput);
    debugOutput->setStyleSheet("border: 1px solid grey;");

    qRegisterMetaType<QHash<QString, int> >("QHash<QString, int>");
    qRegisterMetaType<QtMsgType>("QtMsgType");
    VERIFY(connect(updateVarButton, SIGNAL(pressed()), SLOT(updateDebugFilter())));

    VERIFY(connect(this, SIGNAL(appendDeveloperLog()), SLOT(onAppendDeveloperLog())));
    VERIFY(connect(this, SIGNAL(appendDeveloperMessage(QtMsgType, QString)),
                   SLOT(onAppendDeveloperMessage(QtMsgType, QString))));

    setFilter(GET_SETTING(DebugFilterList));
#endif

#ifdef ALLOW_TRAFFIC_CONTROL
    ui->cbTrafficLimit->setChecked(
        settings.value(app_settings::IsTrafficLimited, app_settings::IsTrafficLimited_Default).toBool());
    ui->sbTrafficLimit->setValue(
        settings.value(app_settings::TrafficLimitKbs, app_settings::TrafficLimitKbs_Default).toInt());

    ui->cbTrafficLimit->setStyleSheet("QCheckBox { padding-right: -1px; }");
#else
    ui->cbTrafficLimit->hide();
    ui->sbTrafficLimit->hide();
    ui->labelTrafficLimit->hide();
#endif  // ALLOW_TRAFFIC_CONTROL

    setPrefStyleSheet();

    ui->listSites->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->listLang->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    VERIFY(connect(ui->listLang, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
                   SLOT(onCurrItemLangChanged(QListWidgetItem*))));
    VERIFY(connect(ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(onCurrTabChanged(int))));
    VERIFY(connect(ui->chbUseProxy, SIGNAL(stateChanged(int)), SLOT(onProxyStateChanged(int))));
    // VERIFY(connect(ui->btnUpdate, SIGNAL(clicked()), this, SIGNAL(signalCheckUpdate())));
    VERIFY(connect(ui->buttonBox, SIGNAL(accepted()), SLOT(accept())));
    VERIFY(connect(ui->buttonBox, SIGNAL(rejected()), SLOT(reject())));

    VERIFY(connect(ui->listSites, SIGNAL(itemPressed(QListWidgetItem*)), SLOT(toggleItem(QListWidgetItem*))));
}

Preferences::~Preferences()
{
    delete ui;
#ifdef DEVELOPER_FEATURES
    utilities::LoggerTag::setHandler(nullptr);
    utilities::setLogHandler(nullptr);
#endif
}

void Preferences::toggleItem(QListWidgetItem* item)
{
    item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
}

void Preferences::setPrefStyleSheet()
{
    QFile file(":/images/preferences/style.css");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString css = file.readAll();
        if (!css.isEmpty())
        {
            setStyleSheet(css);
        }
    }
}

void Preferences::done(int result)
{
    if (result == QDialog::Accepted)
    {
        if (ui->chbUseProxy->isChecked())
        {
            int pos;
            QString text = ui->leProxyAddress->text();
            if (ui->leProxyAddress->validator()->validate(text, pos) != QValidator::Acceptable)
            {
                ui->tabWidget->setCurrentIndex(2);
                QMessageBox::critical(this, Tr::Tr(PROJECT_FULLNAME_TRANSLATION), tr("Enter a valid proxy address."));
                ui->leProxyAddress->setFocus();
                return;
            }
            pos = 0;
            text = ui->leProxyPort->text();
            if (ui->leProxyPort->validator()->validate(text, pos) != QValidator::Acceptable)
            {
                ui->tabWidget->setCurrentIndex(2);
                QMessageBox::critical(this, Tr::Tr(PROJECT_FULLNAME_TRANSLATION), tr("Enter a valid proxy port."));
                ui->leProxyPort->setFocus();
                return;
            }
        }

        if (!apply())
        {
            return;
        }
    }

    QDialog::done(result);
}

bool Preferences::createSettingsDir(QString pathDir, const QString& strategyName)
{
    pathDir = global_functions::getSaveFolder(pathDir, strategyName);
    QDir path(pathDir);
    // absolute path is necessary since mkpath works weird otherwise (2 dirs are created instead of one)
    if (path.isRelative())
    {
        pathDir = path.absolutePath();
    }
    if (!path.exists(pathDir) && !path.mkpath(pathDir))
    {
        ui->tabWidget->setCurrentIndex(0);
        QMessageBox::critical(this, Tr::Tr(PROJECT_FULLNAME_TRANSLATION),
                              tr("The folder \"%1\" cannot be created. Please choose another one.").arg(pathDir));
        ui->lineEditOutputPath->setFocus();
        return false;
    }

    bool canProceed = false;
    QFileInfo newDirPath(pathDir);
    if (newDirPath.exists() && newDirPath.isDir())
    {
        QString testFileName(pathDir);
        if (!testFileName.endsWith(QDir::separator()))
        {
            testFileName.append(QDir::separator());
        }
        testFileName.append("test");
        while (QFile::exists(testFileName))
        {
            testFileName.append("-");
        }
        QFile testFile(testFileName);
        if (testFile.open(QIODevice::WriteOnly))
        {
            // SET_SETTING(saveVideoPath, pathDir);
            canProceed = true;
            testFile.close();
            testFile.remove();
        }
    }
    if (!canProceed)
    {
        ui->tabWidget->setCurrentIndex(0);
        QMessageBox::critical(
            this, Tr::Tr(PROJECT_FULLNAME_TRANSLATION),
            tr("The folder \"%1\" cannot be used, as there is no write access for %2. Please choose another one.")
                .arg(pathDir, Tr::Tr(PROJECT_FULLNAME_TRANSLATION)));
        ui->lineEditOutputPath->setFocus();
    }
    return canProceed;
}

int Preferences::execSelectFolder()
{
    VERIFY(QMetaObject::invokeMethod(ui->btnOutputPath, "clicked", Qt::QueuedConnection));
    return QDialog::exec();
}

void Preferences::fillLanguageComboBox()
{
    ui->listLang->clear();

    bool added = false;

    int iconWidth = 0;
    QIcon itemIcon(":/images/preferences/itemIcoLang.png");
    QList<QSize> sizes = itemIcon.availableSizes();
    if (!sizes.isEmpty())
    {
        iconWidth = sizes.first().width();
    }
    for (const auto& t : Translatable::availableLanguages())
    {
        const auto langStr = t.second;

        auto* item = new QListWidgetItem(langStr, ui->listLang);
        item->setIcon(itemIcon);
        item->setData(Qt::UserRole, t.first);
        // 50 is text margin, 20 is row's height
        item->setSizeHint(QSize(ui->listLang->fontMetrics().width(langStr) + 50 + iconWidth, 20));
        added = true;
    }

    if (!added)
    {
        auto* item = new QListWidgetItem(QString(LANGUAGE_NAME.key), ui->listLang);
        item->setIcon(QIcon(":/images/preferences/itemIcoLang.png"));
        item->setData(Qt::UserRole, "en");
    }
    QString settingsLang = QSettings().value(app_settings::ln, app_settings::ln_Default).toString();
    for (int i = 0; i < ui->listLang->count(); i++)
    {
        QListWidgetItem* item = ui->listLang->item(i);
        QString itemUserData = item->data(Qt::UserRole).toString();
        if (itemUserData == settingsLang)
        {
            ui->listLang->setCurrentItem(item);
            ui->labelSelectedLang->setText(ui->listLang->item(i)->text());
        }
    }
}

void Preferences::initPreferences()
{
    using namespace app_settings;

    QSettings settings;

    auto siteNames = SearchManager::Instance().allStrategiesNames();
    setListSites(settings.value(Sites, Sites_Default).toString(), ui->listSites, siteNames[0]);

    QString Octet = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
    ui->leProxyAddress->setValidator(
        new QRegExpValidator(QRegExp("^" + Octet + "\\." + Octet + "\\." + Octet + "\\." + Octet + "$"), this));
    ui->leProxyPort->setValidator(new QIntValidator(1, 65535, this));

    ui->lineEditOutputPath->setText(global_functions::GetVideoFolder());

    ui->sbSimultaneous->setValue(settings.value(maximumNumberLoads, maximumNumberLoads_Default).toInt());
    ui->leMaxItemPerSite->setValue(settings.value(resultsOnPage, resultsOnPage_Default).toInt());
    ui->cbSelectVideoBy->setCurrentIndex(settings.value(SearchSortOrder, SearchSortOrder_Default).toInt());
    ui->chbClearDownl->setChecked(settings.value(ClearDownloadsOnExit, ClearDownloadsOnExit_Default).toBool());

    ui->chbQuitAppWhenClosingWindow->setChecked(
        settings.value(QuitAppWhenClosingWindow, QuitAppWhenClosingWindow_Default).toBool());
    ui->chbOpenInFolder->setChecked(settings.value(OpenInFolder, OpenInFolder_Default).toBool());

    ui->chbUseProxy->setChecked(settings.value(UseProxy, UseProxy_Default).toBool());
    ui->leProxyAddress->setText(settings.value(ProxyAddress).toString());
    ui->leProxyPort->setText(QString::number(settings.value(ProxyPort).toUInt()));

    if (!ui->chbUseProxy->isChecked())
    {
        ui->leProxyAddress->setEnabled(false);
        ui->leProxyPort->setEnabled(false);
    }

#ifdef DEVELOPER_FEATURES
    debugListVar->setText(GET_SETTING(DebugFilterList));
#endif

    fillLanguageComboBox();
}

void Preferences::on_btnOutputPath_clicked()
{
    QString dir = QDir::toNativeSeparators(
        QFileDialog::getExistingDirectory(this, utilities::Tr::Tr(DOWNLOAD_TO_LABEL), ui->lineEditOutputPath->text(),
                                          /*QFileDialog::DontUseNativeDialog | */ QFileDialog::ShowDirsOnly));
    if (!dir.isEmpty())
    {
        ui->lineEditOutputPath->setText(dir);
    }
}

bool Preferences::apply()
{
    QString strSites = getCheckedSites(ui->listSites);

    QString pathDir = QDir::toNativeSeparators(QDir::cleanPath(ui->lineEditOutputPath->text()));
    QStringList listSites = strSites.split(";", QString::SkipEmptyParts);
    foreach (const QString& str, listSites)
    {
        if (!createSettingsDir(pathDir, str))
        {
            return false;
        }
    }

    using namespace app_settings;

    QSettings settings;

    settings.setValue(VideoFolder, pathDir);

    settings.setValue(Sites, strSites);

    QListWidgetItem* item = ui->listLang->currentItem();
    if (item != nullptr)
    {
        QString itemUserData = item->data(Qt::UserRole).toString();
        if (itemUserData != settings.value(ln, ln_Default))
        {
            settings.setValue(ln, itemUserData);
            auto* retranslator = dynamic_cast<Translatable*>(qApp);
            if (retranslator != nullptr)
            {
                retranslator->retranslateApp(itemUserData);
            }
            ui->retranslateUi(this);
        }
    }

    settings.setValue(maximumNumberLoads, ui->sbSimultaneous->value());
    settings.setValue(resultsOnPage, ui->leMaxItemPerSite->value());

    //	SET_SETTING(CheckUpdate, ui->chbCheckUpdate->isChecked());
    settings.setValue(ClearDownloadsOnExit, ui->chbClearDownl->isChecked());

    settings.setValue(QuitAppWhenClosingWindow, ui->chbQuitAppWhenClosingWindow->isChecked());
    settings.setValue(OpenInFolder, ui->chbOpenInFolder->isChecked());

    settings.setValue(SearchSortOrder, static_cast<strategies::SortOrder>(ui->cbSelectVideoBy->currentIndex()));
    settings.setValue(UseProxy, ui->chbUseProxy->isChecked());
    settings.setValue(ProxyAddress, ui->leProxyAddress->text());
    settings.setValue(ProxyPort, ui->leProxyPort->text().toUShort());

#ifdef ALLOW_TRAFFIC_CONTROL
    settings.setValue(IsTrafficLimited, ui->cbTrafficLimit->isChecked());
    settings.setValue(TrafficLimitKbs, ui->sbTrafficLimit->value());
#endif  // ALLOW_TRAFFIC_CONTROL

#ifdef DEVELOPER_FEATURES
    updateDebugFilter();
#endif

    emit newPreferencesApply();

    return true;
}

void Preferences::onCurrItemLangChanged(QListWidgetItem* item)
{
    if (item != nullptr)
    {
        ui->labelSelectedLang->setText(item->text());
    }
}


void Preferences::onCurrTabChanged(int index)
{
    QSize size(430, 470);
    if (1 == index)
    {
        size.setHeight(360);
    }
    else if (2 == index)
    {
        size.setHeight(260);
    }

    setMinimumSize(size);
    setMaximumSize(size);

    resize(minimumSize());
}


void Preferences::onProxyStateChanged(int state)
{
    bool is_enable = state != Qt::Unchecked;
    ui->leProxyAddress->setEnabled(is_enable);
    ui->leProxyPort->setEnabled(is_enable);
}


#ifdef DEVELOPER_FEATURES
void Preferences::onAppendDeveloperLog()
{
    debugList->clear();
    m_debugListHandled = true;
    m_messageMapLock.lockForRead();
    QHashIterator<QString, int> i(m_messageMap);
    while (i.hasNext())
    {
        i.next();
        debugList->addItem(QString("%1 (%2)").arg(i.key()).arg(i.value()));
    }
    m_messageMapLock.unlock();
}

void Preferences::onAppendDeveloperMessage(QtMsgType type, QString message)
{
    Q_UNUSED(type)
    if (debugOutput && message.length() > 0)
    {
        QString s =
            QDateTime::currentDateTimeUtc().toString("<font color=\"green\">[dd.MM.yy <b>hh:mm:ss</b>:zzz]</font>");
        QString filter;
        const int index = type - QtLoggerTagMsg;
        m_logFilterLock.lockForRead();
        if (index >= 0 && index < m_logFilter.size())
        {
            filter = m_logFilter[index];
        }
        m_logFilterLock.unlock();

        if (!filter.isEmpty())
        {
            s += "<font color=\"blue\">[<b>" + filter + "</b>]</font>";
        }

        s += message;

        // https://bugreports.qt-project.org/browse/QTBUG-29720
        // side effect here - stack overflow
        // when and if bug will be fixed you may remove this "if" statement
        if (!message.contains(QStringLiteral("Cannot create accessible interface for object")))
            debugOutput->append(s);
    }
}

QtMsgType Preferences::getTagId(const QString& tag)
{
    m_logFilterLock.lockForRead();
    auto it = std::lower_bound(m_logFilter.begin(), m_logFilter.end(), tag);
    QtMsgType result = (it != m_logFilter.end() && *it == tag) ? QtMsgType(it - m_logFilter.begin() + QtLoggerTagMsg)
                                                               : QtRejectedLoggerTagMsg;
    m_logFilterLock.unlock();
    m_messageMapLock.lockForWrite();
    ++m_messageMap[tag];
    m_messageMapLock.unlock();
    if (m_debugListHandled)
    {
        m_debugListHandled = false;
        emit appendDeveloperLog();
    }
    return result;
}

bool Preferences::log(QtMsgType type, const QString& text)
{
    if (type != QtRejectedLoggerTagMsg)
    {
        emit appendDeveloperMessage(type, text);
        return true;
    }
    return false;
}

void Preferences::updateDebugFilter()
{
    setFilter(debugListVar->text());
    SET_SETTING(DebugFilterList, debugListVar->text());
}

void Preferences::setFilter(const QString& filterTag)
{
    auto split = filterTag.split(",");
    split.sort();
    m_logFilterLock.lockForWrite();
    std::swap(m_logFilter, split);
    m_logFilterLock.unlock();
}

#endif  // DEVELOPER_FEATURES // DEBUG STUFF

void Preferences::showEvent(QShowEvent* /*unused*/)
{
    initPreferences();
    ui->tabWidget->setCurrentIndex(0);
    onCurrTabChanged(0);
}

bool Preferences::event(QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        auto* keyEvent = (QKeyEvent*)event;
        if (keyEvent->matches(QKeySequence::NextChild))
        {
            ui->tabWidget->setCurrentIndex((ui->tabWidget->currentIndex() + 1) % ui->tabWidget->count());
            return true;
        }
        if (keyEvent->matches(QKeySequence::PreviousChild))
        {
            int newIndex = ui->tabWidget->currentIndex() - 1;
            if (newIndex < 0)
            {
                newIndex = ui->tabWidget->count() - 1;
            }
            ui->tabWidget->setCurrentIndex(newIndex);
            return true;
        }
        if ((keyEvent->nativeVirtualKey() == Qt::Key_X) && (keyEvent->modifiers() == Qt::AltModifier))
        {
            MainWindow::Instance()->closeApp();
            return true;
        }
    }
    return QWidget::event(event);
}
