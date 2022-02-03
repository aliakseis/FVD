#include "downloadscontrol.h"
#include "ui_downloadscontrol.h"
#include "downloadsortfiltermodel.h"

#include <QPainter>
#include <QPixmap>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QMessageBox>
#include <QDialogButtonBox>

#include <QDebug>
#include "utilities/utils.h"
#include "utilities/translation.h"
#include "branding.hxx"


const int DEFAULT_SPEED_LIMIT = 1000;

DownloadsControl::DownloadsControl(QWidget* parent, DownloadSortFilterModel* model) :
	QWidget(parent),
	ui(new Ui::DownloadsControl),
	timeout(2000),
	m_animation(new QPropertyAnimation(this, "windowOpacity", this)),
	m_proxyModel(model),
	dontStopTimer(false),
	isUpdatingState(false)
{
	ui->setupUi(this);

	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool /*| Qt::WindowStaysOnTopHint*/);

#ifndef ALLOW_TRAFFIC_CONTROL
	ui->lineTrafficLimit->hide();
	ui->cbTrafficLimit->hide();
	ui->sbTrafficLimit->hide();
#endif

	VERIFY(connect(&timer, SIGNAL(timeout()), this, SLOT(onTimeout())));

	utilities::Tr::MakeRetranslatable(this, ui);
}

DownloadsControl::~DownloadsControl()
{
	delete ui;
}

void DownloadsControl::on_btnStart_clicked()
{
	emit start();
}

void DownloadsControl::on_btnPause_clicked()
{
	emit pause();
}

void DownloadsControl::on_btnStop_clicked()
{
	emit stop();
}

void DownloadsControl::on_btnRemove_clicked()
{
	if (QMessageBox::question(
				nullptr,
				utilities::Tr::Tr(PROJECT_FULLNAME_TRANSLATION),
				tr("Are you sure you want to remove this download? If download completed file will not be removed."),
				QMessageBox::Yes, QMessageBox::No)
			== QDialogButtonBox::Yes)
	{
		emit remove();
	}
}

void DownloadsControl::on_btnRestart_clicked()
{
	emit reload();
}

void DownloadsControl::on_btnUp_clicked()
{
	emit up();
}

void DownloadsControl::on_btnDown_clicked()
{
	emit down();
	timer.setInterval(timeout);
}

void DownloadsControl::onTimeout()
{
	if (geometry().contains(QCursor::pos()))
	{
		return ;
	}

	hide();
	if (dontStopTimer)
	{
		return ;
	}

	timer.stop();
	dontStopTimer = false;
}

void DownloadsControl::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);

	QRegion reg1(3, 0, width() - 6, height(), QRegion::Rectangle);
	QRegion reg2(0, 3, width(), height() - 6, QRegion::Rectangle);
	QRegion reg3(2, 1, width() - 4, height() - 2, QRegion::Rectangle);
	QRegion reg4(1, 2, width() - 2, height() - 4, QRegion::Rectangle);

	QRegion resultReg = reg1 + reg2 + reg3 + reg4;
	setMask(resultReg);
}

void DownloadsControl::mouseMoveEvent(QMouseEvent* event)
{
	timer.setInterval(timeout);
	QWidget::mouseMoveEvent(event);
}

void DownloadsControl::focusInEvent(QFocusEvent* event)
{
	QWidget::focusInEvent(event);
}

void DownloadsControl::focusOutEvent(QFocusEvent* event)
{
	hide();
	QWidget::focusOutEvent(event);
}

void DownloadsControl::showAtCursor(int affectedRow)
{
	show();
	move(QCursor::pos());
	timer.start(timeout);
	m_affectedRow = affectedRow;
}

int DownloadsControl::affectedRow() const
{
	int actualRow = m_affectedRow;
	if (m_proxyModel != nullptr)
	{
		actualRow =  m_proxyModel->index(m_affectedRow, 0).row();
	}

	return actualRow;
}

void DownloadsControl::setState(State state, bool isUpEnabled, bool isDownEnabled, bool isStopEnabled, bool isStartEnabled, int speedLimit)
{
	isUpdatingState = true;

	ui->btnStart->setVisible(state == Paused);
	ui->btnStart->setEnabled(isStartEnabled);
	ui->btnPause->setVisible(state == Started);
	ui->btnUp->setEnabled(isUpEnabled);
	ui->btnDown->setEnabled(isDownEnabled);
	ui->btnStop->setEnabled(isStopEnabled);

#ifdef ALLOW_TRAFFIC_CONTROL
	ui->cbTrafficLimit->setChecked(speedLimit > 0);
	int absSpeedLimit = std::abs(speedLimit);
	if (0 == absSpeedLimit)
	{
		absSpeedLimit = DEFAULT_SPEED_LIMIT;
	}
	ui->sbTrafficLimit->setValue(absSpeedLimit);
#endif

	isUpdatingState = false;
}

void DownloadsControl::show()
{
	QWidget::show();
}

void DownloadsControl::hide()
{
	QWidget::hide();
}

void DownloadsControl::on_cbTrafficLimit_stateChanged(int state)
{
	if (isUpdatingState)
	{
		return;
	}

	int speedLimit = ui->sbTrafficLimit->value();
	if (state != Qt::Checked)
	{
		speedLimit = -speedLimit;
	}

	emit setSpeedLimit(speedLimit);
}

void DownloadsControl::on_sbTrafficLimit_valueChanged(int value)
{
	if (isUpdatingState)
	{
		return;
	}

	int speedLimit = value;
	if (!ui->cbTrafficLimit->isChecked())
	{
		speedLimit = -speedLimit;
	}

	emit setSpeedLimit(speedLimit);
}
