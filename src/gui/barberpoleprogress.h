#ifndef BARBERPOLEPROGRESS_H
#define BARBERPOLEPROGRESS_H

#include <QWidget>
#include <QTimer>

class BarberPoleProgress : public QWidget
{
    Q_OBJECT
public:
    explicit BarberPoleProgress(QWidget *parent = nullptr);

    void startAnimation();
    void stopAnimation();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer m_periodicUpdateTimer;
};

#endif // BARBERPOLEPROGRESS_H
