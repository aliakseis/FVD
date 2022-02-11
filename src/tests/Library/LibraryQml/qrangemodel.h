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

#ifndef QRANGEMODEL_H
#define QRANGEMODEL_H

#include <QtCore/qobject.h>
#include <qgraphicsitem.h>
#include <qabstractslider.h>
//#include <QtDeclarative/qdeclarative.h>
//#include <QtQml>

class QRangeModelPrivate;

class QRangeModel : public QObject
{
	Q_OBJECT
	Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged USER true)
	Q_PROPERTY(qreal minimumValue READ minimum WRITE setMinimum NOTIFY rangeChanged)
	Q_PROPERTY(qreal maximumValue READ maximum WRITE setMaximum NOTIFY rangeChanged)
	Q_PROPERTY(qreal singleStep READ singleStep WRITE setSingleStep)
	Q_PROPERTY(qreal pageStep READ pageStep WRITE setPageStep)
	Q_PROPERTY(qreal position READ position WRITE setPosition NOTIFY positionChanged)
	Q_PROPERTY(qreal positionAtMinimum READ positionAtMinimum WRITE setPositionAtMinimum NOTIFY positionRangeChanged)
	Q_PROPERTY(qreal positionAtMaximum READ positionAtMaximum WRITE setPositionAtMaximum NOTIFY positionRangeChanged)
	Q_PROPERTY(bool inverted READ inverted WRITE setInverted)
	Q_PROPERTY(bool tracking READ isTracking WRITE setTracking)

public:
	QRangeModel(QObject* parent = 0);
	virtual ~QRangeModel();

	void sedate();
	void awake();

	void setRange(qreal min, qreal max);
	void setPositionRange(qreal min, qreal max);

	void setSingleStep(qreal step);
	qreal singleStep() const;

	void setPageStep(qreal step);
	qreal pageStep() const;

	void setTracking(bool enable);
	bool isTracking() const;

	void setMinimum(qreal min);
	qreal minimum() const;

	void setMaximum(qreal max);
	qreal maximum() const;

	void setPositionAtMinimum(qreal posAtMin);
	qreal positionAtMinimum() const;

	void setPositionAtMaximum(qreal posAtMax);
	qreal positionAtMaximum() const;

	void setInverted(bool inverted);
	bool inverted() const;

	qreal value() const;
	qreal position() const;

public Q_SLOTS:
	void singleStepAdd();
	void singleStepSub();
	void pageStepAdd();
	void pageStepSub();
	void toMinimum();
	void toMaximum();
	void setValue(qreal value);
	void setPosition(qreal position);

Q_SIGNALS:
	void valueChanged(qreal value);
	void positionChanged(qreal position);

	void rangeChanged(qreal min, qreal max);
	void positionRangeChanged(qreal min, qreal max);

protected:
	QRangeModel(QRangeModelPrivate& dd, QObject* parent);
	QRangeModelPrivate* d_ptr;

private:
	Q_DISABLE_COPY(QRangeModel)
	Q_DECLARE_PRIVATE(QRangeModel)

};

//QML_DECLARE_TYPE(QRangeModel)

#endif // QRANGEMODEL_H
