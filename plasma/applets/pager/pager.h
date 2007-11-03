/***************************************************************************
 *   Copyright (C) 2007 by Daniel Laidig <d.laidig@gmx.de>                 *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#ifndef PAGER_H
#define PAGER_H

#include <QLabel>
#include <QGraphicsSceneHoverEvent>
#include <QList>

#include <plasma/applet.h>
#include <plasma/dataengine.h>
#include "ui_pagerConfig.h"

class KDialog;


class Pager : public Plasma::Applet
{
    Q_OBJECT
    public:
        Pager(QObject *parent, const QVariantList &args);
        ~Pager();

        void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option,
                            const QRect &contents);
        QSizeF contentSizeHint() const;
        void constraintsUpdated(Plasma::Constraints);
	Qt::Orientations expandingDirections() const;
        virtual QList<QAction*> contextActions();

    public slots:
        void showConfigurationInterface();
	void recalculateGeometry();
	void recalculateWindowRects();
 
    protected slots:
	virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
        virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

        void configAccepted();
	void currentDesktopChanged(int desktop);
	void windowAdded(WId id);
	void windowRemoved(WId id);
	void activeWindowChanged(WId id);
	void numberOfDesktopsChanged(int num);
	void stackingOrderChanged();
	void windowChanged(WId id);
  	void showingDesktopChanged(bool showing);
        void slotConfigureDesktop();

    protected:
        void createMenu();

    private:
	QTimer* m_timer;
        KDialog *m_dialog;
        Ui::pagerConfig ui;
	bool m_showDesktopNumber;
	int m_itemHeight;
	int m_rows;
	
	int m_desktopCount;
	int m_currentDesktop;
	qreal m_scaleFactor;
	QSizeF m_size;
	QList<QRectF> m_rects;
	QRectF m_hoverRect;
	QList<QList<QPair<WId, QRect> > > m_windowRects;
	QList<QRect> m_activeWindows;
        QList<QAction*> actions;
	
	// dragging of windows
	QRect m_dragOriginal;
	QPointF m_dragOriginalPos;
	QPointF m_dragCurrentPos;
	WId m_dragId;
	int m_dragHighlightedDesktop;
};

K_EXPORT_PLASMA_APPLET(pager, Pager)

#endif
