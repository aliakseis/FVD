#pragma once

#include <QComboBox>

class SitesCombo : public QComboBox
{
	Q_OBJECT
public:
	explicit SitesCombo(QWidget* parent = 0);

	virtual void showPopup() override;
	virtual void hidePopup() override;

	virtual bool eventFilter(QObject* receiver, QEvent* event) override;

	void addSite(const QIcon& icon, const QString& text, const QString& strategyName);
	void addSection(const QIcon& icon, const QString& text);

	QStringList selectedSites() const;

	void updateToolTip();
	void fitContent();

protected:
	virtual void wheelEvent(QWheelEvent* event) override;
	virtual void paintEvent(QPaintEvent* event) override;
	virtual void resizeEvent(QResizeEvent* event) override;
	virtual void moveEvent(QMoveEvent* event) override;

public Q_SLOTS:
	void toggleCheckState(int row, bool programmatically = false);

private:
	int groupCount() const;

private:

	enum UserDataRole
	{
		GroupIndicator = Qt::UserRole,
		StrategyName,
		TextRole // used for keeping full strategy/site name because display name can be cut (with '...' added)
	};
	bool m_containerMousePress;
	QStringList m_selectedStrategies;
	int m_singleRow;
	QString m_displayText;
	QIcon m_displayIcon;
};
