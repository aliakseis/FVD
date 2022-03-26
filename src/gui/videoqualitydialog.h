#pragma once

#include <QDialog>
#include <QEventLoop>

namespace Ui
{
class VideoQualityDialog;
}
class QListWidgetItem;
class RemoteVideoEntity;

class VideoQualityDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VideoQualityDialog(RemoteVideoEntity* entity, QWidget* parent = 0);
    ~VideoQualityDialog();

    int exec();

    void move(const QPoint& pos);

public slots:
    void onLinksExtracted();

private slots:
    void onItemClicked(QListWidgetItem* item);

protected:
    bool event(QEvent* event);
    void resizeEvent(QResizeEvent* event);

private:
    Ui::VideoQualityDialog* ui;
    QEventLoop localEventLoop;
    RemoteVideoEntity* m_entity;
    QPoint m_initPos;
};
