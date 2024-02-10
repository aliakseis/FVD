#include "descriptionpanel.h"

#include <QScrollBar>
#include <QWheelEvent>

#include "remotevideoentity.h"
#include "ui_descriptionpanel.h"
#include "videoplayerwidget.h"

DescriptionPanel::DescriptionPanel(VideoPlayerWidget* parent)
    : QWidget(parent), ui(new Ui::DescriptionPanel)
{
    ui->setupUi(this);
    parent->m_descriptionPanel = this;
}

DescriptionPanel::~DescriptionPanel() { delete ui; }

void DescriptionPanel::setDescription(const QString& site, const QString& description,
    const QString& resolution, int rowNumber)
{
    QString text = site.isEmpty() ? tr("Direct") : site;
    if (!resolution.isEmpty())
    {
        text += QStringLiteral(" | ") + resolution; 
    }
    if (rowNumber > 0)
    {
        text += QStringLiteral(" | #%1").arg(rowNumber);
    }

    ui->descriptionSiteLabel->setText(text);
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
