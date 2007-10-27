/*
 *   Copyright (C) 2007 Menard Alexis <darktears31@gmail.com>
 *
 * This program is free software you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#include "hotplugengine.h"

#include <KConfigGroup>
#include <KDebug>
#include <KLocale>
#include <KStandardDirs>
#include <KDesktopFile>
#include "plasma/datacontainer.h"

//solid specific includes
#include <solid/devicenotifier.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/predicate.h>



HotplugEngine::HotplugEngine(QObject* parent, const QVariantList& args)
    : Plasma::DataEngine(parent)
{
    Q_UNUSED(args)
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(const QString &)),
            this, SLOT(onDeviceAdded(const QString &)));
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(const QString &)),
            this, SLOT(onDeviceRemoved(const QString &)));
    files = KGlobal::dirs()->findAllResources("data", "solid/actions/");
    //kDebug() <<files.size();
    new_device=false;
}

HotplugEngine::~HotplugEngine()
{

}

void HotplugEngine::onDeviceAdded(const QString &udi)
{
    Solid::Device device(udi);

    QStringList interestingDesktopFiles;
    //search in all desktop configuration file if the device inserted is a correct device
    foreach (QString path, files) {
        KDesktopFile cfg(path);
        QString string_predicate = cfg.desktopGroup().readEntry("X-KDE-Solid-Predicate");
        //kDebug()<<string_predicate;
        Solid::Predicate predicate = Solid::Predicate::fromString(string_predicate);
        if (predicate.matches(device)) {
            new_device=true;
            interestingDesktopFiles<<KUrl(path).fileName();
        }
    }

    if (new_device) {
        //kDebug()<<device.product();
        //kDebug()<<device.vendor();
        //kDebug()<< "number of interesting desktop file : " << interestingDesktopFiles.size();
        setData(udi, "added", true);
        setData(udi, "udi", device.udi());

        if (device.vendor().length()==0) {
            setData(udi, "text", device.product());
        } else {
            setData(udi, "text", device.vendor() + ' ' + device.product());
        }
        setData(udi, "icon", device.icon());
        setData(udi, "predicateFiles", interestingDesktopFiles);

        kDebug() << "add hardware solid : " << udi;
        checkForUpdates();
    }

    new_device=false;
}

void HotplugEngine::onDeviceRemoved(const QString &udi)
{
    removeSource(udi);
    Solid::Device device(udi);

    kDebug() << "remove hardware solid : " << udi;

    checkForUpdates();
}

#include "hotplugengine.moc"
