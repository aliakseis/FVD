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

#include "qdeclarativetoplevelitem.h"

#include "qdeclarativetoplevelitem_p.h"

QtDeclarativeTopLevelItemPrivate::QtDeclarativeTopLevelItemPrivate()
    : targetItem(nullptr), transformDirty(0), keepInside(0)
{
}

QtDeclarativeTopLevelItemPrivate::~QtDeclarativeTopLevelItemPrivate() = default;

void QtDeclarativeTopLevelItemPrivate::clearDependencyList()
{
    Q_Q(QtDeclarativeTopLevelItem);
    for (int i = dependencyList.count() - 1; i >= 0; --i)
    {
        dependencyList.takeAt(i)->disconnect(this);
    }
    q->setOpacity(1);
    q->setVisible(false);
}

/*!
  \internal

  Set data bindings between the TopLevelItem and all the items it depend upon,
  including the targetItem and its ancestors.
*/
void QtDeclarativeTopLevelItemPrivate::initDependencyList()
{
    Q_Q(QtDeclarativeTopLevelItem);

    if ((targetItem == nullptr) || (targetItem->parentItem() == nullptr))
    {
        return;
    }

    // ### We are not listening for childrenChange signals in our parent
    // but that seems a little bit too much overhead. It can be done if
    // required in the future.
    setZFromSiblings();

    // The width of the TopLevelItem is the width of the targetItem
    connect(targetItem, SIGNAL(widthChanged()), SLOT(updateWidthFromTarget()));
    connect(targetItem, SIGNAL(heightChanged()), SLOT(updateHeightFromTarget()));

    // Now bind to events that may change the position and/or transforms of
    // the TopLevelItem

    // ### If we had access to QDeclarativeItem private we could do this
    //     in a better way, by adding ourselves to the changeListeners list
    //     in each item.
    //     The benefit is that we could update our data based on the change
    //     rather than having to recalculate the whole tree.

    QDeclarativeItem* item = targetItem;
    while (item->parentItem() != nullptr)
    {
        dependencyList << item;

        // We listen for events that can change the visibility and/or geometry
        // of the targetItem or its ancestors.
        connect(item, SIGNAL(opacityChanged()), SLOT(updateOpacity()));
        connect(item, SIGNAL(visibleChanged()), SLOT(updateVisible()));

        // ### We are not listening to changes in the "transform" property

        // 'updateTransform' may be expensive, so instead of calling it several
        // times, we call the schedule method instead, that also compresses
        // these events.
        connect(item, SIGNAL(xChanged()), SLOT(scheduleUpdateTransform()));
        connect(item, SIGNAL(yChanged()), SLOT(scheduleUpdateTransform()));
        connect(item, SIGNAL(rotationChanged()), SLOT(scheduleUpdateTransform()));
        connect(item, SIGNAL(scaleChanged()), SLOT(scheduleUpdateTransform()));
        connect(item, SIGNAL(transformOriginChanged(TransformOrigin)), SLOT(scheduleUpdateTransform()));

        // parentChanged() may be emitted from destructors and other sensible code regions.
        // By making this connection Queued we wait for the control to reach the event loop
        // allow for the scene to be in a stable state before doing our changes.
        connect(item, SIGNAL(parentChanged()), SLOT(updateParent()), Qt::QueuedConnection);

        item = item->parentItem();
    }

    // 'item' is the root item in the scene, make it our parent
    q->setParentItem(item);

    // Note that we did not connect to signals regarding opacity, visibility or
    // transform changes of our parent, that's because we take that effect
    // automatically, as it is our parent

    // OTOH we need to listen to changes in its size to re-evaluate the keep-inside
    // functionality.
    connect(item, SIGNAL(widthChanged()), SLOT(updateWidthFromTarget()));

    // Call slots for the first time
    updateHeightFromTarget();
    updateTransform();
    updateOpacity();
    updateVisible();
}

void QtDeclarativeTopLevelItemPrivate::scheduleUpdateTransform()
{
    if (transformDirty)
    {
        return;
    }

    transformDirty = 1;
    QMetaObject::invokeMethod(this, "updateTransform", Qt::QueuedConnection);
}

void QtDeclarativeTopLevelItemPrivate::updateTransform()
{
    Q_ASSERT(targetItem);
    Q_Q(QtDeclarativeTopLevelItem);
    updateWidthFromTarget();
    transformDirty = 0;
}

void QtDeclarativeTopLevelItemPrivate::updateOpacity()
{
    Q_ASSERT(targetItem);
    Q_Q(QtDeclarativeTopLevelItem);
}

void QtDeclarativeTopLevelItemPrivate::updateVisible()
{
    Q_ASSERT(targetItem);
    Q_Q(QtDeclarativeTopLevelItem);
}

void QtDeclarativeTopLevelItemPrivate::updateParent()
{
    Q_ASSERT(targetItem);
    clearDependencyList();
    initDependencyList();
}

void QtDeclarativeTopLevelItemPrivate::updateWidthFromTarget()
{
    Q_ASSERT(targetItem);
    Q_Q(QtDeclarativeTopLevelItem);

    // Reset position and size to those of the targetItem
    qreal newX = 0;
    qreal newWidth = targetItem->width();

    if (!keepInside)
    {
        q->setX(newX);
        q->setWidth(newWidth);
        return;
    }

    q->setX(newX);
    q->setWidth(newWidth);
}

void QtDeclarativeTopLevelItemPrivate::updateHeightFromTarget()
{
    Q_ASSERT(targetItem);
    Q_Q(QtDeclarativeTopLevelItem);
    q->setHeight(targetItem->height());
}

void QtDeclarativeTopLevelItemPrivate::setZFromSiblings()
{
    Q_Q(QtDeclarativeTopLevelItem);
    int maxZ = 0;

    const QList<QQuickItem*> siblings = q->parentItem()->childItems();
    for (int i = siblings.count() - 1; i >= 0; --i)
    {
        // Skip other topLevelItems
        QQuickItem* obj = siblings[i];
        if (qobject_cast<QtDeclarativeTopLevelItem*>(obj) != nullptr)
        {
            continue;
        }

        if (siblings[i]->z() > maxZ)
        {
            maxZ = siblings[i]->z();
        }
    }

    q->setZ(maxZ + 1);
}

QtDeclarativeTopLevelItem::QtDeclarativeTopLevelItem(QDeclarativeItem* parent)
    : QDeclarativeItem(parent), d_ptr(new QtDeclarativeTopLevelItemPrivate)
{
    d_ptr->q_ptr = this;
}

QtDeclarativeTopLevelItem::QtDeclarativeTopLevelItem(QtDeclarativeTopLevelItemPrivate& dd, QDeclarativeItem* parent)
    : QDeclarativeItem(parent), d_ptr(&dd)
{
    d_ptr->q_ptr = this;
}

QtDeclarativeTopLevelItem::~QtDeclarativeTopLevelItem() = default;

bool QtDeclarativeTopLevelItem::keepInside() const
{
    Q_D(const QtDeclarativeTopLevelItem);
    return d->keepInside;
}

void QtDeclarativeTopLevelItem::setKeepInside(bool keepInside)
{
    Q_D(QtDeclarativeTopLevelItem);
    if (static_cast<int>(keepInside) == d->keepInside)
    {
        return;
    }

    d->keepInside = keepInside;

    if (d->targetItem != nullptr)
    {
        d->updateWidthFromTarget();
    }
    emit keepInsideChanged(keepInside);
}

void QtDeclarativeTopLevelItem::itemChange(ItemChange change, const ItemChangeData& value)
{
    Q_D(QtDeclarativeTopLevelItem);

    if (d->targetItem == nullptr)
    {
        if (change == QQuickItem::ItemSceneChange)
        {
            d->targetItem = parentItem();
        }

        // Let the changes be finished before we start initDependencyList
        QMetaObject::invokeMethod(d, "initDependencyList", Qt::QueuedConnection);
    }
    QQuickItem::itemChange(change, value);
}
