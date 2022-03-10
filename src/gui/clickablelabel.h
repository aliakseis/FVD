#pragma once

#include <QLabel>

class ClickableLabel : public QLabel
{
	Q_OBJECT
public:
	ClickableLabel(QWidget* parent);
	virtual ~ClickableLabel();
protected:
	void mousePressEvent(QMouseEvent* event) override;
signals:
	void clicked();
};
