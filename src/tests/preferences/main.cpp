#include <QtGui/QApplication>
#include "preferences.h"
#include <QStringList>

int main(int argc, char* argv[])
{
	QApplication a(argc, argv);
	Preferences w;

	w.show();

	return a.exec();
}
