/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#ifndef MAIN_H
#define MAIN_H

#include <QCoreApplication>
#include <QEventLoop>

#include <solid/storageaccess.h>
#include <solid/opticaldrive.h>

class KJob;

namespace Solid {
namespace Control {
    class Authentication;
}
}

class SolidBluetooth : public QCoreApplication
{
    Q_OBJECT
public:
    SolidBluetooth(int &argc, char **argv) : QCoreApplication(argc, argv), m_error(0) {}

    static bool doIt();

    bool bluetoothListAdapters();
    bool bluetoothDefaultAdapter();
    bool bluetoothCreateDevice(const QString &adapterUbi, const QString &mac);
    bool bluetoothRemoveDevice(const QString &adapterUbi, const QString &mac);
    bool bluetoothGetProperties(const QString &adapterUbi);
#if 0    
    bool bluetoothGetRemoteName(const QString &adapterUbi, const QString &mac);
    bool bluetoothAdapterAddress(const QString &ubi);
    bool bluetoothAdapterName(const QString &ubi);
    bool bluetoothAdapterSetName(const QString &ubi, const QString &name);
    bool bluetoothAdapterMode(const QString &ubi);
    bool bluetoothAdapterSetMode(const QString &ubi, const QString &mode);
    bool bluetoothAdapterListConnections(const QString &ubi);
    bool bluetoothAdapterListBondings(const QString &ubi);
    bool bluetoothAdapterScan(const QString &ubi);

    bool bluetoothInputListDevices();
    bool bluetoothInputSetup(const QString &deviceUbi);
    bool bluetoothInputRemoveSetup(const QString &deviceUbi);
    bool bluetoothInputConnect(const QString &deviceUbi);
    bool bluetoothInputDisconnect(const QString &deviceUbi);

    bool bluetoothRemoteCreateBonding(const QString &adapterUbi, const QString &deviceUbi);
    bool bluetoothRemoteRemoveBonding(const QString &adapterUbi, const QString &deviceUbi);
    bool bluetoothRemoteHasBonding(const QString &adapterUbi, const QString &deviceUbi);
#endif

private:
    void connectJob(KJob *job);

    QEventLoop m_loop;
    int m_error;
    QString m_errorString;
private slots:
#if 0
    void slotStorageResult(Solid::ErrorType error, const QVariant &errorData);
    void slotResult(KJob *job);
    void slotPercent(KJob *job, unsigned long percent);
    void slotInfoMessage(KJob *job, const QString &message);
    void slotBluetoothDeviceFound(const QString &ubi, int deviceClass, int rssi);
    void slotBluetoothDiscoveryCompleted();
#endif    
};


#endif