#ifndef LIBRARYQMLLISTENER_H
#define LIBRARYQMLLISTENER_H

#include <QObject>
#include "librarymodel.h"

class LibraryQmlListener : public QObject
{
	Q_OBJECT
public:
	explicit LibraryQmlListener(QObject* parent = 0);

	LibraryModel* m_model;

public slots:
	void onImageClicked(int index);
	void onDeleteClicked(int index);
	void onPlayClicked(int index);

};

#endif // LIBRARYQMLLISTENER_H
