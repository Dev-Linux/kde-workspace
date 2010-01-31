/***************************************************************************
 *   notificationstack.cpp                                                *
 *   Copyright (C) 2010 Marco Martin <notmart@gmail.com>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "notificationstack.h"
#include "../core/notification.h"
#include "notificationwidget.h"

#include <QGraphicsLinearLayout>
#include <QTimer>

#include <KDebug>

#include <Plasma/FrameSvg>
#include <Plasma/Dialog>

namespace SystemTray
{

NotificationStack::NotificationStack(QGraphicsItem *parent)
   : QGraphicsWidget(parent),
     m_size(4),
     m_underMouse(false)
{
    m_mainLayout = new QGraphicsLinearLayout(Qt::Vertical, this);
    m_canDismissTimer = new QTimer(this);
    m_canDismissTimer->setSingleShot(true);

    m_delayedRemoveTimer = new QTimer(this);
    m_delayedRemoveTimer->setSingleShot(true);
    connect(m_delayedRemoveTimer, SIGNAL(timeout()), this, SLOT(popNotification()));

    setAcceptsHoverEvents(true);
}

NotificationStack::~NotificationStack()
{
}

void NotificationStack::addNotification(Notification *notification)
{
    m_canDismissTimer->start(1000);
    connect(notification, SIGNAL(notificationDestroyed(SystemTray::Notification *)), this, SLOT(removeNotification(SystemTray::Notification *)));
    connect(notification, SIGNAL(expired(SystemTray::Notification *)), this, SLOT(delayedRemoveNotification(SystemTray::Notification *)));

    NotificationWidget *notificationWidget = new NotificationWidget(notification, this);
    notificationWidget->installEventFilter(this);
    notificationWidget->setAcceptsHoverEvents(this);
    notificationWidget->setBackgroundVisible(false);
    connect(notificationWidget, SIGNAL(actionTriggered(SystemTray::Notification *)), this, SLOT(removeNotification(SystemTray::Notification *)));

    m_notificationWidgets[notification] = notificationWidget;
    m_notifications.append(notification);

    if (m_notifications.size() > 1) {
        notificationWidget->setCollapsed(true);
    } else {
        m_currentNotificationWidget = notificationWidget;
    }

    if (m_notifications.size() > m_size) {
        bool found = false;

        //try to kill the oldest notification of the same app
        foreach (Notification *notif, m_notifications) {
            if (notif->applicationName() == notification->applicationName()) {
                m_notificationWidgets[notif]->deleteLater();
                m_notificationWidgets.remove(notif);
                m_notifications.removeAll(notif);
                found = true;
                break;
            }
        }
        //or kill the oldest one
        if (!found) {
            Notification *notif = m_notifications.first();
            m_notificationWidgets[notif]->deleteLater();
            m_notificationWidgets.remove(notif);
            m_notifications.pop_front();
        }
    }

    m_mainLayout->insertItem(0, notificationWidget);
    m_mainLayout->activate();
    updateGeometry();
    resize(effectiveSizeHint(Qt::PreferredSize));
}

void NotificationStack::removeNotification(SystemTray::Notification *notification)
{
    NotificationWidget *nw = m_notificationWidgets.value(notification);
    if (nw) {
        nw->deleteLater();
    }
    m_mainLayout->removeItem(nw);
    m_notificationWidgets.remove(notification);
    m_notifications.removeAll(notification);

    if (m_notifications.count() > 0) {
        setCurrentNotification(m_notifications.first());
    }

    if (m_notifications.count() == 0) {
        emit stackEmpty();
    }

    resize(sizeHint(Qt::MinimumSize, QSizeF()));
}

void NotificationStack::delayedRemoveNotification(SystemTray::Notification *notification)
{
    m_notificationsToRemove.append(notification);
    if (!m_underMouse) {
        m_delayedRemoveTimer->start(1000);
    }
}

void NotificationStack::setCurrentNotification(SystemTray::Notification *notification)
{
    if (m_notificationWidgets.contains(notification)) {
        if (m_currentNotificationWidget) {
            m_currentNotificationWidget.data()->setCollapsed(true);
        }
        m_currentNotificationWidget = m_notificationWidgets.value(notification);
        m_currentNotificationWidget.data()->setCollapsed(false);
    }
}

void NotificationStack::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    m_underMouse = true;
    m_delayedRemoveTimer->stop();
}

void NotificationStack::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    m_underMouse = false;
    m_delayedRemoveTimer->start(1000);
}

void NotificationStack::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    event->accept();
}

void NotificationStack::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)

    if (!m_canDismissTimer->isActive()) {
        emit hideRequested();
    }
}

NotificationWidget *NotificationStack::currentNotificationWidget() const
{
    if (m_currentNotificationWidget) {
        return m_currentNotificationWidget.data();
    } else {
        return 0;
    }
}



bool NotificationStack::eventFilter(QObject *watched, QEvent *event)
{
    NotificationWidget *nw = qobject_cast<NotificationWidget *>(watched);

    if (!nw) {
        return false;
    }

    if (event->type() == QEvent::GraphicsSceneHoverEnter) {
        if (m_currentNotificationWidget) {
            m_currentNotificationWidget.data()->setCollapsed(true);
        }
        nw->setCollapsed(false);
        m_currentNotificationWidget = nw;
        m_canDismissTimer->start(1000);
    } else if (event->type() == QEvent::GraphicsSceneMove) {
        emit updateRequested();
    }


    return false;
}

void NotificationStack::popNotification()
{
    if (m_notificationsToRemove.isEmpty()) {
        return;
    }

    Notification *notif = m_notificationsToRemove.first();
    removeNotification(notif);
    m_notificationsToRemove.pop_front();
    m_delayedRemoveTimer->start(1000);
}

}

#include "notificationstack.moc"
