/***************************************************************************
 *   Copyright 2009 by Marco Martin <notmart@gmail.com>                    *
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

#ifndef NOTIFICATIONAREAWATCHER
#define NOTIFICATIONAREAWATCHER

#include <kdedmodule.h>

#include <QObject>
#include <QStringList>

class QDBusConnectionInterface;

class NotificationAreaWatcher : public KDEDModule
{
Q_OBJECT
public:
    NotificationAreaWatcher(QObject *parent, const QList<QVariant>&);
    ~NotificationAreaWatcher();

public Q_SLOTS:
    void registerService(const QString &service);

    QStringList registeredServices() const;

protected Q_SLOTS:
    void serviceChange(const QString& name,
                       const QString& oldOwner,
                       const QString& newOwner);

Q_SIGNALS:
    void serviceRegistered(const QString &service);
    //TODO: decide if this makes sense, the systray itself could notice the vanishing of items, but looks complete putting it here
    void serviceUnregistered(const QString &service);

private:
    QDBusConnectionInterface *m_dbusInterface;
    QStringList m_registeredServices;
};
#endif
