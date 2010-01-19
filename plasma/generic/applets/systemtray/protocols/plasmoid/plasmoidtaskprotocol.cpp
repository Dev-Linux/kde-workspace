/***************************************************************************
 *   plasmoidprotocol.cpp                                                  *
 *                                                                         *
 *   Copyright (C) 2008 Jason Stubbs <jasonbstubbs@gmail.com>              *
 *   Copyright (C) 2008 Sebastian Kügler <sebas@kde.org>                   *
 *   Copyright (C) 2009 Marco Martin <notmart@gmail.com>                   *
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

#include "plasmoidtask.h"
#include "plasmoidtaskprotocol.h"

#include <KDebug>

#include <Plasma/Applet>

namespace SystemTray
{

PlasmoidProtocol::PlasmoidProtocol(QObject *parent)
    : Protocol(parent)
{
}


PlasmoidProtocol::~PlasmoidProtocol()
{
}


void PlasmoidProtocol::init()
{
}

void PlasmoidProtocol::forwardConstraintsEvent(Plasma::Constraints constraints)
{
    foreach (PlasmoidTask *task, m_tasks) {
        task->forwardConstraintsEvent(constraints);
    }
}

void PlasmoidProtocol::loadFromConfig(const KConfigGroup &cg, Plasma::Applet *parent)
{
    QHash<QString, PlasmoidTask*> existingTasks = m_tasks;
    m_tasks.clear();

    KConfigGroup appletGroup(&cg, "Applets");
    foreach (const QString &groupName, appletGroup.groupList()) {
        KConfigGroup childGroup(&appletGroup, groupName);
        QString appletName = childGroup.readEntry("plugin", QString());

        if (m_tasks.contains(appletName)) {
            continue;
        }

        if (existingTasks.contains(appletName)) {
            m_tasks.insert(appletName, existingTasks.value(appletName));
            existingTasks.remove(appletName);
            continue;
        }

        kDebug() << "Registering task with the manager" << appletName;

        PlasmoidTask *task = new PlasmoidTask(appletName, groupName.toInt(), this, parent);
        Plasma::Applet *applet = qobject_cast<Plasma::Applet *>(task->widget(parent, true));
        if (applet) {
            applet->restore(childGroup);
        }

        if (!task->isValid()) {
            // we failed to load our applet *sob*
            delete task;
            continue;
        }

        m_tasks.insert(appletName, task);
        connect(task, SIGNAL(taskDeleted(QString)), this, SLOT(cleanupTask(QString)));
        emit taskCreated(task);
        existingTasks.remove(appletName);
    }

    QHashIterator<QString, PlasmoidTask*> it(existingTasks);
    while (it.hasNext()) {
        it.next();
        Plasma::Applet *applet = qobject_cast<Plasma::Applet *>(it.value()->widget(parent, true));
        if (applet) {
            applet->destroy();
        }
    }
}

void PlasmoidProtocol::addApplet(const QString appletName, Plasma::Applet *parent)
{
    kDebug() << "Registering task with the manager" << appletName;

    PlasmoidTask *task = new PlasmoidTask(appletName, 0, this, parent);

    if (!task->isValid()) {
        // we failed to load our applet *sob*
        delete task;
        return;
    }

    m_tasks[appletName] = task;
    connect(task, SIGNAL(taskDeleted(QString)), this, SLOT(cleanupTask(QString)));
    emit taskCreated(task);
}

void PlasmoidProtocol::removeApplet(const QString appletName, Plasma::Applet *parent)
{
    Plasma::Applet *applet = qobject_cast<Plasma::Applet *>(m_tasks[appletName]->widget(parent, true));

    if (applet) {
        applet->destroy();
    }
}

void PlasmoidProtocol::cleanupTask(QString typeId)
{
    kDebug() << "task with typeId" << typeId << "removed";
    m_tasks.remove(typeId);
}

QStringList PlasmoidProtocol::applets(const Plasma::Applet *host) const
{
    QStringList list;
    QHashIterator<QString, PlasmoidTask *> i(m_tasks);

    while (i.hasNext()) {
        i.next();
        if (i.value()->host() == host) {
            list << i.key();
        }
    }

    return list;
}

}

#include "plasmoidtaskprotocol.moc"
