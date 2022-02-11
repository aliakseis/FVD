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

#ifndef QRANGEMODEL_P_H
#define QRANGEMODEL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt Components API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qrangemodel.h"

class QRangeModelPrivate
{
	Q_DECLARE_PUBLIC(QRangeModel)
public:
	QRangeModelPrivate(QRangeModel* qq);
	virtual ~QRangeModelPrivate();

	void init();

	bool isSedated;
	bool signalsBlocked;

	qreal pos, posatmin, posatmax;
	qreal minimum, maximum, pageStep, singleStep, value;

	uint tracking : 1;
	uint inverted : 1;

	QRangeModel* q_ptr;

	inline qreal positionFromValue(qreal value)
	{
		const qreal scale = qreal(maximum - minimum) /
							qreal(posatmax - posatmin);
		return value / scale;
	}

	inline qreal valueFromPosition(qreal pos)
	{
		const qreal scale = qreal(maximum - minimum) /
							qreal(posatmax - posatmin);
		return pos * scale;
	}
};

#endif // QRANGEMODEL_P_H
