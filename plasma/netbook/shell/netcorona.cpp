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

#include "netcorona.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDir>
#include <QGraphicsLayout>

#include <KCmdLineArgs>
#include <KDebug>
#include <KDialog>
#include <KGlobalSettings>
#include <KStandardDirs>
#include <KWindowSystem>

#include <kephal/screens.h>

#include <Plasma/Containment>
#include <Plasma/DataEngineManager>

#include "plasmaapp.h"
#include "netview.h"
#include <plasma/containmentactionspluginsconfig.h>
#include <plasmagenericshell/scripting/scriptengine.h>

NetCorona::NetCorona(QObject *parent)
    : Plasma::Corona(parent)
{
    init();
}

void NetCorona::init()
{
    QDesktopWidget *desktop = QApplication::desktop();
    QObject::connect(desktop, SIGNAL(resized(int)), this, SLOT(screenResized(int)));
    connect(KWindowSystem::self(), SIGNAL(workAreaChanged()), this, SIGNAL(availableScreenRegionChanged()));

    Plasma::ContainmentActionsPluginsConfig desktopPlugins;
    desktopPlugins.addPlugin(Qt::NoModifier, Qt::MidButton, "paste");
    desktopPlugins.addPlugin(Qt::NoModifier, Qt::RightButton, "contextmenu");
    Plasma::ContainmentActionsPluginsConfig panelPlugins;
    panelPlugins.addPlugin(Qt::NoModifier, Qt::RightButton, "contextmenu");

    setContainmentActionsDefaults(Plasma::Containment::DesktopContainment, desktopPlugins);
    setContainmentActionsDefaults(Plasma::Containment::CustomContainment, desktopPlugins);
    setContainmentActionsDefaults(Plasma::Containment::PanelContainment, panelPlugins);
    setContainmentActionsDefaults(Plasma::Containment::CustomPanelContainment, panelPlugins);

    enableAction("lock widgets", false);
}

void NetCorona::loadDefaultLayout()
{
    evaluateScripts(ScriptEngine::defaultLayoutScripts());
    if (!containments().isEmpty()) {
        return;
    }

    QString defaultConfig = KStandardDirs::locate("appdata", "plasma-default-layoutrc");
    if (!defaultConfig.isEmpty()) {
        kDebug() << "attempting to load the default layout from:" << defaultConfig;
        loadLayout(defaultConfig);
        //FIXME: pretty brutal way to localize the names, don't see other ways
        foreach (Plasma::Containment *containment, containments()) {
            if (containment->activity() == "Search and launch") {
                containment->setActivity(i18n("Search and launch"));
            } else if (containment->activity() == "Newspaper") {
                containment->setActivity(i18n("Page one"));
            } 
        }
        return;
    }

    // used to force a save into the config file
    KConfigGroup invalidConfig;

    // FIXME: need to load the Netbook-specific containment
    // passing in an empty string will get us whatever the default
    // containment type is!
    Plasma::Containment* c = addContainmentDelayed(QString());

    if (!c) {
        return;
    }

    c->init();

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    bool isDesktop = args->isSet("desktop");

    if (isDesktop) {
        c->setScreen(0);
    }

    c->setWallpaper("image", "SingleImage");
    c->setFormFactor(Plasma::Planar);
    c->updateConstraints(Plasma::StartupCompletedConstraint);
    c->flushPendingConstraintsEvents();
    c->save(invalidConfig);

    emit containmentAdded(c);

    QVariantList netPanelArgs;
    netPanelArgs << PlasmaApp::self()->mainView()->width();
    c = addContainment("netpanel", netPanelArgs);
    /*
    loadDefaultApplet("systemtray", panel);

    foreach (Plasma::Applet* applet, panel->applets()) {
        applet->init();
        applet->flushPendingConstraintsEvents();
        applet->save(invalidConfig);
    }
    */

    requestConfigSync();
    /*
    foreach (Plasma::Containment *c, containments()) {
        kDebug() << "letting the world know about" << (QObject*)c;
        emit containmentAdded(c);
    }
    */
}

Plasma::Applet *NetCorona::loadDefaultApplet(const QString &pluginName, Plasma::Containment *c)
{
    QVariantList args;
    Plasma::Applet *applet = Plasma::Applet::load(pluginName, 0, args);

    if (applet) {
        c->addApplet(applet);
    }

    return applet;
}

Plasma::Containment *NetCorona::findFreeContainment() const
{
    foreach (Plasma::Containment *cont, containments()) {
        if ((cont->containmentType() == Plasma::Containment::DesktopContainment ||
            cont->containmentType() == Plasma::Containment::CustomContainment) &&
            cont->screen() == -1 && !offscreenWidgets().contains(cont)) {
            return cont;
        }
    }

    return 0;
}

void NetCorona::screenResized(int screen)
{
    int numScreens = QApplication::desktop()->numScreens();
    if (screen < numScreens) {
        foreach (Plasma::Containment *c, containments()) {
            if (c->screen() == screen) {
                // trigger a relayout
                c->setScreen(screen);
            }
        }
    }
}

int NetCorona::numScreens() const
{
    return Kephal::ScreenUtils::numScreens();
}

QRect NetCorona::screenGeometry(int id) const
{
    return Kephal::ScreenUtils::screenGeometry(id);
}

QRegion NetCorona::availableScreenRegion(int id) const
{
    return KWindowSystem::workArea();
}

void NetCorona::processUpdateScripts()
{
    evaluateScripts(ScriptEngine::pendingUpdateScripts());
}

void NetCorona::evaluateScripts(const QStringList &scripts)
{
    foreach (const QString &script, scripts) {
        ScriptEngine scriptEngine(this);
        connect(&scriptEngine, SIGNAL(printError(QString)), this, SLOT(printScriptError(QString)));
        connect(&scriptEngine, SIGNAL(print(QString)), this, SLOT(printScriptMessage(QString)));
        connect(&scriptEngine, SIGNAL(createPendingPanelViews()), PlasmaApp::self(), SLOT(createWaitingPanels()));

        QFile file(script);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text) ) {
            QString code = file.readAll();
            kDebug() << "evaluating startup script:" << script;
            scriptEngine.evaluateScript(code);
        }
    }
}

void NetCorona::printScriptError(const QString &error)
{
    kWarning() << "Startup script errror:" << error;
}

void NetCorona::printScriptMessage(const QString &error)
{
    kDebug() << "Startup script: " << error;
}


#include "netcorona.moc"

