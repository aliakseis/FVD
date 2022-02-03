#include "playerheader.h"
#include "ui_playerheader.h"

PlayerHeader::PlayerHeader(QWidget* parent) :
	QWidget(parent),
	ui(new Ui::PlayerHeader)
{
	ui->setupUi(this);
	ui->label->setImages(PreviewPanelButton::RightArrow);
}

PlayerHeader::~PlayerHeader()
{
	delete ui;
}

void PlayerHeader::setVideoTitle(const QString& title)
{
	ui->labelTitle->setText(title);
}

QString PlayerHeader::videoTitle() const
{
	return ui->labelTitle->text();
}
