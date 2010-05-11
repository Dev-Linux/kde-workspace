/*
 *  Copyright (C) 2010 Andriy Rysin (rysin@kde.org)
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


#ifndef KEYBOARD_APPLET_H_
#define KEYBOARD_APPLET_H_

//#include <Plasma/Applet>
#include <plasma/applet.h>

#include <QtCore/QList>

#include "x11_helper.h"
#include "flags.h"

class QGraphicsSceneMouseEvent;
class QAction;
class KConfigDialog;
class QActionGroup;
class Rules;
class KeyboardConfig;

class KeyboardApplet : public Plasma::Applet
{
	Q_OBJECT

public Q_SLOTS:
	void init();
	void destroy();

private Q_SLOTS:
    void configChanged();
    void layoutChanged();

public:
	KeyboardApplet(QObject *parent, const QVariantList &args);
	virtual ~KeyboardApplet();

	void paintInterface(QPainter *p, const QStyleOptionGraphicsItem *option, const QRect &contentsRect);

protected:
	void mousePressEvent ( QGraphicsSceneMouseEvent *event );
	QList<QAction*> contextualActions();
//	void createConfigurationInterface(KConfigDialog *parent);

private Q_SLOTS:
	void actionTriggered(QAction* action);
	void keyboardConfigChanged();

private:
	void readConfig();
	const QIcon getFlag(const QString& layout);
	void updateTooltip();

//	bool drawFlag;
	Flags flags;
	XEventNotifier xEventNotifier;
	QActionGroup* actionGroup;
	Rules* rules;
	KeyboardConfig* keyboardConfig;
};

#endif /* KEYBOARD_APPLET_H_ */
