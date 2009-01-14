/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Bernhard Loos <nhuh.put@web.de>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#include <kwinconfig.h>

#include "boxswitch_config.h"
#include "desktopgrid_config.h"
#include "diminactive_config.h"
#include "magiclamp_config.h"
#include "maketransparent_config.h"
#include "presentwindows_config.h"
#include "shadow_config.h"
#include "showfps_config.h"
#include "thumbnailaside_config.h"
#include "zoom_config.h"


#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include "coverswitch_config.h"
#include "cube_config.h"
#include "cylinder_config.h"
#include "flipswitch_config.h"
#include "invert_config.h"
#include "lookingglass_config.h"
#include "mousemark_config.h"
#include "magnifier_config.h"
#include "sharpen_config.h"
#include "snow_config.h"
#include "sphere_config.h"
#include "trackmouse_config.h"
#include "wobblywindows_config.h"
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
// xrender-only here if any
#endif
#ifdef HAVE_CAPTURY
#include "videorecord_config.h"
#endif

#include <kwineffects.h>

#include <KPluginLoader>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

#define COMMON_PLUGINS \
    registerPlugin<KWin::BoxSwitchEffectConfig>("boxswitch"); \
    registerPlugin<KWin::DesktopGridEffectConfig>("desktopgrid"); \
    registerPlugin<KWin::DimInactiveEffectConfig>("diminactive"); \
    registerPlugin<KWin::MagicLampEffectConfig>("magiclamp"); \
    registerPlugin<KWin::MakeTransparentEffectConfig>("maketransparent"); \
    registerPlugin<KWin::PresentWindowsEffectConfig>("presentwindows");   \
    registerPlugin<KWin::ShadowEffectConfig>("shadow"); \
    registerPlugin<KWin::ShowFpsEffectConfig> ("showfps"); \
    registerPlugin<KWin::ThumbnailAsideEffectConfig>("thumbnailaside"); \
    registerPlugin<KWin::ZoomEffectConfig>("zoom");

#define OPENGL_PLUGINS \
    registerPlugin<KWin::CoverSwitchEffectConfig>("coverswitch"); \
    registerPlugin<KWin::CubeEffectConfig>("cube"); \
    registerPlugin<KWin::CylinderEffectConfig>("cylinder"); \
    registerPlugin<KWin::FlipSwitchEffectConfig>("flipswitch"); \
    registerPlugin<KWin::InvertEffectConfig>("invert"); \
    registerPlugin<KWin::LookingGlassEffectConfig>("lookingglass"); \
    registerPlugin<KWin::MouseMarkEffectConfig>("mousemark"); \
    registerPlugin<KWin::MagnifierEffectConfig>("magnifier"); \
    registerPlugin<KWin::SharpenEffectConfig>("sharpen"); \
    registerPlugin<KWin::SnowEffectConfig>("snow"); \
    registerPlugin<KWin::SphereEffectConfig>("sphere"); \
    registerPlugin<KWin::TrackMouseEffectConfig>("trackmouse"); \
    registerPlugin<KWin::WobblyWindowsEffectConfig> ("wobblywindows");

#define XRENDER_PLUGINS
#define CAPTURY_PLUGINS \
    registerPlugin<KWin::VideoRecordEffectConfig> ("videorecord");

K_PLUGIN_FACTORY_DEFINITION(EffectFactory,
    COMMON_PLUGINS
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    OPENGL_PLUGINS
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    XRENDER_PLUGINS
#endif
#ifdef HAVE_CAPTURY
    CAPTURY_PLUGINS
#endif
    )
K_EXPORT_PLUGIN(EffectFactory("kwin"))

#undef COMMON_PLUGINS
#undef OPENGL_PLUGINS
#undef XRENDER_PLUGINS
#undef CAPTURY_PLUGINS
