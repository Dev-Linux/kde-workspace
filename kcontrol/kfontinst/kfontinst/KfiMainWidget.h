#ifndef __KFI_MAIN_WIDGET_H__
#define __KFI_MAIN_WIDGET_H__

////////////////////////////////////////////////////////////////////////////////
//
// Class Name    : CKfiMainWindow
// Author        : Craig Drummond
// Project       : K Font Installer (kfontinst-kcontrol)
// Creation Date : 29/04/2001
// Version       : $Revision$ $Date$
//
////////////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
////////////////////////////////////////////////////////////////////////////////
// (C) Craig Drummond, 2001
////////////////////////////////////////////////////////////////////////////////

#include "KfiMainWidgetData.h"
#include "FontsWidget.h"

class CKfiMainWidget : public CKfiMainWidgetData
{
    Q_OBJECT

    public:

    CKfiMainWidget(QWidget *parent, const char *name=NULL);
    virtual ~CKfiMainWidget() {}

    void scanFonts()        { itsFonts->scanDirs(); }
    void configureSystem();
    void reset();
    void storeSettings()    { itsFonts->storeSettings(); }

    public slots:

    void wMadeChanges();

    signals:

    void madeChanges();
};

#endif
