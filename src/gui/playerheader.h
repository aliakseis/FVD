#pragma once

#include <QWidget>

class CustomDockWidget;
class PreviewPanelButton;

namespace Ui
{
class PlayerHeader;
}

class PlayerHeader : public QWidget
{
    Q_OBJECT
    friend class CustomDockWidget;

public:
    explicit PlayerHeader(QWidget* parent = 0);
    ~PlayerHeader();

    void setVideoTitle(const QString& title);
    QString videoTitle() const;
    PreviewPanelButton* label() const;

private:
    Ui::PlayerHeader* ui;
};
