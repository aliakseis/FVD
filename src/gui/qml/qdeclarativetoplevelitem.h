/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Components project on Qt Labs.
**
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions contained
** in the Technology Preview License Agreement accompanying this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
****************************************************************************/

#ifndef QTDECLARATIVETOPLEVELITEM_H
#define QTDECLARATIVETOPLEVELITEM_H


#include <QQuickItem>
#define QDeclarativeItem QQuickItem

class QtDeclarativeTopLevelItemPrivate;
class QtDeclarativeTopLevelItem : public QDeclarativeItem
{
	Q_OBJECT
	Q_PROPERTY(bool keepInside READ keepInside WRITE setKeepInside NOTIFY keepInsideChanged FINAL)

public:
	QtDeclarativeTopLevelItem(QDeclarativeItem* parent = 0);
	virtual ~QtDeclarativeTopLevelItem();

	bool keepInside() const;
	void setKeepInside(bool keepInside);

Q_SIGNALS:
	void keepInsideChanged(bool keepInside);

protected:
	QtDeclarativeTopLevelItem(QtDeclarativeTopLevelItemPrivate& dd, QDeclarativeItem* parent = 0);

	virtual void itemChange(ItemChange, const ItemChangeData&) override;

	QScopedPointer<QtDeclarativeTopLevelItemPrivate> d_ptr;

private:
	Q_DISABLE_COPY(QtDeclarativeTopLevelItem)
	Q_DECLARE_PRIVATE(QtDeclarativeTopLevelItem)
};

QML_DECLARE_TYPE(QtDeclarativeTopLevelItem)



#endif // QTDECLARATIVETOPLEVELITEM_H
