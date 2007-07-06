/*
 * Copyright (c) 2007 Gustavo Pichorim Boiko <gustavo.boiko@kdemail.net>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __RANDRCONFIG_H__
#define __RANDRCONFIG_H__

#include <QWidget>
#include "ui_randrconfigbase.h"
#include "randr.h"

#ifdef HAS_RANDR_1_2

class RandRDisplay;
class SettingsContainer;
class CollapsibleWidget;
class QGraphicsScene;


class RandRConfig : public QWidget, public Ui::RandRConfigBase
{
	Q_OBJECT
public:
	RandRConfig(QWidget *parent, RandRDisplay *display);
	virtual ~RandRConfig();

	void load();
	void save();
	void defaults();

	void apply();
	void update();

public slots:
	void slotUpdateView();

signals:
	void changed(bool c);

private:
	RandRDisplay *m_display;
	bool m_changed;
	SettingsContainer *m_container;
	QList<CollapsibleWidget*> m_outputList;
	QGraphicsScene *m_scene;
};

#endif

#endif
