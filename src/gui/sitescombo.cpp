#include "sitescombo.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QPaintEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QStylePainter>
#include "utilities/utils.h"
#include "settings_declaration.h"
#include "mainwindow.h"
#include "branding.hxx"

using namespace ::utilities;

SitesCombo::SitesCombo(QWidget* parent) :
	QComboBox(parent),
	m_containerMousePress(false)
{
	VERIFY(connect(this, SIGNAL(activated(int)), this, SLOT(toggleCheckState(int))));

	view()->installEventFilter(this);
	view()->window()->installEventFilter(this);
	view()->viewport()->installEventFilter(this);
	this->installEventFilter(this);
}

void SitesCombo::showPopup()
{
	QComboBox::showPopup();
}

void SitesCombo::hidePopup()
{
	if (m_containerMousePress)
	{
		QSettings().setValue(app_settings::CheckedSites, selectedSites().join(";"));
		updateToolTip();
		QComboBox::hidePopup();
	}
}

bool SitesCombo::eventFilter(QObject* receiver, QEvent* event)
{
	switch (event->type())
	{
	case QEvent::MouseButtonPress:
		m_containerMousePress = (receiver == view()->window());
		break;
	case QEvent::MouseButtonRelease:
		m_containerMousePress = false;
		break;
	case QEvent::KeyPress:
	{
		auto* keyEvent = (QKeyEvent*)event;
		if (keyEvent->matches(QKeySequence::NextChild))
		{
            QSettings().setValue(app_settings::CheckedSites, selectedSites().join(";"));
			updateToolTip();
			QComboBox::hidePopup();
            MainWindow::Instance()->nextTab();
			return true;
		}
		if (keyEvent->matches(QKeySequence::PreviousChild))
		{
            QSettings().setValue(app_settings::CheckedSites, selectedSites().join(";"));
			updateToolTip();
			QComboBox::hidePopup();
            MainWindow::Instance()->prevTab();
			return true;
		}
		if ((keyEvent->nativeVirtualKey() == Qt::Key_X) && (keyEvent->modifiers() == Qt::AltModifier))
		{
            QSettings().setValue(app_settings::CheckedSites, selectedSites().join(";"));
            MainWindow::Instance()->closeApp();
            return true;
		}
	}
	break;
	default:
		break;
	}
	return false;
}

QStringList SitesCombo::selectedSites() const
{
	return m_selectedStrategies;
}

void SitesCombo::paintEvent(QPaintEvent* /*e*/)
{
	QStylePainter painter(this);
	painter.setPen(palette().color(QPalette::Text));

	// draw the combobox frame, focus rect and selected etc.
	QStyleOptionComboBox opt;
	initStyleOption(&opt);
	painter.drawComplexControl(QStyle::CC_ComboBox, opt);

	// draw the icon and text
	opt.currentIcon = m_displayIcon;
	opt.currentText = m_displayText;

	QStyle* pStyle = style();
	QRect arrowRect = pStyle->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxArrow, this);

	const int textMargin = 5; // 5 is just a margin between text and left/right border
	int textWidth = width() - arrowRect.width() - textMargin * 2;
	if (!opt.currentIcon.isNull())
	{
		textWidth -= opt.iconSize.width() + textMargin;
	}
	if (m_selectedStrategies.size() == count() - groupCount())
	{
		// special case when all items are selected
		opt.currentText = tr("All Sites");
	}
	else
	{
		opt.currentText = opt.fontMetrics.elidedText(opt.currentText, Qt::ElideRight, textWidth);
	}
	// increase overall width, so empty space instead of icons won't hold place
	opt.rect.setRight(width() + opt.iconSize.width());

	painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
}

void SitesCombo::toggleCheckState(int row, bool  /*programmatically*/ /* = false */)
{
	QVariant value = itemData(row, Qt::CheckStateRole);
	auto state = static_cast<Qt::CheckState>(value.toInt());
	bool isGroup = itemData(row, Qt::UserRole).toBool();
	int rowCount = model()->rowCount();

	if (isGroup)
	{
		int nextItemIndex = row + 1;
		while (nextItemIndex < rowCount)
		{
			if (itemData(nextItemIndex, GroupIndicator).toBool()/* && !(row == 0 && state == Qt::Checked)*/)
			{
				break;
			}

			if (state == Qt::Checked || state == Qt::PartiallyChecked)
			{
				setItemData(nextItemIndex, Qt::Unchecked, Qt::CheckStateRole);
			}
			else
			{
				setItemData(nextItemIndex, Qt::Checked, Qt::CheckStateRole);
			}

			++nextItemIndex;
		}
	}

	QComboBox::setItemData(row, (state == Qt::Unchecked ? Qt::Checked : Qt::Unchecked), Qt::CheckStateRole);

	QMap<int, int> groupStates;
	QStringList selectedSites;
	m_selectedStrategies.clear();
	int currentGroupIndex = -1;
	int groupCheckedCount = 0;
	int groupTotalItems = 0;
	int totalCheckedCount = 0;

	int i = 0;
	while (i < rowCount)
	{
		if (itemData(i, GroupIndicator).toBool())
		{
			groupTotalItems = 0;
			groupCheckedCount = 0;
			currentGroupIndex = i;
		}
		else
		{
			++groupTotalItems;
		}

		int state = itemData(i, Qt::CheckStateRole).toInt();
		if ((state == Qt::Checked) && (!itemData(i, GroupIndicator).toBool()))
		{
			selectedSites << itemData(i, TextRole).toString();
			m_selectedStrategies << itemData(i, StrategyName).toString();
			m_singleRow = i;
			++totalCheckedCount;
			++groupCheckedCount;
		}

		++i;

		if (((i < rowCount) && itemData(i, GroupIndicator).toBool()) || (i == rowCount))
		{
			if (groupTotalItems > 0)
			{
				if (groupTotalItems == groupCheckedCount)
				{
					groupStates[currentGroupIndex] = Qt::Checked;
				}
				else if ((groupTotalItems > 0) && (groupTotalItems > groupCheckedCount)  && (groupCheckedCount > 0))
				{
					groupStates[currentGroupIndex] = Qt::PartiallyChecked;
				}
				else
				{
					groupStates[currentGroupIndex] = Qt::Unchecked;
				}
			}
		}
	}

	int fullyCheckedCount = 0;
	int partaillyCheckedCount = 0;
	for (auto it = groupStates.begin(); it != groupStates.end(); ++it)
	{
		QComboBox::setItemData(it.key(), it.value(), Qt::CheckStateRole);

		if (it.value() == Qt::PartiallyChecked)
		{
			++partaillyCheckedCount;
		}
		else if (it.value() == Qt::Checked)
		{
			++fullyCheckedCount;
		}
	}

	// set icon and text
	if (totalCheckedCount == 1)
	{
		m_displayIcon = itemData(m_singleRow, Qt::DecorationRole).value<QIcon>();
		m_displayText = itemData(m_singleRow, TextRole).toString();
	}
	else if (groupStates[0] == Qt::Checked)
	{
		m_displayIcon = itemData(0, Qt::DecorationRole).value<QIcon>();
		m_displayText = itemData(0, TextRole).toString();
	}
	else if (partaillyCheckedCount == 0 && fullyCheckedCount == 1)
	{
		int chRow = groupStates.key(Qt::Checked, -1);
		Q_ASSERT(chRow >= 0);
		m_displayIcon = itemData(chRow, Qt::DecorationRole).value<QIcon>();
		m_displayText = itemData(chRow, TextRole).toString();
	}
	else if (groupStates[0] == Qt::PartiallyChecked && fullyCheckedCount > 0)
	{
		QStringList strlist;

		for (auto it = groupStates.begin(); it != groupStates.end(); ++it)
		{
			if (itemData(it.key(), Qt::CheckStateRole) == Qt::Checked)
			{
				strlist << itemData(it.key(), TextRole).toString();
			}
		}

		int i = 1;
		while ((i < rowCount) && (!itemData(i, GroupIndicator).toBool()))
		{
			if (itemData(i, Qt::CheckStateRole) == Qt::Checked)
			{
				strlist << itemData(i, TextRole).toString();
			}
			++i;
		}

		m_displayText = strlist.join(", ");
		m_displayIcon = QIcon();
	}
	else
	{
		m_displayText = selectedSites.join(", ");
		m_displayIcon = QIcon();
	}
}

int SitesCombo::groupCount() const
{
	int groupCount = 0;
	for (int i = 0; i < count(); ++i)
	{
		if (itemData(i, Qt::UserRole).toBool())
		{
			++groupCount;
		}
	}
	return groupCount;
}

void SitesCombo::addSite(const QIcon& icon, const QString& text, const QString& strategyName)
{
	int row = model()->rowCount();
	addItem(icon, text);
	setItemData(row, strategyName, StrategyName);
	setItemData(row, text, TextRole);
}

void SitesCombo::addSection(const QIcon& icon, const QString& text)
{
	int row = model()->rowCount();
	addItem(icon, text, true);
	setItemData(row, text, TextRole);
}

void SitesCombo::wheelEvent(QWheelEvent* /*e*/)
{

}

void SitesCombo::resizeEvent(QResizeEvent* event)
{
	fitContent();
	QComboBox::resizeEvent(event);
}

void SitesCombo::moveEvent(QMoveEvent* event)
{
	// hide popup if it is visible on moving the control
	// otherwise popup and combo box parts will have different positions
	if (view()->isVisible())
	{
        QSettings().setValue(app_settings::CheckedSites, selectedSites().join(";"));
		updateToolTip();
		QComboBox::hidePopup();
	}
	QComboBox::moveEvent(event);
}

void SitesCombo::updateToolTip()
{
	QString tooltipText;
	for (int i = 0; i < count(); ++i)
	{
		if (!itemData(i, StrategyName).isNull() && itemData(i, Qt::CheckStateRole) == Qt::Checked)
		{
			if (tooltipText.length() > 0)
			{
				tooltipText.append(", ");
			}
			tooltipText.append(itemText(i));
		}
	}
	setToolTip(tooltipText);
}

void SitesCombo::fitContent()
{
	for (int i = 0; i < model()->rowCount(); ++i)
	{
		QString cutText = fontMetrics().elidedText(itemData(i, TextRole).toString(), Qt::ElideRight, width() - 64); // 64 is just margin for two icons and text margins
		setItemText(i, cutText);
	}
}
