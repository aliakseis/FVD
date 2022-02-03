#pragma once

#include <QLabel>

class PreviewPanelButton : public QLabel
{
	Q_OBJECT
public:
	enum Type { LeftArrow, RightArrow, Scale };

	PreviewPanelButton(QWidget* parent);
	virtual ~PreviewPanelButton();
	void setImages(Type type);
protected:
	void mousePressEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);

private:
	QPixmap m_pushedPixmap;
	QPixmap m_pixmap;
};
