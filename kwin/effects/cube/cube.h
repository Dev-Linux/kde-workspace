/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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

#ifndef KWIN_CUBE_H
#define KWIN_CUBE_H

#include <kwineffects.h>
#include <kwinglutils.h>
#include <QObject>
#include <QQueue>
#include <QSet>

namespace KWin
{

class CubeEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        CubeEffect();
        ~CubeEffect();
        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual bool borderActivated( ElectricBorder border );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );
        virtual void mouseChanged( const QPoint& pos, const QPoint& oldpos, Qt::MouseButtons buttons, 
            Qt::MouseButtons oldbuttons, Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers );
        virtual void desktopChanged( int old );
        virtual void tabBoxAdded( int mode );
        virtual void tabBoxUpdated();
        virtual void tabBoxClosed();

        static bool supported();
    protected slots:
        void toggle();
    protected:
        enum RotationDirection
            {
            Left,
            Right,
            Upwards,
            Downwards
            };
        enum VerticalRotationPosition
            {
            Up,
            Normal,
            Down
            };
        virtual void paintCube( int mask, QRegion region, ScreenPaintData& data );
        void paintSlideCube( int mask, QRegion region, ScreenPaintData& data );
        virtual void paintCap( float z, float zTexture );
        virtual void paintCapStep( float z, float zTexture, bool texture );
        void loadConfig( QString config );
        void rotateCube();
        void rotateToDesktop( int desktop );
        void setActive( bool active );
        bool activated;
        bool cube_painting;
        bool keyboard_grab;
        bool schedule_close;
        ElectricBorder borderActivate;
        int painting_desktop;
        Window input;
        int frontDesktop;
        float cubeOpacity;
        bool opacityDesktopOnly;
        bool displayDesktopName;
        bool reflection;
        bool rotating;
        bool verticalRotating;
        bool desktopChangedWhileRotating;
        bool paintCaps;
        TimeLine timeLine;
        TimeLine verticalTimeLine;
        TimeLine slideTimeLine;
        RotationDirection rotationDirection;
        RotationDirection verticalRotationDirection;
        VerticalRotationPosition verticalPosition;
        QQueue<RotationDirection> rotations;
        QQueue<RotationDirection> verticalRotations;
        QQueue<RotationDirection> slideRotations;
        QColor backgroundColor;
        QColor capColor;
        GLTexture* wallpaper;
        bool texturedCaps;
        GLTexture* capTexture;
        float manualAngle;
        float manualVerticalAngle;
        TimeLine::CurveShape currentShape;
        bool start;
        bool stop;
        bool reflectionPainting;
        bool slide;
        int oldDesktop;
        int rotationDuration;
        QList<EffectWindow*> windowsOnOtherScreens;
        QSet<EffectWindow*> panels;
        QSet<EffectWindow*> stickyWindows;
        int activeScreen;
        bool animateDesktopChange;
        bool bigCube;
        bool bottomCap;
        bool closeOnMouseRelease;
        float zoom;
        float zPosition;
        bool useForTabBox;
        bool invertKeys;
        bool invertMouse;
        bool tabBoxMode;
        bool dontSlidePanels;
        bool dontSlideStickyWindows;
        bool shortcutsRegistered;

        // GL lists
        bool capListCreated;
        bool recompileList;
        GLuint glList;
    };

} // namespace

#endif
