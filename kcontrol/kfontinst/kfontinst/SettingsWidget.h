#ifndef __SETTINGS_WIDGET_H__
#define __SETTINGS_WIDGET_H__

////////////////////////////////////////////////////////////////////////////////
//
// Class Name    : CSettingsWidget
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

#include <kurlrequester.h>
#include "SettingsWidgetData.h"

class CSettingsWidget : public CSettingsWidgetData
{
    Q_OBJECT

    public:

    CSettingsWidget(QWidget *parent, const char *name=NULL) : CSettingsWidgetData(parent, name) { reset(); }
    virtual ~CSettingsWidget()     { }

    void reset();

    void encodingsDirButtonPressed();
    void gsFontmapButtonPressed();
    void cupsButtonPressed();
    void xDirButtonPressed();
    void xConfigButtonPressed();
    void dirButtonPressed();
    void encodingsDirChanged(const QString &);
    void gsFontmapChanged(const QString &);
    void cupsChanged(const QString &);
    void xDirChanged(const QString &);
    void xConfigChanged(const QString &);
    void dirChanged(const QString &);

    void t1SubDir(const QString &str);
    void ttSubDir(const QString &str);
    void ghostscriptChecked(bool on);
    void cupsChecked(bool on);
    void xftChecked(bool on);
    void setGhostscriptFile(const QString &f);
    void setXConfigFile(const QString &f);
    void configureSelected(bool on);
    void ppdSelected(const QString &str);
    void generateAfmsSelected(bool on);
    void xRefreshSelected(int val);
    void customXStrChanged(const QString &str);
    void afmEncodingSelected(const QString &str);
    void t1AfmSelected(bool on);
    void ttAfmSelected(bool on);

    signals:

    void madeChanges();

    private:

    void setupSubDirCombos();
    void setupPpdCombo();
    void scanEncodings();
};

#endif
