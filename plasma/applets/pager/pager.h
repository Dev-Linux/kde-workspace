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
class KSelectionOwner;
class KColorScheme;

namespace Plasma
{
    class FrameSvg;
}

class Pager : public Plasma::Applet
{
    Q_OBJECT
    public:
        Pager(QObject *parent, const QVariantList &args);
        ~Pager();
        void init();
        void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option,
                            const QRect &contents);
        void constraintsEvent(Plasma::Constraints);
        virtual QList<QAction*> contextualActions();

    public slots:
        void recalculateGeometry();
        void recalculateWindowRects();
        void themeRefresh();

    protected slots:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
        virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
        virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
        virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
        virtual void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
        virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
        virtual void wheelEvent(QGraphicsSceneWheelEvent *);

        void configAccepted();
        void currentDesktopChanged(int desktop);
        void desktopsSizeChanged();
        void windowAdded(WId id);
        void windowRemoved(WId id);
        void activeWindowChanged(WId id);
        void numberOfDesktopsChanged(int num);
        void desktopNamesChanged();
        void stackingOrderChanged();
        void windowChanged(WId id, unsigned int properties);
        void showingDesktopChanged(bool showing);
        void slotConfigureDesktop();
        void lostDesktopLayoutOwner();
        void animationUpdate(qreal progress, int animId);
        void dragSwitch();

    protected:
        void createMenu();
        KColorScheme *colorScheme();
        QRect fixViewportPosition( const QRect& r );
        void createConfigurationInterface(KConfigDialog *parent);
        bool posOnDesktopRect(const QRectF& r, const QPointF& pos);
        void handleHoverMove(const QPointF& pos);
        void handleHoverLeave();

    private:
        QTimer* m_timer;
        Ui::pagerConfig ui;
        enum DisplayedText
        {
            Number,
            Name,
            None
        };

        struct AnimInfo
        {
            int animId;
            qreal alpha;
            bool fadeIn;
            bool operator == (AnimInfo otherAnim) const { return otherAnim.animId == animId; }
        };

        DisplayedText m_displayedText;
        bool m_showWindowIcons;
        bool m_showOwnBackground;
        int m_rows;
        int m_columns;
        int m_desktopCount;
        int m_currentDesktop;
        qreal m_widthScaleFactor;
        qreal m_heightScaleFactor;
        QSizeF m_size;
        QList<QRectF> m_rects;
        //list of info about animations for each desktop
        QList<AnimInfo> m_animations;
        QRectF m_hoverRect;
        int m_hoverIndex;
        QList<QList<QPair<WId, QRect> > > m_windowRects;
        QList<QRect> m_activeWindows;
        QList<QAction*> m_actions;
        KSelectionOwner* m_desktopLayoutOwner;
        Plasma::FrameSvg *m_background;
        KColorScheme *m_colorScheme;

        // dragging of windows
        QRect m_dragOriginal;
        QPointF m_dragOriginalPos;
        QPointF m_dragCurrentPos;
        WId m_dragId;
        int m_dirtyDesktop;
        int m_dragStartDesktop;
        int m_dragHighlightedDesktop;

        // desktop switching on drop event
        int m_dragSwitchDesktop;
        QTimer* m_dragSwitchTimer;

        static const int s_FadeInDuration = 50;
        static const int s_FadeOutDuration = 100;
    };

K_EXPORT_PLASMA_APPLET(pager, Pager)

#endif
