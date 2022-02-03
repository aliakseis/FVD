#include "descriptionpanel.h"
#include "ui_descriptionpanel.h"
#include "remotevideoentity.h"
#include "videoplayerwidget.h"
#include <QScrollBar>
#include <QWheelEvent>

DescriptionPanel::DescriptionPanel(QWidget* parent) : QWidget(parent), ui(new Ui::DescriptionPanel)
{
	ui->setupUi(this);
	((VideoPlayerWidget*)parent)->m_descriptionPanel = this;
}

DescriptionPanel::~DescriptionPanel()
{
	delete ui;
}

void DescriptionPanel::setDescription(const QString& site, const QString& description, const QString& resolution)
{
	if (resolution.isEmpty())
	{
		ui->descriptionSiteLabel->setText(site);
	}
	else
	{
		ui->descriptionSiteLabel->setText(site + " | " + resolution);
	}
	ui->textEdit->setText(description);
}

void DescriptionPanel::resetDescription()
{
	ui->descriptionSiteLabel->setText("");
	ui->textEdit->setText("");
}

void DescriptionPanel::wheelEvent(QWheelEvent* event)
{
	QScrollBar* scrollBar = ui->textEdit->verticalScrollBar();
	if (scrollBar == nullptr || !scrollBar->isVisible())
	{
		QWidget::wheelEvent(event);
	}
}
