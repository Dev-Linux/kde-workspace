/* This file is part of the KDE Display Manager Configuration package
    Copyright (C) 1997 Thomas Tanghus (tanghus@earthling.net)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/  

#ifndef __KDMUSERS_H__
#define __KDMUSERS_H__

#include <qlist.h>
#include <qstring.h>
#include <qimage.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qcheckbox.h>

#include <klistbox.h>
#include <kcolorbtn.h>
#include <kurl.h>

#include <pwd.h>

#include <kcmodule.h>


class KDMUsersWidget : public KCModule
{
	Q_OBJECT

public:
	KDMUsersWidget(QWidget *parent=0, const char *name=0);

        void load();
        void save();
	void defaults();

	bool eventFilter(QObject *o, QEvent *e);

protected:
	void userButtonDragEnterEvent(QDragEnterEvent *e);
	void userButtonDropEvent(QDropEvent *e);

private slots:
        void slotUserSelected(const QString &user);
        void slotAllToNo();
        void slotAllToUsr();
        void slotUsrToAll();
        void slotNoToAll();
        void slotUserPixChanged(QString);
	void slotChanged();

private:

        KIconLoader *iconloader;
	QButtonGroup *usrGroup, *shwGroup;
        QRadioButton *rbselusr, *rballusr;
        QCheckBox    *cbusrshw, *cbusrsrt;
        KIconButton *userbutton;
        QLabel       *userlabel;
	KListBox     *alluserlb, *nouserlb, *userlb;
	bool         showallusers;
	QString      m_userPixDir;
	QString      m_defaultText;
};

#endif


