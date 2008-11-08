/*
 *   Copyright 2008 Aaron Seigo <aseigo@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "desktopcorona.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDir>
#include <QGraphicsLayout>

#include <KDebug>
#include <KDialog>
#include <KGlobalSettings>
#include <KStandardDirs>
#include <KWindowSystem>

#include <Plasma/Containment>
#include <Plasma/DataEngineManager>

DesktopCorona::DesktopCorona(QObject *parent)
    : Plasma::Corona(parent)
{
    init();
}

void DesktopCorona::init()
{
    QDesktopWidget *desktop = QApplication::desktop();
    m_numScreens = desktop->numScreens();
    connect(desktop, SIGNAL(resized(int)), this, SLOT(screenResized(int)));
    connect(KWindowSystem::self(), SIGNAL(workAreaChanged()),
            this, SIGNAL(availableScreenRegionChanged()));
}

void DesktopCorona::checkScreens()
{
    // quick sanity check to ensure we have containments for each screen!
    int numScreens = QApplication::desktop()->numScreens();
    for (int i = 0; i < numScreens; ++i) {
        if (!containmentForScreen(i)) {
            //TODO: should we look for containments that aren't asigned but already exist?
            Plasma::Containment* c = addContainment("desktop");
            c->setScreen(i);
            c->setFormFactor(Plasma::Planar);
            c->flushPendingConstraintsEvents();
            c->setActivity(i18n("Desktop"));
        } else if (i >= m_numScreens) {
            // now ensure that if our screen count changed we actually get views
            // for them, even if the Containment already existed for that screen
            // so we "lie" and emit a containmentAdded signal for every new screen
            // regardless of whether it actually already existed, or just got added
            // and therefore had this signal emitted. plasma can handle such
            // multiple emissions of the signal, and this is simply the most
            // straightforward way of accomplishing this
            kDebug() << "Notifying of new screen: " << i;
            emit containmentAdded(containmentForScreen(i));
        }
    }

    m_numScreens = numScreens;
}

int DesktopCorona::numScreens() const
{
    return QApplication::desktop()->numScreens();
}

QRect DesktopCorona::screenGeometry(int id) const
{
    return QApplication::desktop()->screenGeometry(id);
}

QRegion DesktopCorona::availableScreenRegion(int id) const
{
    // TODO: more precise implementation needed
    return QRegion(QApplication::desktop()->availableGeometry(id));
}

void DesktopCorona::loadDefaultLayout()
{
    QString defaultConfig = KStandardDirs::locate("appdata", "plasma-default-layoutrc");
    if (!defaultConfig.isEmpty()) {
        kDebug() << "attempting to load the default layout from:" << defaultConfig;
        loadLayout(defaultConfig);
        return;
    }

    QDesktopWidget *desktop = QApplication::desktop();
    int numScreens = desktop->numScreens();
    kDebug() << "number of screens is" << numScreens;
    int topLeftScreen = 0;
    QPoint topLeftCorner = desktop->screenGeometry(0).topLeft();

    // find our "top left" screen, use it as the primary
    for (int i = 0; i < numScreens; ++i) {
        QRect g = desktop->screenGeometry(i);
        kDebug() << "     screen " << i << "geometry is" << g;

        if (g.x() <= topLeftCorner.x() && g.y() >= topLeftCorner.y()) {
            topLeftCorner = g.topLeft();
            topLeftScreen = i;
        }
    }

    // used to force a save into the config file
    KConfigGroup invalidConfig;

    // create a containment for each screen
    for (int i = 0; i < numScreens; ++i) {
        // passing in an empty string will get us whatever the default
        // containment type is!
        Plasma::Containment* c = addContainmentDelayed(QString());

        if (!c) {
            continue;
        }

        c->init();
        c->setScreen(i);
        c->setWallpaper("image", "SingleImage");
        c->setFormFactor(Plasma::Planar);
        c->updateConstraints(Plasma::StartupCompletedConstraint);
        c->flushPendingConstraintsEvents();
        c->save(invalidConfig);

        // put a folder view on the first screen
        if (i == topLeftScreen) {
            QDir desktopFolder(KGlobalSettings::desktopPath());
            if (desktopFolder.exists()) {
                //TODO: should we also not show this if the desktop folder is empty?
                Plasma::Applet *folderView =  Plasma::Applet::load("folderview", c->id() + 1);
                if (folderView) {
                    c->addApplet(folderView, QPointF(KDialog::spacingHint(), KDialog::spacingHint()), true);
                    KConfigGroup config = folderView->config();
                    config.writeEntry("url", "desktop:/");
                    folderView->init();
                    folderView->flushPendingConstraintsEvents();
                }
            }
        }

        emit containmentAdded(c);
    }

    // make a panel at the bottom
    Plasma::Containment* panel = addContainmentDelayed("panel");

    if (!panel) {
        return;
    }

    panel->init();
    panel->setScreen(topLeftScreen);
    panel->setLocation(Plasma::BottomEdge);
    panel->updateConstraints(Plasma::StartupCompletedConstraint);
    panel->flushPendingConstraintsEvents();

    // some default applets to get a usable UI
    Plasma::Applet *applet = loadDefaultApplet("launcher", panel);
    if (applet) {
        applet->setGlobalShortcut(KShortcut("Alt+F1"));
    }

    loadDefaultApplet("notifier", panel);
    loadDefaultApplet("pager", panel);
    loadDefaultApplet("tasks", panel);
    loadDefaultApplet("systemtray", panel);

    Plasma::DataEngineManager *engines = Plasma::DataEngineManager::self();
    Plasma::DataEngine *power = engines->loadEngine("powermanagement");
    if (power) {
        const QStringList &batteries = power->query("Battery")["sources"].toStringList();
        if (!batteries.isEmpty()) {
            loadDefaultApplet("battery", panel);
        }
    }
    engines->unloadEngine("powermanagement");

    loadDefaultApplet("digital-clock", panel);

    foreach (Plasma::Applet* applet, panel->applets()) {
        applet->init();
        applet->flushPendingConstraintsEvents();
        applet->save(invalidConfig);
    }

    panel->save(invalidConfig);
    emit containmentAdded(panel);

    requestConfigSync();
    /*
    foreach (Plasma::Containment *c, containments()) {
        kDebug() << "letting the world know about" << (QObject*)c;
        emit containmentAdded(c);
    }
    */
}

Plasma::Applet *DesktopCorona::loadDefaultApplet(const QString &pluginName, Plasma::Containment *c)
{
    QVariantList args;
    Plasma::Applet *applet = Plasma::Applet::load(pluginName, 0, args);

    if (applet) {
        c->addApplet(applet);
    }

    return applet;
}

void DesktopCorona::screenResized(int screen)
{
    int numScreens = QApplication::desktop()->numScreens();
    if (screen < numScreens) {
        foreach (Plasma::Containment *c, containments()) {
            if (c->screen() == screen) {
                // trigger a relayout
                c->setScreen(screen);
            }
        }

        checkScreens(); // ensure we have containments for every screen
    } else {
        m_numScreens = numScreens;
    }
}

#include "desktopcorona.moc"

