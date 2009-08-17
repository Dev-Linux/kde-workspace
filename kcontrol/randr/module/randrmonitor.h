/********************************************************************

Copyright (C) 2008 Lubos Lunak <l.lunak@suse.cz>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef RANDRMONITOR_H
#define RANDRMONITOR_H

#include <kdedmodule.h>
#include <qwidget.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <fixx11h.h>

class RandrMonitorHelper;

class RandrMonitorModule
    : public KDEDModule
    {
    Q_OBJECT
    public:
        RandrMonitorModule(QObject* parent, const QList<QVariant>&);
        virtual ~RandrMonitorModule();
        void processX11Event( XEvent* e );
    private slots:
        void poll();
    private:
        void initRandr();
        void getRandrInfo( XRROutputChangeNotifyEvent* e, QString* change, QRect* rect );
        QStringList connectedMonitors() const;
        bool have_randr;
        int randr_base;
        int randr_error;
        Window window;
        QStringList currentMonitors;
        RandrMonitorHelper* helper;
        QDialog* dialog;
    };

class RandrMonitorHelper
    : public QWidget
    {
    Q_OBJECT
    public:
        RandrMonitorHelper( RandrMonitorModule* module );
    protected:
        virtual bool x11Event( XEvent* e );
    private:
        RandrMonitorModule* module;
    };


inline
RandrMonitorHelper::RandrMonitorHelper( RandrMonitorModule* m )
    : module( m )
    {
    }

#endif
