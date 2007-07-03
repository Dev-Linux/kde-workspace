/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "demo_wavywindows.h"

#include <math.h>


namespace KWin
{

KWIN_EFFECT( demo_wavywindows, WavyWindowsEffect )

WavyWindowsEffect::WavyWindowsEffect()
    {
    mTimeElapsed = 0.0f;
    }


void WavyWindowsEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    // Adjust elapsed time
    mTimeElapsed += (time / 1000.0f);
    // We need to mark the screen windows as transformed. Otherwise the whole
    //  screen won't be repainted, resulting in artefacts
    *mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(mask, region, time);
    }

void WavyWindowsEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    // This window will be transformed by the effect
    *mask |= PAINT_WINDOW_TRANSFORMED;
    // Check if OpenGL compositing is used
    // Request the window to be divided into cells which are at most 30x30
    //  pixels big
    w->requestVertexGrid(30);

    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void WavyWindowsEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    // Make sure we have OpenGL compositing and the window is vidible and not a
    //  special window
// TODO    if( w->isVisible() && !w->isSpecialWindow() )
    if( !w->isSpecialWindow() )
        {
        // We have OpenGL compositing and the window has been subdivided
        //  because of our request (in pre-paint pass)
        // Transform all the vertices to create wavy effect
        QVector< Vertex >& vertices = w->vertices();
        for(int i = 0; i < vertices.count(); i++)
            {
            vertices[i].pos[0] += sin(mTimeElapsed + vertices[i].texcoord[1] / 60 + 0.5f) * 10;
            vertices[i].pos[1] += sin(mTimeElapsed + vertices[i].texcoord[0] / 80) * 10;
            }
        // We have changed the vertices, so they will have to be reset before
        //  the next paint pass
        w->markVerticesDirty();
        }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

void WavyWindowsEffect::postPaintScreen()
    {
    // Repaint the workspace so that everything would be repainted next time
    effects->addRepaintFull();

    // Call the next effect.
    effects->postPaintScreen();
    }

} // namespace

