/***************************************************************************
 *   Copyright (C) 2007 by Alexis Ménard <darktears31@gmail.com>           *
 *   Copyright (C) 2009 by Frederik Gladhorn <gladhorn@kde.org>            *
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

#include "lockout.h"

// Plasma
#include <Plasma/IconWidget>
#include <Plasma/ToolTipManager>

// Qt
#include <QtGui/QWidget> // QWIDGETSIZE_MAX
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QGraphicsLinearLayout>

// KDE
#include <KIcon>
#include <KJob>
#include <KAuthorized>
#ifndef Q_OS_WIN
#include <KConfigDialog>
#include <KSharedConfig>
#include <kworkspace/kworkspace.h>
#include <screensaver_interface.h>
#endif

#include <solid/control/powermanager.h>

// Windows
#ifdef Q_OS_WIN
#include <windows.h>
#endif // Q_OS_WIN

static const int MINBUTTONSIZE = 16;
static const int MARGINSIZE = 1;

LockOut::LockOut(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args)
{
#ifndef Q_OS_WIN
    setHasConfigurationInterface(true);
#endif
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
}

void LockOut::init()
{
    m_layout = new QGraphicsLinearLayout(this);
    m_layout->setContentsMargins(0,0,0,0);
    m_layout->setSpacing(0);

    //Tooltip strings maybe should be different (eg. "Leave..."->"Logout")?
    m_iconLock = new Plasma::IconWidget(KIcon("system-lock-screen"), "", this);
    connect(m_iconLock, SIGNAL(clicked()), this, SLOT(clickLock()));
    Plasma::ToolTipContent lockToolTip(i18n("Lock"),i18n("Lock the screen"),m_iconLock->icon());
    Plasma::ToolTipManager::self()->setContent(m_iconLock, lockToolTip);

    m_iconLogout = new Plasma::IconWidget(KIcon("system-shutdown"), "", this);
    connect(m_iconLogout, SIGNAL(clicked()), this, SLOT(clickLogout()));
    Plasma::ToolTipContent logoutToolTip(i18n("Leave..."),i18n("Logout, turn off or restart the computer"),m_iconLogout->icon());
    Plasma::ToolTipManager::self()->setContent(m_iconLogout, logoutToolTip);

    m_iconSleep = new Plasma::IconWidget(KIcon("system-suspend"), "", this);
    connect(m_iconSleep, SIGNAL(clicked()), this, SLOT(clickSleep()));
    Plasma::ToolTipContent sleepToolTip(i18n("Suspend"),i18n("Sleep (suspend to RAM)"),m_iconSleep->icon());
    Plasma::ToolTipManager::self()->setContent(m_iconSleep, sleepToolTip);
    
    m_iconHibernate = new Plasma::IconWidget(KIcon("system-suspend-hibernate"), "", this);
    connect(m_iconHibernate, SIGNAL(clicked()), this, SLOT(clickHibernate()));
    Plasma::ToolTipContent hibernateToolTip(i18n("Hibernate"),i18n("Hibernate (suspend to disk)"),m_iconHibernate->icon());
    Plasma::ToolTipManager::self()->setContent(m_iconHibernate, hibernateToolTip);
   
    configChanged();
}

void LockOut::configChanged()
{
    #ifndef Q_OS_WIN
        KConfigGroup cg = config();
        m_showLockButton = cg.readEntry("showLockButton", true);
        m_showLogoutButton = cg.readEntry("showLogoutButton", true);
        m_showSleepButton = cg.readEntry("showSleepButton", false);
        m_showHibernateButton = cg.readEntry("showHibernateButton", false);
    #endif
    
    countButtons();
    showButtons();
}

void LockOut::countButtons()
{
    m_visibleButtons = 0;
  
    if (m_showLockButton){
        m_visibleButtons++;
    }
    
    if ( m_showLogoutButton){
        m_visibleButtons++;
    }
    
    if (m_showSleepButton){
        m_visibleButtons++;
    }
    
    if (m_showHibernateButton){
        m_visibleButtons++;
    }
}

LockOut::~LockOut()
{
}

void LockOut::checkLayout()
{
    qreal left,top, right, bottom;
    getContentsMargins(&left,&top, &right, &bottom);
    int width = geometry().width() - left - right;
    int height = geometry().height() - top - bottom;

    Qt::Orientation direction = Qt::Vertical;
    int minWidth = 0;
    int minHeight = 0;

    switch (formFactor()) {
        case Plasma::Vertical:
            if (width >= (MINBUTTONSIZE + MARGINSIZE) * m_visibleButtons) {
                direction = Qt::Horizontal;
                height = qMax(width / m_visibleButtons, MINBUTTONSIZE);
                minHeight = MINBUTTONSIZE;
            } else {
                minHeight = MINBUTTONSIZE * m_visibleButtons + top + bottom;
            }
            break;

        case Plasma::Horizontal:
            if (height < (MINBUTTONSIZE + MARGINSIZE) * m_visibleButtons) {
                direction = Qt::Horizontal;
                minWidth = MINBUTTONSIZE * m_visibleButtons + left + right;
            } else {
                width = qMax(height / m_visibleButtons, MINBUTTONSIZE);
                minWidth = MINBUTTONSIZE;
            }
            break;

        default:
            if (width > height) {
                direction = Qt::Horizontal;
                minWidth = MINBUTTONSIZE * m_visibleButtons + left + right;
                minHeight = MINBUTTONSIZE + top + bottom;
            } else {
                minWidth = MINBUTTONSIZE + left + right;
                minHeight = MINBUTTONSIZE * m_visibleButtons + top + bottom;
            }
            break;
    }

    m_layout->setOrientation(direction);
    setMinimumSize(minWidth, minHeight);
    if (direction == Qt::Horizontal) {
        setPreferredSize(height * m_visibleButtons + left + right, height + top + bottom);
    } else {
        setPreferredSize(width + left + right, width * m_visibleButtons + top + bottom);
    }
}

void LockOut::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & Plasma::FormFactorConstraint ||
        constraints & Plasma::SizeConstraint) {
        checkLayout();
    }
}

void LockOut::clickLock()
{
    kDebug()<<"LockOut:: lock clicked ";

#ifndef Q_OS_WIN
    QString interface("org.freedesktop.ScreenSaver");
    org::freedesktop::ScreenSaver screensaver(interface, "/ScreenSaver",
                                              QDBusConnection::sessionBus());
    if (screensaver.isValid()) {
        screensaver.Lock();
    }
#else
    LockWorkStation();
#endif // !Q_OS_WIN
}

void LockOut::clickLogout()
{
    if (!KAuthorized::authorizeKAction("logout")) {
        return;
    }
    
    kDebug()<<"LockOut:: logout clicked ";
#ifndef Q_OS_WIN
    KWorkSpace::requestShutDown( KWorkSpace::ShutdownConfirmDefault,
                                 KWorkSpace::ShutdownTypeDefault,
                                 KWorkSpace::ShutdownModeDefault);
#endif
}

#include <KMessageBox>

void LockOut::clickSleep()
{
    if (KMessageBox::questionYesNo(0,
                                   i18n("Do you want to suspend to RAM (sleep)?"),
                                   i18n("Suspend"))
            != KMessageBox::Yes) {
        return;
    }
    // Check if powerdevil is running, and use its methods to suspend if available
    // otherwise go through Solid directly
    QStringList modules;
    QDBusInterface kdedInterface("org.kde.kded", "/kded", "org.kde.kded");
    QDBusReply<QStringList> reply = kdedInterface.call("loadedModules");
    if (reply.isValid() && reply.value().contains("powerdevil")) {
        kDebug() << "Using powerdevil to suspend";
        QDBusConnection dbus(QDBusConnection::sessionBus());
        QDBusInterface iface("org.kde.kded", "/modules/powerdevil", "org.kde.PowerDevil", dbus);
        iface.call("suspend", Solid::Control::PowerManager::ToRam);
    } else {
        kDebug() << "Powerdevil not available, using solid to suspend";
        KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
        job->start();
    }
}

void LockOut::clickHibernate()
{
    if (KMessageBox::questionYesNo(0,
                                   i18n("Do you want to suspend to disk (hibernate)?"),
                                   i18n("Suspend"))
            != KMessageBox::Yes) {
        return;
    }
    // Check if powerdevil is running, and use its methods to hibernate if available
    // otherwise go through Solid directly
    QStringList modules;
    QDBusInterface kdedInterface("org.kde.kded", "/kded", "org.kde.kded");
    QDBusReply<QStringList> reply = kdedInterface.call("loadedModules");
    if (reply.isValid() && reply.value().contains("powerdevil")) {
        kDebug() << "Using powerdevil to hibernate";
        QDBusConnection dbus(QDBusConnection::sessionBus());
        QDBusInterface iface("org.kde.kded", "/modules/powerdevil", "org.kde.PowerDevil", dbus);
        iface.call("suspend", Solid::Control::PowerManager::ToDisk);
    } else {
        kDebug() << "Powerdevil not available, using solid to hibernate";
        KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
        job->start();
    }
}

void LockOut::configAccepted()
{
#ifndef Q_OS_WIN
    bool changed = false;
    KConfigGroup cg = config();

    if (m_showLockButton != ui.checkBox_lock->isChecked()) {
        m_showLockButton = !m_showLockButton;
        cg.writeEntry("showLockButton", m_showLockButton);
        changed = true;
    }

    if (m_showLogoutButton != ui.checkBox_logout->isChecked()) {
        m_showLogoutButton = !m_showLogoutButton;
        cg.writeEntry("showLogoutButton", m_showLogoutButton);
        changed = true;
    }

    if (m_showSleepButton != ui.checkBox_sleep->isChecked()) {
        m_showSleepButton = !m_showSleepButton;
        cg.writeEntry("showSleepButton", m_showSleepButton);
        changed = true;
    }

    if (m_showHibernateButton != ui.checkBox_hibernate->isChecked()) {
        m_showHibernateButton = !m_showHibernateButton;
        cg.writeEntry("showHibernateButton", m_showHibernateButton);
        changed = true;
    }

    if (changed) {
        int oldButtonCount = m_visibleButtons;
        countButtons();
        showButtons();
        if (formFactor() != Plasma::Horizontal && formFactor() != Plasma::Vertical) {
            resize(size().width(), (size().height() / oldButtonCount) * m_visibleButtons);
        }
        emit configNeedsSaving();
    }
#endif
}

void LockOut::createConfigurationInterface(KConfigDialog *parent)
{
#ifndef Q_OS_WIN
    QWidget *widget = new QWidget(parent);
    ui.setupUi(widget);
    parent->addPage(widget, i18n("General"), Applet::icon());
    connect(parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(configAccepted()));

    ui.checkBox_lock->setChecked(m_showLockButton);
    ui.checkBox_logout->setChecked(m_showLogoutButton);
    ui.checkBox_sleep->setChecked(m_showSleepButton);
    ui.checkBox_hibernate->setChecked(m_showHibernateButton);
#endif
}

void LockOut::showButtons()
{
#ifdef Q_OS_WIN
    m_layout->addItem(m_iconLock);
#else
    //make sure we don't add a button twice in the layout
    //definitely not the best workaround...
    m_layout->removeItem(m_iconLock);
    m_layout->removeItem(m_iconLogout);
    m_layout->removeItem(m_iconSleep);
    m_layout->removeItem(m_iconHibernate);

    m_iconLock->setVisible(m_showLockButton);

    if (m_showLockButton) { 
        m_layout->addItem(m_iconLock);
    }

    m_iconLogout->setVisible(m_showLogoutButton);
    if (m_showLogoutButton) {
        m_layout->addItem(m_iconLogout);
    }

    m_iconSleep->setVisible(m_showSleepButton);
    if (m_showSleepButton) {
        m_layout->addItem(m_iconSleep);
    }

    m_iconHibernate->setVisible(m_showHibernateButton);
    if (m_showHibernateButton) {
        m_layout->addItem(m_iconHibernate);
    }

    setConfigurationRequired(!m_showLockButton && !m_showLogoutButton && !m_showSleepButton && !m_showHibernateButton);
    checkLayout();
#endif // !Q_OS_WIN
}

#include "lockout.moc"
