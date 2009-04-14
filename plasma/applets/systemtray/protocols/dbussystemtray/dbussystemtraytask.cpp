/***************************************************************************
 *                                                                         *
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

#include "dbussystemtraytask.h"

#include <QGraphicsWidget>
#include <QGraphicsSceneContextMenuEvent>
#include <QIcon>
#include <QDBusReply>

#include <KIcon>
#include <KIconLoader>

#include <plasma/plasma.h>
#include <Plasma/Corona>
#include <Plasma/View>
#include <Plasma/IconWidget>
#include <Plasma/ToolTipContent>
#include <Plasma/ToolTipManager>
#include <Plasma/Applet>

#include "dbussystemtraywidget.h"
#include "notificationarea_interface.h"
#include "systemtraytypes.h"

namespace SystemTray
{

class DBusSystemTrayTaskPrivate
{
public:
    DBusSystemTrayTaskPrivate(DBusSystemTrayTask *q)
        : q(q),
          currentFrame(0),
          movieTimer(0),
          blinkTimer(0),
          blink(false)
    {
    }

    ~DBusSystemTrayTaskPrivate()
    {
        delete movieTimer;
        delete blinkTimer;
    }

    QPixmap iconDataToPixmap(const ImageStruct &icon) const;

    void iconDestroyed(QObject *obj);
    void refresh();
    void syncIcon();

    void syncAttentionIcon();
    void blinkAttention();
    void syncMovie();
    void updateMovieFrame();

    void syncToolTip();
    void syncStatus(QString status);



    DBusSystemTrayTask *q;
    QString name;
    QString title;
    DBusSystemTrayTask::ItemStatus status;
    DBusSystemTrayTask::ItemCategory category;
    QIcon icon;
    QIcon attentionIcon;
    QVector<QPixmap> movie;
    int currentFrame;
    QTimer *movieTimer;
    QTimer *blinkTimer;
    bool blink;
    QHash<Plasma::Applet *, Plasma::IconWidget *>iconWidgets;
    Plasma::ToolTipContent toolTipData;
    org::kde::NotificationAreaItem *notificationAreaItemInterface;
};


DBusSystemTrayTask::DBusSystemTrayTask(const QString &service)
    : Task(),
      d(new DBusSystemTrayTaskPrivate(this))
{
    setObjectName("DBusSystemTrayTask");
    qDBusRegisterMetaType<ImageStruct>();
    qDBusRegisterMetaType<ImageVector>();

    d->name = service;

    d->notificationAreaItemInterface = new org::kde::NotificationAreaItem(service, "/NotificationAreaItem",
                                                 QDBusConnection::sessionBus());
    d->notificationAreaItemInterface->title();

    d->category = (ItemCategory)metaObject()->enumerator(metaObject()->indexOfEnumerator("ItemCategory")).keyToValue(d->notificationAreaItemInterface->category().toLatin1());


    connect(d->notificationAreaItemInterface, SIGNAL(NewIcon()), this, SLOT(syncIcon()));
    connect(d->notificationAreaItemInterface, SIGNAL(NewAttentionIcon()), this, SLOT(syncAttentionIcon()));
    connect(d->notificationAreaItemInterface, SIGNAL(NewToolTip()), this, SLOT(syncToolTip()));
    connect(d->notificationAreaItemInterface, SIGNAL(NewStatus(QString)), this, SLOT(syncStatus(QString)));
}


DBusSystemTrayTask::~DBusSystemTrayTask()
{
    delete d;
}


QGraphicsWidget* DBusSystemTrayTask::createWidget(Plasma::Applet *host)
{
    if (d->iconWidgets.contains(host)) {
        return d->iconWidgets[host];
    }

    Plasma::IconWidget *iconWidget = new DBusSystemTrayWidget(host, d->notificationAreaItemInterface);
    iconWidget->show();

    iconWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    iconWidget->setMinimumSize(KIconLoader::SizeSmallMedium, KIconLoader::SizeSmallMedium);
    iconWidget->setPreferredSize(KIconLoader::SizeSmallMedium, KIconLoader::SizeSmallMedium);

    connect(iconWidget, SIGNAL(clicked()), d->notificationAreaItemInterface, SLOT(Activate()));
    iconWidget->installEventFilter(this);

    connect(iconWidget, SIGNAL(destroyed(QObject *)), this, SLOT(iconDestroyed(QObject *)));
    d->iconWidgets[host] = iconWidget;

    //Delay because syncStatus needs that createWidget is done
    QTimer::singleShot(0, this, SLOT(refresh()));
    return iconWidget;
}

bool DBusSystemTrayTask::isEmbeddable() const
{
    return true;
}

bool DBusSystemTrayTask::isValid() const
{
    return !d->name.isEmpty();
}

DBusSystemTrayTask::ItemCategory DBusSystemTrayTask::category() const
{
    return d->category;
}

QString DBusSystemTrayTask::name() const
{
    return d->name;
}


QString DBusSystemTrayTask::typeId() const
{
    return d->name;
}


QIcon DBusSystemTrayTask::icon() const
{
    return d->icon;
}


bool DBusSystemTrayTask::eventFilter(QObject *watched, QEvent *event)
{
    Plasma::IconWidget *iw = qobject_cast<Plasma::IconWidget *>(watched);
    if (d->iconWidgets.values().contains(iw) && event->type() == QEvent::GraphicsSceneContextMenu) {
        QGraphicsSceneMouseEvent *me = static_cast<QGraphicsSceneMouseEvent *>(event);
        d->notificationAreaItemInterface->ContextMenu(me->screenPos().x(), me->screenPos().y());
        return true;
    }
    return false;
}



//DBusSystemTrayTaskPrivate

void DBusSystemTrayTaskPrivate::iconDestroyed(QObject *obj)
{
    Plasma::IconWidget *iw = static_cast<Plasma::IconWidget *>(obj);

    QHash<Plasma::Applet *, Plasma::IconWidget*>::const_iterator i = iconWidgets.constBegin();
    while (i != iconWidgets.constEnd()) {
        if (i.value() == iw) {
            iconWidgets.remove(i.key());
            return;
        }
        ++i;
    }
}

void DBusSystemTrayTaskPrivate::refresh()
{
    syncIcon();
    syncAttentionIcon();
    syncMovie();
    syncToolTip();
    syncStatus(notificationAreaItemInterface->status());
}

QPixmap DBusSystemTrayTaskPrivate::iconDataToPixmap(const ImageStruct &icon) const
{
    QImage iconImage( icon.width, icon.height, QImage::Format_ARGB32 );
    memcpy(iconImage.bits(), (uchar*)icon.data.data(), iconImage.numBytes());

    return QPixmap::fromImage(iconImage);
}

//normal icon
void DBusSystemTrayTaskPrivate::syncIcon()
{
    if (notificationAreaItemInterface->icon().length() > 0) {
        icon = KIcon(notificationAreaItemInterface->icon());
    } else {
        icon = iconDataToPixmap(notificationAreaItemInterface->image());
    }

    if (status != DBusSystemTrayTask::NeedsAttention) {
        foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
            iconWidget->setIcon(icon);
        }
    }
}


//Attention icon and movie

void DBusSystemTrayTaskPrivate::syncAttentionIcon()
{
    if (notificationAreaItemInterface->attentionIcon().length() > 0) {
        attentionIcon = KIcon(notificationAreaItemInterface->attentionIcon());
    } else {
        attentionIcon = iconDataToPixmap(notificationAreaItemInterface->attentionImage());
    }
}

void DBusSystemTrayTaskPrivate::blinkAttention()
{
    if (blink) {
        foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
            iconWidget->setIcon(attentionIcon);
        }
    } else {
        foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
            iconWidget->setIcon(icon);
        }
    }
    blink = !blink;
}

void DBusSystemTrayTaskPrivate::syncMovie()
{
    ImageVector movieData = notificationAreaItemInterface->attentionMovie();
    movie = QVector<QPixmap>(movieData.size());

    if (!movieData.isEmpty()) {
        for (int i=0; i<movieData.size(); ++i) {
            movie[i] = iconDataToPixmap(movieData[i]);
        }
    }
}

void DBusSystemTrayTaskPrivate::updateMovieFrame()
{
    foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
        iconWidget->setIcon(movie[currentFrame]);
    }
    currentFrame = (currentFrame + 1) % movie.size();
}


//toolTip

void DBusSystemTrayTaskPrivate::syncToolTip()
{
    if (notificationAreaItemInterface->toolTipTitle().isEmpty()) {
        foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
            Plasma::ToolTipManager::self()->clearContent(iconWidget);
        }
        return;
    }

    QIcon toolTipIcon;
    if (notificationAreaItemInterface->toolTipIcon().length() > 0) {
        toolTipIcon = KIcon(notificationAreaItemInterface->toolTipIcon());
    } else {
        toolTipIcon = iconDataToPixmap(notificationAreaItemInterface->toolTipImage());
    }

    toolTipData.setMainText(notificationAreaItemInterface->toolTipTitle());
    toolTipData.setSubText(notificationAreaItemInterface->toolTipSubTitle());
    toolTipData.setImage(toolTipIcon);
    foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
        Plasma::ToolTipManager::self()->setContent(iconWidget, toolTipData);
    }
}


//Status

void DBusSystemTrayTaskPrivate::syncStatus(QString newStatus)
{
    status = (DBusSystemTrayTask::ItemStatus)q->metaObject()->enumerator(q->metaObject()->indexOfEnumerator("ItemStatus")).keyToValue(newStatus.toLatin1());

    if (status == DBusSystemTrayTask::NeedsAttention) {
        q->setOrder(Task::Last);
        if (q->hidden() & Task::AutoHidden) {
            q->setHidden(q->hidden()^Task::AutoHidden);
        }

        if (movie.size() != 0) {
            if (!movieTimer) {
                movieTimer = new QTimer(q);
                q->connect(movieTimer, SIGNAL(timeout()), q, SLOT(updateMovieFrame()));
                movieTimer->start(100);
            }
        } else if (!attentionIcon.isNull()) {
            if (!blinkTimer) {
                blinkTimer = new QTimer(q);
                q->connect(blinkTimer, SIGNAL(timeout()), q, SLOT(blinkAttention()));
                blinkTimer->start(500);
            }
        }
    } else {
        if (status == DBusSystemTrayTask::Active &&
            (q->hidden() & Task::AutoHidden)) {
            q->setHidden(q->hidden()^Task::AutoHidden);
        } else if (status == DBusSystemTrayTask::Passive) {
            q->setHidden(q->hidden()|Task::AutoHidden);
        }
        q->setOrder(Task::Normal);
        if (movieTimer) {
            movieTimer->stop();
            movieTimer->deleteLater();
            movieTimer = 0;
        }
        if (blinkTimer) {
            blinkTimer->stop();
            blinkTimer->deleteLater();
            blinkTimer = 0;
        }

        foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
            iconWidget->setIcon(icon);
        }
    }

    emit q->changed(q);
}

}

#include "dbussystemtraytask.moc"
