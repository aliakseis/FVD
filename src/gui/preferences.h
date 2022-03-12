#pragma once

#include <QDialog>
#include <QReadWriteLock>
#include <QHash>
#include "utilities/loggertag.h"
#include "utilities/logger.h"
#include "ui_utils/tabbeddialogcombo.h"

class QListWidgetItem;
class QListWidget;
class QLineEdit;
class QTextEdit;

namespace Ui
{
class Preferences;
}

using ui_utils::TabbedDialogCombo;

class Preferences
	: public QDialog
#ifdef DEVELOPER_FEATURES
	, public utilities::LoggerTag::LoggerTagHandler, public utilities::LoggerHandler
#endif //#ifdef DEVELOPER_FEATURES
{
	Q_OBJECT

public:
	explicit Preferences(QWidget* parent = 0);
	~Preferences();
	virtual void done(int result) override;
	int execSelectFolder();

protected:

	bool event(QEvent* event);

signals:
	void newPreferencesApply();
	void signalCheckUpdate();

private slots:
	void on_btnOutputPath_clicked();
	void onCurrItemLangChanged(QListWidgetItem* item);
	void onCurrTabChanged(int index);
	void onProxyStateChanged(int state);

	static void toggleItem(QListWidgetItem* item);

private:
	void setPrefStyleSheet();
	inline void initPreferences();
	inline void fillLanguageComboBox();
	static void setListSites(const QString& strSite, QListWidget* listSites, const QStringList& langs);
	static void setContentSize(QListWidget* wdt);
	static QString getCheckedSites(QListWidget* lWidg);

	virtual void showEvent(QShowEvent*) override;

	bool createSettingsDir(QString dirPath, const QString& strategyName);

	bool apply();

#ifdef DEVELOPER_FEATURES
	QtMsgType getTagId(const QString& tag) override;
	bool log(QtMsgType type, const QString& text) override;

	void setFilter(const QString& filterTag);

signals:
	void appendDeveloperLog();
	void appendDeveloperMessage(QtMsgType type, QString message);

private slots:
	void updateDebugFilter();
	void onAppendDeveloperLog();
	void onAppendDeveloperMessage(QtMsgType type, QString message);

private:
	QLineEdit* debugListVar;
	QListWidget* debugList;
	QTextEdit* debugOutput;
	QReadWriteLock m_logFilterLock;
	QStringList m_logFilter;
	QReadWriteLock m_messageMapLock;
	QHash<QString, int> m_messageMap;
	volatile bool m_debugListHandled;
#endif //DEVELOPER_FEATURES

private:
	Ui::Preferences* ui;
	bool m_dataChanged = false;
};
