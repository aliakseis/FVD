#pragma once

#include <QWidget>


namespace Ui
{
class DescriptionPanel;
}

class VideoPlayerWidget;

class DescriptionPanel : public QWidget
{
    Q_OBJECT
public:
    DescriptionPanel(VideoPlayerWidget* parent);
    virtual ~DescriptionPanel();

    void setDescription(const QString& site, const QString& description, const QString& resolution = "");
    void resetDescription();

protected:
    void wheelEvent(QWheelEvent* event);

private:
    Ui::DescriptionPanel* ui;
};
