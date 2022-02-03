#pragma once

#include <QObject>
class Test_Associations: public QObject
{
	Q_OBJECT
private slots:
	void setAsDefaultTorrent();
	void removeAsDefaultTorrent();
};
