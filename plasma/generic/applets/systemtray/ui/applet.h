/***************************************************************************
 *   applet.h                                                              *
 *                                                                         *
 *   Copyright (C) 2008 Jason Stubbs <jasonbstubbs@gmail.com>              *
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

#ifndef APPLET_H
#define APPLET_H

#include <plasma/popupapplet.h>

#include "ui_protocols.h"
#include "ui_autohide.h"
#include "ui_plasmoidtasks.h"

#include "../core/task.h"

namespace Plasma
{
class ExtenderItem;
class TabBar;
class Dialog;
}

namespace SystemTray
{

class Manager;
class TaskArea;

class Applet : public Plasma::PopupApplet
{
    Q_OBJECT

public:
    explicit Applet(QObject *parent, const QVariantList &arguments = QVariantList());
    ~Applet();

    void init();
    QGraphicsWidget *graphicsWidget();
    void constraintsEvent(Plasma::Constraints constraints);
    void setGeometry(const QRectF &rect);
    Manager *manager() const;
    QSet<Task::Category> shownCategories() const;

protected:
    void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect &contentsRect);
    void createConfigurationInterface(KConfigDialog *parent);
    void configChanged();
    void popupEvent(bool show);

    void mousePressEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event); }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event); }
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) { Q_UNUSED(event); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) { Q_UNUSED(event); }

private slots:
    void configAccepted();
    void propogateSizeHintChange(Qt::SizeHint which);
    void checkSizes();
    void checkDefaultApplets();

private:
    void setTaskAreaGeometry();

    static SystemTray::Manager *s_manager;
    static int s_managerUsage;

    TaskArea *m_taskArea;
    TaskArea *m_hiddenTaskArea;
    QWeakPointer<QWidget> m_notificationInterface;
    QWeakPointer<QWidget> m_autoHideInterface;
    QWeakPointer<QWidget> m_plasmoidTasksInterface;
    QSet<Task::Category> m_shownCategories;
    QDateTime m_lastActivity;


    Plasma::FrameSvg *m_background;
    Plasma::Svg *m_icons;

    Ui::ProtocolsConfig m_notificationUi;
    Ui::AutoHideConfig m_autoHideUi;
    Ui::PlasmoidTasksConfig m_plasmoidTasksUi;
};

}


#endif
