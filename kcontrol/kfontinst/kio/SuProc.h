#ifndef __SU_PROC_H__
#define __SU_PROC_H__

/*
 * KFontInst - KDE Font Installer
 *
 * Copyright 2003-2007 Craig Drummond <craig@kde.org>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <kdesu/su.h>
#include <QThread>

class QString;

using namespace KDESu;

namespace KFI
{

//
// SuProcess::exec blocks until command terminates - therefore
// we need to run this in a separate thread!
class CSuProc : public QThread
{
    public:

    CSuProc(QByteArray &sock, QString &passwd);

    private:

    void run();

    private:

    SuProcess itsProc;
    QString   &itsPasswd;
};

}

#endif
