#pragma once

#include <QWidget>


namespace Ui
{
class DescriptionPanel;
}

class DescriptionPanel : public QWidget
{
    Q_OBJECT
public:
    DescriptionPanel(QWidget* parent);
    virtual ~DescriptionPanel();

    void setDescription(const QString& site, const QString& description, const QString& resolution = "");
    void resetDescription();

protected:
    void wheelEvent(QWheelEvent* event);

private:
    Ui::DescriptionPanel* ui;
};
