/*
 * $Id$
 *
 * Keramik KWin client (version 0.8)
 *
 * Copyright (C) 2002 Fredrik H�glund <fredrik@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __KERAMIK_H
#define __KERAMIK_H

#include <qbutton.h>
#include <kdecoration.h>
#include <kdecorationfactory.h>

#include "tiles.h"

#include <X11/Xlib.h>

class QSpacerItem;

namespace Keramik {

	enum TilePixmap  { TitleLeft=0, TitleCenter, TitleRight,
	                   CaptionSmallLeft, CaptionSmallCenter, CaptionSmallRight,
	                   CaptionLargeLeft, CaptionLargeCenter, CaptionLargeRight,
					   GrabBarLeft, GrabBarCenter, GrabBarRight,
	                   BorderLeft, BorderRight, NumTiles };

	enum Button      { MenuButton=0, OnAllDesktopsButton, HelpButton, MinButton,
	                   MaxButton, CloseButton, NumButtons };

	enum ButtonDeco  { Menu=0, OnAllDesktops, NotOnAllDesktops, Help, Minimize, Maximize,
	                   Restore, Close, NumButtonDecos };

	struct SettingsCache
	{
		bool largeGrabBars:1;
	};

	class KeramikHandler : public QObject, public KDecorationFactory {

		Q_OBJECT

		public:
			KeramikHandler();
			~KeramikHandler();

			virtual bool reset( unsigned long changed );
                        virtual KDecoration* createDecoration( KDecorationBridge* );

			bool showAppIcons() const        { return showIcons; }
			bool useShadowedText() const     { return shadowedText; }
			bool largeCaptionBubbles() const { return !smallCaptionBubbles; }

			int titleBarHeight( bool large ) const {
				return ( large ? activeTiles[CaptionLargeCenter]->height()
						: activeTiles[CaptionSmallCenter]->height() );
			}

			int grabBarHeight() const
				{ return activeTiles[GrabBarCenter]->height(); }

			const QPixmap *roundButton() const  { return titleButtonRound; }
			const QPixmap *squareButton() const { return titleButtonSquare; }
			const QBitmap *buttonDeco( ButtonDeco deco ) const
				{ return buttonDecos[ deco ]; }

			inline const QPixmap *tile( TilePixmap tilePix, bool active ) const;

		signals:
			void softReset();

		private:
			void readConfig();
			void createPixmaps();
			void destroyPixmaps();

			void flip( QPixmap *&, QPixmap *& );
			void flip( QPixmap *& );
			void pretile( QPixmap *&, int, Qt::Orientation );
			QPixmap *composite( QImage *, QImage * );
			QImage  *loadImage( const QString &, const QColor & );
			QPixmap *loadPixmap( const QString &, const QColor & );

		private:
			bool showIcons:1, shadowedText:1,
				smallCaptionBubbles:1, largeGrabBars:1;
			SettingsCache *settings_cache;
			KeramikImageDb *imageDb;

			QPixmap *activeTiles[ NumTiles ];
			QPixmap *inactiveTiles[ NumTiles ];
			QBitmap *buttonDecos[ NumButtonDecos ];

			QPixmap *titleButtonRound, *titleButtonSquare;

	}; // class KeramikHandler

	class KeramikClient;
	class KeramikButton : public QButton
	{
		public:
			KeramikButton( KeramikClient *, const char *, Button, const QString & );
			~KeramikButton();

			int lastButton() const { return lastbutton; }

		private:
			void enterEvent( QEvent * );
			void leaveEvent( QEvent * );
			void mousePressEvent( QMouseEvent * );
			void mouseReleaseEvent( QMouseEvent * );
			void drawButton( QPainter * );

		private:
			KeramikClient *client;
			Button button;
			bool hover;
			int lastbutton;
	}; // class KeramikButton


	class KeramikClient : public KDecoration
	{
			Q_OBJECT

		public:

			KeramikClient( KDecorationBridge* bridge, KDecorationFactory* factory );
			~KeramikClient();
                        virtual void init();
			virtual MousePosition mousePosition( const QPoint& p ) const;
		    	virtual void borders( int& left, int& right, int& top, int& bottom ) const;
			virtual void resize( const QSize& s );
			virtual QSize minimumSize() const;
			virtual bool eventFilter( QObject* o, QEvent* e );
			virtual void activeChange();
			virtual void captionChange();
                        virtual void maximizeChange();
                        virtual void desktopChange();
                        virtual void shadeChange() {};

		private:
			void createLayout();
			void addButtons( QBoxLayout*, const QString & );
			void updateMask(); // FRAME
			void updateCaptionBuffer();
			void iconChange();
			void resizeEvent( QResizeEvent *); // FRAME
			void paintEvent( QPaintEvent *); // FRAME
			void mouseDoubleClickEvent( QMouseEvent * ); // FRAME
			int width() const { return widget()->width(); }
			int height() const { return widget()->height(); }

			void calculateCaptionRect();

			inline bool maximizedVertical() const {
				return ( maximizeMode() & MaximizeVertical );
			}

			inline void setRectangle( QRegion& r, int x, int y, int w, int h ) {
				r |= QRegion( x, y, w, h );
			}

		private slots:
			void menuButtonPressed();
			void slotMaximize();
			void reset();

		private:
			QSpacerItem   *topSpacer, *titlebar;
			KeramikButton *button[ NumButtons ];
			QRect          captionRect;
			QPixmap        captionBuffer;
			QPixmap       *activeIcon, *inactiveIcon;
			bool           captionBufferDirty:1, maskDirty:1;
			bool           largeCaption:1, largeTitlebar:1;
	}; // class KeramikClient

} // namespace Keramik

#endif // ___KERAMIK_H

// vim: set noet ts=4 sw=4:
