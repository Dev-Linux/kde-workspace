/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SHOWFPS_H
#define KWIN_SHOWFPS_H

#include <kwineffects.h>

#include <qdatetime.h>

namespace KWin
{

class ShowFpsEffect
    : public Effect
    {
    public:
        ShowFpsEffect();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
    private:
        void paintGL( int fps );
        void paintXrender( int fps );
        QTime t;
        enum { NUM_PAINTS = 100 }; // remember time needed to paint this many paints
        int paints[ NUM_PAINTS ]; // time needed to paint
        int paints_pos;  // position in the queue
        enum { MAX_FPS = 200 };
        int frames[ MAX_FPS ]; // (sec*1000+msec) of the time the frame was done
        int frames_pos; // position in the queue
        float alpha;
        int x;
        int y;
    };

} // namespace

#endif
