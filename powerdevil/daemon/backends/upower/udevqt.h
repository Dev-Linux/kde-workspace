/*
    Copyright 2009 Benjamin K. Stuhl <bks24@cornell.edu>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UDEVQT_H
#define UDEVQT_H

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

namespace UdevQt
{

class DevicePrivate;
class Device
{
    public:
        Device();
        Device(const Device &other);
        ~Device();
        Device &operator= (const Device &other);

        bool isValid() const;
        QString subsystem() const;
        QString devType() const;
        QString name() const;
        QString sysfsPath() const;
        int sysfsNumber() const;
        QString driver() const;
        QString primaryDeviceFile() const;
        QStringList alternateDeviceSymlinks() const;
        QStringList deviceProperties() const;
        QStringList sysfsProperties() const;
        Device parent() const;

        // ### should this really be a QVariant? as far as udev knows, everything is a string...
        // see also Client::devicesByProperty
        QVariant deviceProperty(const QString &name) const;
        QString decodedDeviceProperty(const QString &name) const;
        QVariant sysfsProperty(const QString &name) const;
        Device ancestorOfType(const QString &subsys, const QString &devtype) const;

    private:
        Device(DevicePrivate *devPrivate);
        friend class Client;
        friend class ClientPrivate;

        DevicePrivate *d;
};

typedef QList<Device> DeviceList;

class ClientPrivate;
class Client : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList watchedSubsystems READ watchedSubsystems WRITE setWatchedSubsystems)

    public:
        Client(QObject *parent = 0);
        Client(const QStringList &subsystemList, QObject *parent = 0);
        ~Client();

        QStringList watchedSubsystems() const;
        void setWatchedSubsystems(const QStringList &subsystemList);

        DeviceList allDevices();
        DeviceList devicesByProperty(const QString &property, const QVariant &value);
        DeviceList devicesBySubsystem(const QString &subsystem);
        Device deviceByDeviceFile(const QString &deviceFile);
        Device deviceBySysfsPath(const QString &sysfsPath);
        Device deviceBySubsystemAndName(const QString &subsystem, const QString &name);

    signals:
        void deviceAdded(const UdevQt::Device &dev);
        void deviceRemoved(const UdevQt::Device &dev);
        void deviceChanged(const UdevQt::Device &dev);
        void deviceOnlined(const UdevQt::Device &dev);
        void deviceOfflined(const UdevQt::Device &dev);

    private:
        friend class ClientPrivate;
        Q_PRIVATE_SLOT(d, void _uq_monitorReadyRead(int fd))
        ClientPrivate *d;
};

}

#endif
