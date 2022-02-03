#pragma once

#include <QLabel>

class EllipsisLabel : public QLabel
{
	Q_OBJECT
public:
	EllipsisLabel(QWidget* parent);
	virtual ~EllipsisLabel();

	void setText(const QString& text);
	QString text();

protected:
	void resizeEvent(QResizeEvent* event);
private:
	QString title;
	void cutText();
};
