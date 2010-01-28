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

#include <QDir>
#include <QGraphicsWidget>
#include <QGraphicsSceneContextMenuEvent>
#include <QIcon>
#include <QMovie>

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
#include "statusnotifieritem_interface.h"
#include "systemtraytypes.h"

#include <netinet/in.h>

namespace SystemTray
{

class DBusSystemTrayTaskPrivate
{
public:
    DBusSystemTrayTaskPrivate(DBusSystemTrayTask *q)
        : q(q),
          movie(0),
          blinkTimer(0),
          blink(false),
          valid(false),
          embeddable(false)
    {
    }

    ~DBusSystemTrayTaskPrivate()
    {
        delete movie;
        delete blinkTimer;
    }

    QPixmap KDbusImageStructToPixmap(const KDbusImageStruct &image) const;
    QIcon imageVectorToPixmap(const KDbusImageVector &vector) const;
    void overlayIcon(QIcon *icon, QIcon *overlay);

    void iconDestroyed(QObject *obj);
    void refresh();

    void blinkAttention();
    void updateMovieFrame();

    void syncToolTip();
    void syncStatus(QString status);

    //callbacks
    void syncToolTip(const KDbusToolTipStruct &);
    void syncMovie(const QString &);
    void refreshCallback(QDBusPendingCallWatcher *call);


    DBusSystemTrayTask *q;
    QString typeId;
    QString name;
    QString title;
    QIcon icon;
    QIcon attentionIcon;
    QMovie *movie;
    QTimer *blinkTimer;
    QHash<Plasma::Applet *, Plasma::IconWidget *>iconWidgets;
    Plasma::ToolTipContent toolTipData;
    org::kde::StatusNotifierItem *statusNotifierItemInterface;
    bool blink : 1;
    bool valid : 1;
    bool embeddable : 1;
};


DBusSystemTrayTask::DBusSystemTrayTask(const QString &service, QObject *parent)
    : Task(parent),
      d(new DBusSystemTrayTaskPrivate(this))
{
    setObjectName("DBusSystemTrayTask");
    qDBusRegisterMetaType<KDbusImageStruct>();
    qDBusRegisterMetaType<KDbusImageVector>();
    qDBusRegisterMetaType<KDbusToolTipStruct>();

    d->typeId = service;
    d->name = service;

    d->statusNotifierItemInterface = new org::kde::StatusNotifierItem(service, "/StatusNotifierItem",
                                                                  QDBusConnection::sessionBus(), this);

    //TODO: how to behave if its not valid?
    d->valid = !service.isEmpty() && d->statusNotifierItemInterface->isValid();

    if (d->valid) {
        connect(d->statusNotifierItemInterface, SIGNAL(NewIcon()), this, SLOT(refresh()));
        connect(d->statusNotifierItemInterface, SIGNAL(NewAttentionIcon()), this, SLOT(refresh()));
        connect(d->statusNotifierItemInterface, SIGNAL(NewOverlayIcon()), this, SLOT(refresh()));
        connect(d->statusNotifierItemInterface, SIGNAL(NewToolTip()), this, SLOT(refresh()));
        connect(d->statusNotifierItemInterface, SIGNAL(NewStatus(QString)), this, SLOT(syncStatus(QString)));
        d->refresh();
    }
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

    DBusSystemTrayWidget *iconWidget = new DBusSystemTrayWidget(host, d->statusNotifierItemInterface);
    iconWidget->show();

    iconWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    iconWidget->setMinimumSize(KIconLoader::SizeSmall, KIconLoader::SizeSmall);
    iconWidget->setPreferredSize(KIconLoader::SizeSmallMedium, KIconLoader::SizeSmallMedium);

    connect(iconWidget, SIGNAL(destroyed(QObject *)), this, SLOT(iconDestroyed(QObject *)));
    d->iconWidgets[host] = iconWidget;

    //Delay because syncStatus needs that createWidget is done
    QTimer::singleShot(0, this, SLOT(refresh()));
    return iconWidget;
}

bool DBusSystemTrayTask::isValid() const
{
    return d->valid;
}

bool DBusSystemTrayTask::isEmbeddable() const
{
    return d->embeddable;
}

QString DBusSystemTrayTask::name() const
{
    return d->name;
}

QString DBusSystemTrayTask::typeId() const
{
    return d->typeId;
}

QIcon DBusSystemTrayTask::icon() const
{
    return d->icon;
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
    QDBusMessage message = QDBusMessage::createMethodCall(statusNotifierItemInterface->service(),
    statusNotifierItemInterface->path(), "org.freedesktop.DBus.Properties", "GetAll");

    message << statusNotifierItemInterface->interface();
    QDBusPendingCall call = statusNotifierItemInterface->connection().asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, q);
    q->connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher *)), q, SLOT(refreshCallback(QDBusPendingCallWatcher *)));
}


void DBusSystemTrayTaskPrivate::refreshCallback(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QVariantMap> reply = *call;
    QVariantMap properties = reply.argumentAt<0>();
    if (reply.isError()) {
        valid = false;
        embeddable = false;
    } else {
        QString cat = properties["Category"].toString();
        if (!cat.isEmpty()) {
            int index = q->metaObject()->indexOfEnumerator("Category");
            int key = q->metaObject()->enumerator(index).keyToValue(cat.toLatin1());

            if (key != -1) {
                q->setCategory((Task::Category)key);
            }
        }

        syncStatus(properties["Status"].toString());

        QString title = properties["Title"].toString();
        if (!title.isEmpty()) {
            name = title;

            if (typeId.isEmpty()) {
                typeId = title;
            }
        }

        QString id = properties["Id"].toString();
        if (!id.isEmpty()) {
            typeId = id;
        }

        QIcon overlay;
        QStringList overlayNames;

        //Icon
        {
            KDbusImageVector image;

            properties["OverlayIconPixmap"].value<QDBusArgument>() >> image;
            if (image.isEmpty()) {
                QString iconName = properties["OverlayIconName"].toString();
                if (!iconName.isEmpty()) {
                    overlayNames << iconName;
                    overlay = KIcon(iconName);
                }
            } else {
                overlay = imageVectorToPixmap(image);
            }

            properties["IconPixmap"].value<QDBusArgument>() >> image;
            if (image.isEmpty()) {
                QString iconName = properties["IconName"].toString();
                if (!iconName.isEmpty()) {
                    icon = KIcon(iconName, 0, overlayNames);

                    if (overlayNames.isEmpty() && !overlay.isNull()) {
                        overlayIcon(&icon, &overlay);
                    }
                }
            } else {
                icon = imageVectorToPixmap(image);
                if (!icon.isNull() && !overlay.isNull()) {
                    overlayIcon(&icon, &overlay);
                }
            }
        }

        if (q->status() != Task::NeedsAttention) {
            foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
                iconWidget->setIcon(icon);
                //This hardcoded number is needed to support pixel perfection of icons coming from other environments, in kde actualsize will jusrt return our usual 22x22
                QSize size = icon.actualSize(QSize(24, 24));
                iconWidget->setPreferredSize(iconWidget->sizeFromIconSize(qMax(size.width(), size.height())));
            }
        }

        //Attention icon
        {
            KDbusImageVector image;
            properties["AttentionIconPixmap"].value<QDBusArgument>() >> image;
            if (image.isEmpty()) {
                QString iconName = properties["AttentionIconName"].toString();
                if (!iconName.isEmpty()) {
                    attentionIcon = KIcon(iconName, 0, overlayNames);

                    if (overlayNames.isEmpty() && !overlay.isNull()) {
                        overlayIcon(&attentionIcon, &overlay);
                    }
                }
            } else {
                attentionIcon = imageVectorToPixmap(image);
                if (!attentionIcon.isNull() && !overlay.isNull()) {
                    overlayIcon(&icon, &overlay);
                }
            }
        }

        QString movieName = properties["AttentionMovieName"].toString();
        syncMovie(movieName);

        KDbusToolTipStruct toolTip;
        properties["ToolTip"].value<QDBusArgument>() >> toolTip;
        syncToolTip(toolTip);

        embeddable = true;
    }

    emit q->changed(q);
    delete call;
}

QPixmap DBusSystemTrayTaskPrivate::KDbusImageStructToPixmap(const KDbusImageStruct &icon) const
{
    //swap from network byte order if we are little endian
    if (QSysInfo::ByteOrder == QSysInfo::LittleEndian) {
        uint *uintBuf = (uint *) icon.data.data();
        for (uint i = 0; i < icon.data.size()/sizeof(uint); ++i) {
            *uintBuf = ntohl(*uintBuf);
            ++uintBuf;
        }
    }
    QImage iconImage( icon.width, icon.height, QImage::Format_ARGB32 );
    memcpy(iconImage.bits(), (uchar*)icon.data.data(), iconImage.numBytes());

    return QPixmap::fromImage(iconImage);
}

QIcon DBusSystemTrayTaskPrivate::imageVectorToPixmap(const KDbusImageVector &vector) const
{
    QIcon icon;

    for (int i = 0; i<vector.size(); ++i) {
        icon.addPixmap(KDbusImageStructToPixmap(vector[i]));
    }

    return icon;
}

void DBusSystemTrayTaskPrivate::overlayIcon(QIcon *icon, QIcon *overlay)
{
    //kDebug() << "WRRRRRRROOOOOOOOOOOOOONG!";
    QPixmap iconPixmap = icon->pixmap(KIconLoader::SizeSmall, KIconLoader::SizeSmall);

    QPainter p(&iconPixmap);

    const int size = KIconLoader::SizeSmall/2;
    p.drawPixmap(QRect(size, size, size, size), overlay->pixmap(size, size), QRect(0,0,size,size));
    p.end();

    //if an icon exactly that size wasn't found don't add it to the vector
    iconPixmap = icon->pixmap(KIconLoader::SizeSmallMedium, KIconLoader::SizeSmallMedium);
    if (iconPixmap.width() == KIconLoader::SizeSmallMedium) {
        const int size = KIconLoader::SizeSmall;
        QPainter p(&iconPixmap);
        p.drawPixmap(QRect(iconPixmap.width()-size, iconPixmap.height()-size, size, size), overlay->pixmap(size, size), QRect(0,0,size,size));
        p.end();
        icon->addPixmap(iconPixmap);
    }

    iconPixmap = icon->pixmap(KIconLoader::SizeMedium, KIconLoader::SizeMedium);
    if (iconPixmap.width() == KIconLoader::SizeMedium) {
        const int size = KIconLoader::SizeSmall;
        QPainter p(&iconPixmap);
        p.drawPixmap(QRect(iconPixmap.width()-size, iconPixmap.height()-size, size, size), overlay->pixmap(size, size), QRect(0,0,size,size));
        p.end();
        icon->addPixmap(iconPixmap);
    }

    iconPixmap = icon->pixmap(KIconLoader::SizeLarge, KIconLoader::SizeLarge);
    if (iconPixmap.width() == KIconLoader::SizeLarge) {
        const int size = KIconLoader::SizeSmallMedium;
        QPainter p(&iconPixmap);
        p.drawPixmap(QRect(iconPixmap.width()-size, iconPixmap.height()-size, size, size), overlay->pixmap(size, size), QRect(0,0,size,size));
        p.end();
        icon->addPixmap(iconPixmap);
    }

    //hopefully huge and enormous not necessary right now, since it's quite costly
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

void DBusSystemTrayTaskPrivate::syncMovie(const QString &movieName)
{
    delete movie;
    if (movieName.isEmpty()) {
        movie = 0;
        return;
    }
    if (QDir::isAbsolutePath(movieName)) {
        movie = new QMovie(movieName);
    } else {
        movie = KIconLoader::global()->loadMovie(movieName, KIconLoader::Panel);
    }
    if (movie) {
        q->connect(movie, SIGNAL(frameChanged(int)), q, SLOT(updateMovieFrame()));
    }
}



void DBusSystemTrayTaskPrivate::updateMovieFrame()
{
    Q_ASSERT(movie);
    QPixmap pix = movie->currentPixmap();
    foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
        iconWidget->setIcon(pix);
    }
}


//toolTip

void DBusSystemTrayTaskPrivate::syncToolTip(const KDbusToolTipStruct &tipStruct)
{
    if (tipStruct.title.isEmpty()) {
        foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
            Plasma::ToolTipManager::self()->clearContent(iconWidget);
        }
        return;
    }

    QIcon toolTipIcon;
    if (tipStruct.image.size() == 0) {
        toolTipIcon = KIcon(tipStruct.icon);
    } else {
        toolTipIcon = imageVectorToPixmap(tipStruct.image);
    }

    toolTipData.setMainText(tipStruct.title);
    toolTipData.setSubText(tipStruct.subTitle);
    toolTipData.setImage(toolTipIcon);
    foreach (Plasma::IconWidget *iconWidget, iconWidgets) {
        Plasma::ToolTipManager::self()->setContent(iconWidget, toolTipData);
    }
}


//Status

void DBusSystemTrayTaskPrivate::syncStatus(QString newStatus)
{
    Task::Status status = (Task::Status)q->metaObject()->enumerator(q->metaObject()->indexOfEnumerator("Status")).keyToValue(newStatus.toLatin1());

    if (q->status() == status) {
        return;
    }

    if (status == Task::NeedsAttention) {
        if (movie) {
            movie->stop();
            movie->start();
        } else if (!attentionIcon.isNull()) {
            if (!blinkTimer) {
                blinkTimer = new QTimer(q);
                q->connect(blinkTimer, SIGNAL(timeout()), q, SLOT(blinkAttention()));
                blinkTimer->start(500);
            }
        }
    } else {
        if (movie) {
            movie->stop();
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

    q->setStatus(status);
}

}

#include "dbussystemtraytask.moc"
