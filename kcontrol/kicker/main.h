/*
 *  Copyright (c) 2000 Matthias Elter <elter@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 */

#ifndef __main_h__
#define __main_h__

#include <kcmodule.h>
#include <kconfig.h>

#include "extensionInfo.h"

class QTabWidget;
class KDirWatch;
class PositionTab;
class HidingTab;
class MenuTab;
//class LookAndFeelTab;
//class AppletTab;
class ExtensionsTab;

class KickerConfig : public KCModule
{
    Q_OBJECT

public:
    KickerConfig(QWidget *parent = 0L, const char *name = 0L);
    void load();
    void save();
    void defaults();
    QString quickHelp() const;
    const KAboutData* aboutData() const;

    void populateExtensionInfoList(QListView* list);
    void reloadExtensionInfo();
    void saveExtentionInfo();
    const extensionInfoList& extensionsInfo();

    // now that it's all split up, bring the code dupe under control
    static void initScreenNumber();
    static QString configName();
    static void notifyKicker();

signals:
    void extensionInfoChanged();
    void extensionAdded(extensionInfo*);
    void extensionChanged(const QString&);
    void extensionAboutToChange(const QString&);

public slots:
    void configChanged();

protected slots:
    void positionPanelChanged(QListViewItem*);
    void hidingPanelChanged(QListViewItem*);
    void configChanged(const QString&);
    void configRemoved(const QString&);

private:
    void setupExtensionInfo(KConfig& c, bool checkExists);

    KDirWatch      *configFileWatch; 
    PositionTab    *positiontab;
    HidingTab      *hidingtab;
//    LookAndFeelTab *lookandfeeltab;
    MenuTab        *menutab;
//    AppletTab      *applettab;
    extensionInfoList m_extensionInfo;
    static int kickerconfig_screen_number;
};

#endif // __main_h__
