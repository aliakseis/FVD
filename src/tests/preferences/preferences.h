#ifndef Preferences_H
#define Preferences_H

#include <QDialog>

class QStringList;
class QListWidgetItem;

namespace Ui
{
class Preferences;
}

class QListWidget;

class Preferences : public QDialog
{
	Q_OBJECT

public:
	explicit Preferences(QWidget* parent = 0);
	~Preferences();
	QString readStyleSheet();
	bool setPrefStyleSheet();

	void setListLanguage(const QStringList& langs);
	void setListSites(const QStringList& langs);
	void setListAbultSites(const QStringList& langs);
	void setCurLanguage(const QString& lang);

private slots:
	void onCurrItemLangChanged(QListWidgetItem* item);
	void onCurrTabChanged(int index);

protected:
	void keyPressEvent(QKeyEvent* event);
	void setContentSize(QListWidget* wdt);
private:
	Ui::Preferences* ui;
};

#endif // Preferences_H
