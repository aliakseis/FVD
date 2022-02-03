#include "libraryqmllistener.h"

#include <QDebug>
#include <QMessageBox>
#include <QApplication>
#include <QMouseEvent>

LibraryQmlListener::LibraryQmlListener(QObject* parent) :
	QObject(parent)
	, m_model(nullptr)
{
}

void LibraryQmlListener::onImageClicked(int index)
{
	QString filePath = qvariant_cast<QString>(m_model->index(index).data(LibraryModel::RoleThumbnail));

}

void LibraryQmlListener::onDeleteClicked(int index)
{
	qDebug() << "C++ delete clicked" << index;
	m_model->removeRow(index);
}

void LibraryQmlListener::onPlayClicked(int index)
{
	QString filePath = qvariant_cast<QString>(m_model->index(index).data(LibraryModel::RoleThumbnail));
}

