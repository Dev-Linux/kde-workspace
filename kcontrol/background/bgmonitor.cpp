/* vi: ts=8 sts=4 sw=4

   This file is part of the KDE project, module kcmbackground.

   Copyright (C) 2002 Laurent Montel <montell@club-internet.fr>
   Copyright (C) 2003 Waldo Bastian <bastian@kde.org>
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License 
   version 2 as published by the Free Software Foundation.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
 */

#include <kurldrag.h>

#include "bgmonitor.h"

BGMonitor::BGMonitor(QWidget *parent, const char *name)
    : QWidget(parent, name)
{
    setAcceptDrops(true);
}


void BGMonitor::dropEvent(QDropEvent *e)
{
    if (!KURLDrag::canDecode(e))
        return;

    KURL::List uris;
    if (KURLDrag::decode(e, uris) && (uris.count() > 0)) {
        // TODO: Download remote file
        if (uris.first().isLocalFile())
           emit imageDropped(uris.first().path());
    }
}

void BGMonitor::dragEnterEvent(QDragEnterEvent *e)
{
    if (KURLDrag::canDecode(e))
        e->accept(rect());
    else
        e->ignore(rect());
}

#include "bgmonitor.moc"
