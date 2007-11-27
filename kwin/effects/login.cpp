/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

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

#include "login.h"

#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( login, LoginEffect )

LoginEffect::LoginEffect()
    : progress( 1 )
    , login_window( NULL )
    {
    }

void LoginEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( login_window != NULL )
        {
        if( progress != 1 )
            {
            progress = qBound( 0., progress + time / 2000., 1. );
            if( progress == 1 )
                {
                login_window->unrefWindow();
                login_window = NULL;
                effects->prePaintScreen( data, time );
                return;
                }
            }
        }
    effects->prePaintScreen( data, time );
    }

void LoginEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( progress != 1 && w == login_window )
        {
        w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DELETE );
        data.setTranslucent();
        }
    effects->prePaintWindow( w, data, time );
    }

void LoginEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( w == login_window && progress != 1 )
        data.opacity *= ( 1 - progress );
    effects->paintWindow( w, mask, region, data );
    }

void LoginEffect::postPaintScreen()
    {
    if( login_window != NULL && progress != 1 )
        effects->addRepaintFull();
    effects->postPaintScreen();
    }

void LoginEffect::windowAdded( EffectWindow* w )
    {
    if( isLoginSplash( w ))
        {
        login_window = w;
        progress = 1;
        effects->addRepaintFull();
        }
    }

void LoginEffect::windowClosed( EffectWindow* w )
    {
    if( w == login_window )
        {
        login_window->refWindow();
        progress = 0;
        effects->addRepaintFull();
        }
    }

bool LoginEffect::isLoginSplash( EffectWindow* w )
    { // TODO there should be probably a better way (window type?)
    // see also fade effect
    if( w->windowClass() == "ksplashx ksplashx"
        || w->windowClass() == "ksplashsimple ksplashsimple" )
        {
        return true;
        }
    return false;
    }

} // namespace
