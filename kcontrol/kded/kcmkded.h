/* This file is part of the KDE project
   Copyright (C) 2002 Daniel Molkentin <molkentin@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
 */
#ifndef KCMKDED_H
#define KCMKDED_H

#include <qlistview.h>
#include <kcmodule.h>

class KListView;
class QStringList;
class QPushButton;

class KDEDConfig : public KCModule
{
Q_OBJECT
public:
	KDEDConfig(QWidget* parent, const char* name= 0L, const QStringList& foo = QStringList());
	~KDEDConfig() {};

	void       load();
	void       save();
	void       defaults();

	QString quickHelp() const;
	const KAboutData* aboutData() const;


protected slots:

	void slotReload();
	void slotStartService();
	void slotStopService();
	void slotServiceRunningToggled();
	void configureService();
	void slotEvalItem(QListViewItem *item);
	void slotItemChecked(QCheckListItem *item);
	void getServiceStatus();

private:

	KListView *_lvLoD;
	KListView *_lvStartup;
	QPushButton *_pbStart;
	QPushButton *_pbStop;
	QPushButton *_pbOptions;
};

class CheckListItem : public QObject, public QCheckListItem
{
	Q_OBJECT
public:
	CheckListItem(QListView* parent, const QString &text);
	~CheckListItem() { }
signals:
	void changed(QCheckListItem*);
protected:
	virtual void stateChange(bool);
};

#endif // KCMKDED_H

