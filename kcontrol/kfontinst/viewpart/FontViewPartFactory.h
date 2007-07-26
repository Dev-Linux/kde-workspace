#ifndef __FONT_VIEW_PART_FACTORY_H__
#define __FONT_VIEW_PART_FACTORY_H__

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

#include <klibloader.h>

class KComponentData;
class KAboutData;

namespace KFI
{

class CFontViewPartFactory : public KLibFactory
{
    Q_OBJECT

    public:

    CFontViewPartFactory();
    virtual ~CFontViewPartFactory();
    virtual QObject *createObject(QObject *parent = 0, const char *className = "QObject", const QStringList &args = QStringList());

    static const KComponentData &componentData();

    private:

    static KComponentData *theirInstance;
    static KAboutData *theirAbout;
};

}

#endif
