/*
 *  Copyright (c) 2000 Kurt Granroth <granroth@kde.org>
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

#ifndef __kcmtaskbar_h__
#define __kcmtaskbar_h__

#include <kcmodule.h>
#include "kcmtaskbarui.h"

class QCheckBox;
class QButtonGroup;

class TaskbarConfig : public KCModule
{
    Q_OBJECT

public:
    TaskbarConfig( QWidget *parent=0, const char* name=0 , const QStringList &list = QStringList() );
    virtual ~TaskbarConfig();

    void load();
    void save();
    void defaults();

    QString quickHelp() const;

protected slots:
    void configChanged();
    void slotUpdateComboBox();

private:
    enum Action {
        ShowTaskList = 0,
        ShowOperationsMenu,
        ActivateRaiseOrIconify,
        Activate,
        Raise,
        Lower,
        Iconify };
    enum GroupMode {
        GroupNever = 0,
        GroupWhenFull,
        GroupAlways };

    static Action buttonAction( ButtonState button, const QString& actionName = QString::null );
    static QString buttonAction( int action );
    static const QStringList& actionList();
    static QStringList i18nActionList();
    static GroupMode groupMode( const QString& groupModeName = QString::null );
    static QString groupMode( int groupModeNum );
    static const QStringList& groupModeList();
    static QStringList i18nGroupModeList();
    TaskbarConfigUI *ui;
    bool m_moduleChanged;
};

#endif
