/*
 *   Copyright 2006, 2007 Aaron Seigo <aseigo@kde.org>
 *   Copyright 2010 Chani Armitage <chani@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2,
 *   or (at your option) any later version.
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

#ifndef PLASMA_APP_H
#define PLASMA_APP_H

#include <QHash>
#include <QList>
#include <QSize>
#include <QTimer>
#include <QWeakPointer>

#include <KUniqueApplication>

#include <Plasma/Plasma>
#include <plasma/packagemetadata.h>

class QSignalMapper;

namespace Plasma
{
    class AccessAppletJob;
    class Containment;
    class Context;
    class Corona;
    class Dialog;
} // namespace Plasma

namespace Kephal {
    class Screen;
} // namespace Kephal

class ControllerWindow;
class DesktopView;
class PanelView;
class DesktopCorona;
class InteractiveConsole;

class PlasmaApp : public KUniqueApplication
{
    Q_OBJECT
public:
    ~PlasmaApp();

    static PlasmaApp* self();
    static bool hasComposite();

    static void suspendStartup(bool completed);
    Plasma::Corona* corona();

    /**
     * Should be called when a panel hides or unhides itself
     */
    void panelHidden(bool hidden);

    /**
     * Returns the PanelViews
     */
    QList<PanelView*> panelViews() const;

    void showWidgetExplorer(int screen, Plasma::Containment *c);
    void showActivityManager(int screen, Plasma::Containment *c);
    void hideController(int screen);

    static bool isPanelContainment(Plasma::Containment *containment);

    /**
     * returns a list of all existing activities
     */
    QStringList listActivities();

#ifdef Q_WS_X11
    Atom m_XdndAwareAtom;
    Atom m_XdndEnterAtom;
    Atom m_XdndFinishedAtom;
    Atom m_XdndPositionAtom;
    Atom m_XdndStatusAtom;
    Atom m_XdndVersionAtom;
#endif

public Q_SLOTS:
    // DBUS interface. if you change these methods, you MUST run:
    // qdbuscpp2xml plasmaapp.h -o dbus/org.kde.plasma.App.xml
    void toggleDashboard();
    void showDashboard(bool show);

    void showInteractiveConsole();
    void loadScriptInInteractiveConsole(const QString &script);

    Q_SCRIPTABLE void quit();
    void setPerVirtualDesktopViews(bool perDesktopViews);
    bool perVirtualDesktopViews() const;
    void setFixedDashboard(bool fixedDashboard);
    bool fixedDashboard() const;

    void createWaitingPanels();
    void createWaitingDesktops();
    void createView(Plasma::Containment *containment);

    void showActivityManager();
    /**
     * create a new activity based on the active one
     */
    void cloneCurrentActivity();
    /**
     * create a new blank activity with @p plugin containment type
     */
    void createActivity(const QString &plugin);
    /**
     * create a new containment of type @p plugin and associate it with @p activity
     * FIXME maybe this belongs in desktopcorona
     */
    Plasma::Containment* addContainment(const QString &activity, const QString &plugin = QString());

    void updateActivityName(Plasma::Context *context);

protected:
#ifdef Q_WS_X11
    PanelView *findPanelForTrigger(WId trigger) const;
    bool x11EventFilter(XEvent *event);
#endif
    void setControllerVisible(bool show);

private:
    PlasmaApp();
    DesktopView* viewForScreen(int screen, int desktop) const;
    void showController(int screen, Plasma::Containment *c, bool widgetExplorerMode);

private Q_SLOTS:
    void setupDesktop();
    void cleanup();
    void containmentAdded(Plasma::Containment *containment);
    void containmentScreenOwnerChanged(int, int, Plasma::Containment*);
    void syncConfig();
    void panelRemoved(QObject* panel);
    void screenRemoved(int id);
    bool canRelocatePanel(PanelView * view, Kephal::Screen *screen);
    void compositingChanged();
    void configureContainment(Plasma::Containment*);
    void updateActions(Plasma::ImmutabilityType immutability);
    void checkVirtualDesktopViews(int numDesktops);
    void setWmClass(WId id);
    void remotePlasmoidAdded(Plasma::PackageMetadata metadata);
    void addRemotePlasmoid(const QString &location);
    void plasmoidAccessFinished(Plasma::AccessAppletJob *job);
    void wallpaperCheckedIn();
    void wallpaperCheckInTimeout();
    void dashboardClosed();

private:
    DesktopCorona *m_corona;
    QList<PanelView*> m_panels;
    QList<QWeakPointer<Plasma::Containment> > m_panelsWaiting;
    QList<QWeakPointer<Plasma::Containment> > m_desktopsWaiting;
    QList<DesktopView*> m_desktops;
    QTimer m_panelViewCreationTimer;
    QTimer m_desktopViewCreationTimer;
    QWeakPointer<InteractiveConsole> m_console;
    int m_panelHidden;
    QSignalMapper *m_mapper;
    QHash<int, QWeakPointer<ControllerWindow> > m_widgetExplorers;
    int m_startupSuspendWaitCount;
    bool m_ignoreDashboardClosures;
    int m_primaryScreen;
};

#endif // multiple inclusion guard
