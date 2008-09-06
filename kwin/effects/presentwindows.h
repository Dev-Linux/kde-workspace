/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_PRESENTWINDOWS_H
#define KWIN_PRESENTWINDOWS_H

// Include with base class for effects.
#include <kwineffects.h>
#include <kwinglutils.h>
#include <kwinxrenderutils.h>

#include <QPixmap>

namespace KWin
{

/**
 * Expose-like effect which shows all windows on current desktop side-by-side,
 *  letting the user select active window.
 **/
class PresentWindowsEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        PresentWindowsEffect();
        virtual ~PresentWindowsEffect();

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintScreen();

        virtual void windowAdded( EffectWindow* c );
        virtual void windowClosed( EffectWindow* c );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual bool borderActivated( ElectricBorder border );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );

        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();

        enum { LayoutNatural, LayoutRegularGrid, LayoutFlexibleGrid }; // Layout modes

    public slots:
        void setActive(bool active);
        void toggleActive()  { mShowWindowsFromAllDesktops = false; setActive(!mActivated); }
        void toggleActiveAllDesktops()  { mShowWindowsFromAllDesktops = true; setActive(!mActivated); }

    protected:
        // Updates window tranformations, i.e. destination pos and scale of the window
        void rearrangeWindows();
        void prepareToRearrange();
        void calculateWindowTransformationsKompose(EffectWindowList windowlist, int screen);
        void calculateWindowTransformationsClosest(EffectWindowList windowlist, int screen);
        void calculateWindowTransformationsNatural(EffectWindowList windowlist, int screen);

        // Helper methods for layout calculation
        double windowAspectRatio(EffectWindow* c);
        int windowWidthForHeight(EffectWindow* c, int h);
        int windowHeightForWidth(EffectWindow* c, int w);

        void assignSlots( EffectWindowList windowlist, const QRect& area, int columns, int rows );
        void getBestAssignments( EffectWindowList windowlist );

        bool isOverlappingAny( EffectWindow* w, const EffectWindowList& windowlist, const QRegion& border );

        void updateFilterTexture();
        void discardFilterTexture();

        void paintWindowIcon( EffectWindow* w, WindowPaintData& data );

        void setHighlightedWindow( EffectWindow* w );
        EffectWindow* relativeWindow( EffectWindow* w, int xdiff, int ydiff, bool wrap ) const;
        EffectWindow* relativeWindowGrid( EffectWindow* w, int xdiff, int ydiff, bool wrap ) const;
        EffectWindow* findFirstWindow() const;

        // Called once the effect is activated (and wasn't activated before)
        void effectActivated();
        // Called once the effect has terminated
        void effectTerminated();

    private:
        bool mShowWindowsFromAllDesktops;

        // Whether the effect is currently activated by the user
        bool mActivated;
        // 0 = not active, 1 = fully active
        TimeLine mActiveness;
        // 0 = start of rearranging (old_area), 1 = done
        TimeLine mRearranging;
        double highlightChangeTime;

        Window mInput;
        bool hasKeyboardGrab;

        EffectWindowList mWindowsToPresent;
        struct WindowData
            {
            QRect area;
            QRect old_area; // when rearranging, otherwise unset
            double scale;
            double old_scale; // when rearranging, otherwise unset
            double highlight;
            int slot;
            int x, y; // position of the slot in the grid
            int slot_distance;
            QPixmap icon;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
            KSharedPtr< GLTexture > iconTexture;
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            XRenderPicture iconPicture;
#endif
            };
        typedef QHash<EffectWindow*, WindowData> DataHash;
        DataHash mWindowData;
        EffectWindow* mHighlightedWindow;
        // Grid and window remembering
        struct GridSize
            {
            int columns;
            int rows;
            };
        QVector<GridSize> screenGridSizes;
        QVector<int> numOfWindows;

        QString windowFilter;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        GLTexture* filterTexture;
        QRect filterTextureRect;
        QRect filterFrameRect;
#endif

        ElectricBorder borderActivate;
        ElectricBorder borderActivateAll;
        int layoutMode;
        bool drawWindowCaptions;
        bool drawWindowIcons;
        bool mTabBoxMode;
        bool tabBox;
        int accuracy;
        bool fillGaps;
    };

} // namespace

#endif
