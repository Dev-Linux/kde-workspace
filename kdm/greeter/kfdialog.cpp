    /*

    Dialog class to handle input focus -- see headerfile
    $Id$

    Copyright (C) 1997, 1998 Steffen Hansen <hansen@kde.org>
    Copyright (C) 2000 Oswald Buddenhagen <ossi@kde.org>


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    */
 

#include "kfdialog.h"
#include <qapplication.h>
#include <X11/Xlib.h>

#include <unistd.h>
#include <stdio.h>

int
FDialog::exec()
{
    setResult(0);

    if (isModal()) {

	show();
#if 0	/* Enable this for qt 3.0 */
	qApp->processEvents();

	if (XGrabKeyboard ( qt_xdisplay(), winId(), True, GrabModeAsync,
			    GrabModeAsync, CurrentTime ) == GrabSuccess) {
	    QDialog::exec();
	    XUngrabKeyboard (qt_xdisplay(), CurrentTime);
	} else
	    hide();
#endif

	// Give focus back to parent:
	if( parentWidget() != 0)
	    parentWidget()->setActiveWindow();

    } else
	show();

    return result();
}

#include "kfdialog.moc"
