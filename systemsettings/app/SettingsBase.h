/***************************************************************************
 *   Copyright (C) 2009 by Ben Cooksley <ben@eclipse.endoftheinternet.org> *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA          *
 ***************************************************************************/

#ifndef SETTINGS_BASE_H
#define SETTINGS_BASE_H

#include "MenuItem.h"
#include "BaseMode.h"
#include "ui_configDialog.h"

#include <QMap>
#include <QStackedWidget>

#include <KDialog>
#include <KService>
#include <KLineEdit>
#include <KActionMenu>
#include <KConfigGroup>
#include <KXmlGuiWindow>
#include <KAboutApplicationDialog>

class SettingsBase : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit SettingsBase(QWidget * parent = 0);
    ~SettingsBase();
    bool queryClose();

protected:
    virtual QSize sizeHint() const;

private:
    MenuItem * initModuleLists(MenuItem * parent);

private slots:
    void initApplication();
    void initToolBar();
    void initHelpMenu();
    void initConfig();
    void initMenuList(MenuItem * parent);
    void configUpdated();
    void configShow();
    void about();
    void changePlugin();
    void toggleDirtyState(bool state);
    void updateViewActions();
    void moduleChanged();
    void changeToolBar( BaseMode::ToolBarItems toolbar );
    void changeAboutMenu( const KAboutData * menuAbout, KAction * menuItem, QString fallback );

private:
    // The plugins
    QMap<QString, BaseMode *> possibleViews;
    BaseMode * activeView;
    // The search bar
    KLineEdit * searchText;
    QWidget * spacerWidget;
    // The toolbar
    KAction * searchAction;
    KAction * spacerAction;
    KAction * configureAction;
    // The help menu
    KAction * aboutViewAction;
    KAction * aboutModuleAction;
    KActionMenu * helpActionMenu;
    // The configuration
    KDialog * configDialog;
    Ui::ConfigDialog configWidget;
    KConfigGroup mainConfigGroup;
    // The control module
    QStackedWidget * stackedWidget;
    // The module list
    MenuItem * rootModule;
    KService::List categories;
    KService::List modules;
    // The about dialog
    KAboutApplicationDialog * aboutDialog;
    // Show enhanced tooltips
    bool showTooltips;

};
#endif
