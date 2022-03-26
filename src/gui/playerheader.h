#pragma once

#include <QWidget>

class CustomDockWidget;

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

private:
    Ui::PlayerHeader* ui;
};
