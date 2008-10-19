/*
 *   Copyright 2006, 2007 Aaron Seigo <aseigo@kde.org>
 *   Copyright 2008 Chani Armitage <chanika@gmail.com>
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

#include <QList>

#include <KUniqueApplication>

#include "ui_saverconfig.h"

namespace Plasma
{
    class Containment;
    class Corona;
} // namespace Plasma

class SaverView;
class KConfigDialog;

class PlasmaApp : public KUniqueApplication
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.plasmaoverlay.App")
public:
    ~PlasmaApp();

    static PlasmaApp* self();
    static bool hasComposite();

    Plasma::Corona* corona();

    void setActiveOpacity(qreal opacity);
    void setIdleOpacity(qreal opacity);
    qreal activeOpacity() const;
    qreal idleOpacity() const;

Q_SIGNALS:
    // DBUS interface.
    //if you change stuff, remember to regenerate with -S -M
    void viewCreated(uint id); //XXX this is actually a WId but qdbuscpp2xml is dumb
    void hidden();

public Q_SLOTS:
    // DBUS interface.
    //if you change stuff, remember to regenerate with -S -M
    /**
     * tell plasma to go into active mode, ready for interaction
     */
    void activate();
    /**
     * tell plasma to go into idle mode
     * this does not mean exit, it just means the computer is idle
     */
    void deactivate();
    /**
     * lock widgets
     */
    void lock();
    //not really slots, but we want them in dbus
    /**
     * @return the window id of our view, or 0 if there is none
     * again, this is really a WId but dbus doesn't like those.
     */
    uint viewWinId();
    /**
     * quit the application
     * this is a duplicate so we can have everything we need in one dbus interface
     */
    void quit();

private Q_SLOTS:
    void cleanup();
    void createView(Plasma::Containment *containment);
    void adjustSize(int screen);
    void dialogDestroyed(QObject *obj);
    void hideDialogs();
    void showDialogs();
    void createConfigurationInterface(KConfigDialog *parent);
    void configAccepted();

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    PlasmaApp(Display* display, Qt::HANDLE visual, Qt::HANDLE colormap);

    Plasma::Corona *m_corona;
    SaverView *m_view;
    QList<QWidget*> m_dialogs;
    Ui::saverConfig ui;

    qreal m_activeOpacity;
    qreal m_idleOpacity;
};

#endif // multiple inclusion guard
