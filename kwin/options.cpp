/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "options.h"

#include <qpalette.h>
#include <qpixmap.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <qtooltip.h>

#include "client.h"

namespace KWinInternal
{

Options::Options()
    :   electric_borders( 0 ),
        electric_border_delay(0)
    {
    d = new KDecorationOptionsPrivate;
    d->defaultKWinSettings();
    updateSettings();
    }

Options::~Options()
    {
    delete d;
    }

unsigned long Options::updateSettings()
    {
    KConfig *config = KGlobal::config();
    unsigned long changed = 0;
    changed |= d->updateKWinSettings( config ); // read decoration settings

    config->setGroup( "Windows" );
    moveMode = config->readEntry("MoveMode", "Opaque" ) == "Opaque"?Opaque:Transparent;
    resizeMode = config->readEntry("ResizeMode", "Opaque" ) == "Opaque"?Opaque:Transparent;
    show_geometry_tip = config->readBoolEntry("GeometryTip", false);

    QString val;

    val = config->readEntry ("FocusPolicy", "ClickToFocus");
    focusPolicy = ClickToFocus; // what a default :-)
    if ( val == "FocusFollowsMouse" )
        focusPolicy = FocusFollowsMouse;
    else if ( val == "FocusUnderMouse" )
        focusPolicy = FocusUnderMouse;
    else if ( val == "FocusStrictlyUnderMouse" )
        focusPolicy = FocusStrictlyUnderMouse;

    val = config->readEntry ("AltTabStyle", "KDE");
    altTabStyle = KDE; // what a default :-)
    if ( val == "CDE" )
        altTabStyle = CDE;

    rollOverDesktops = config->readBoolEntry("RollOverDesktops", TRUE);
    
//    focusStealingPreventionLevel = config->readNumEntry( "FocusStealingPreventionLevel", 2 );
    // TODO use low level for now
    focusStealingPreventionLevel = config->readNumEntry( "FocusStealingPreventionLevel", 1 );
    focusStealingPreventionLevel = KMAX( 0, KMIN( 4, focusStealingPreventionLevel ));
    if( !focusPolicyIsReasonable()) // #48786, comments #7 and later
        focusStealingPreventionLevel = 0;

    KConfig *gc = new KConfig("kdeglobals", false, false);
    bool isVirtual = KApplication::desktop()->isVirtualDesktop();
    gc->setGroup("Windows");
    xineramaEnabled = gc->readBoolEntry ("XineramaEnabled", isVirtual ) &&
                      isVirtual;
    if (xineramaEnabled) 
        {
        xineramaPlacementEnabled = gc->readBoolEntry ("XineramaPlacementEnabled", true);
        xineramaMovementEnabled = gc->readBoolEntry ("XineramaMovementEnabled", true);
        xineramaMaximizeEnabled = gc->readBoolEntry ("XineramaMaximizeEnabled", true);
        }
    else 
        {
        xineramaPlacementEnabled = xineramaMovementEnabled = xineramaMaximizeEnabled = false;
        }
    delete gc;

    val = config->readEntry("Placement","Smart");
    if (val == "Random")               placement = Random;
    else if (val == "Cascade")              placement = Cascade;
    else if (val == "Centered")     placement = Centered;
    else if (val == "ZeroCornered") placement = ZeroCornered;
    else                            placement = Smart;

    animateShade = config->readBoolEntry("AnimateShade", TRUE );

    animateMinimize = config->readBoolEntry("AnimateMinimize", TRUE );
    animateMinimizeSpeed = config->readNumEntry("AnimateMinimizeSpeed", 5 );

    if( focusPolicy == ClickToFocus ) 
        {
        autoRaise = false;
        autoRaiseInterval = 0;
        }
    else 
        {
        autoRaise = config->readBoolEntry("AutoRaise", FALSE );
        autoRaiseInterval = config->readNumEntry("AutoRaiseInterval", 0 );
        }

    shadeHover = config->readBoolEntry("ShadeHover", FALSE );
    shadeHoverInterval = config->readNumEntry("ShadeHoverInterval", 250 );

    // important: autoRaise implies ClickRaise
    clickRaise = autoRaise || config->readBoolEntry("ClickRaise", TRUE );

    borderSnapZone = config->readNumEntry("BorderSnapZone", 10);
    windowSnapZone = config->readNumEntry("WindowSnapZone", 10);
    snapOnlyWhenOverlapping=config->readBoolEntry("SnapOnlyWhenOverlapping",FALSE);
    electric_borders = config->readNumEntry("ElectricBorders", 0);
    electric_border_delay = config->readNumEntry("ElectricBorderDelay", 150);

    OpTitlebarDblClick = windowOperation( config->readEntry("TitlebarDoubleClickCommand", "Shade"), true );

    ignorePositionClasses = config->readListEntry("IgnorePositionClasses");
    ignoreFocusStealingClasses = config->readListEntry("IgnoreFocusStealingClasses");
    // Qt3.2 and older had resource class all lowercase, but Qt3.3 has it capitalized
    // therefore Client::resourceClass() forces lowercase, force here lowercase as well
    for( QStringList::Iterator it = ignorePositionClasses.begin();
         it != ignorePositionClasses.end();
         ++it )
        (*it) = (*it).lower();
    for( QStringList::Iterator it = ignoreFocusStealingClasses.begin();
         it != ignoreFocusStealingClasses.end();
         ++it )
        (*it) = (*it).lower();

    // Mouse bindings
    config->setGroup( "MouseBindings");
    CmdActiveTitlebar1 = mouseCommand(config->readEntry("CommandActiveTitlebar1","Raise"), true );
    CmdActiveTitlebar2 = mouseCommand(config->readEntry("CommandActiveTitlebar2","Lower"), true );
    CmdActiveTitlebar3 = mouseCommand(config->readEntry("CommandActiveTitlebar3","Operations menu"), true );
    CmdInactiveTitlebar1 = mouseCommand(config->readEntry("CommandInactiveTitlebar1","Activate and raise"), true );
    CmdInactiveTitlebar2 = mouseCommand(config->readEntry("CommandInactiveTitlebar2","Activate and lower"), true );
    CmdInactiveTitlebar3 = mouseCommand(config->readEntry("CommandInactiveTitlebar3","Operations menu"), true );
    CmdWindow1 = mouseCommand(config->readEntry("CommandWindow1","Activate, raise and pass click"), false );
    CmdWindow2 = mouseCommand(config->readEntry("CommandWindow2","Activate and pass click"), false );
    CmdWindow3 = mouseCommand(config->readEntry("CommandWindow3","Activate and pass click"), false );
    CmdAllModKey = (config->readEntry("CommandAllKey","Alt") == "Meta") ? Qt::Key_Meta : Qt::Key_Alt;
    CmdAll1 = mouseCommand(config->readEntry("CommandAll1","Move"), false );
    CmdAll2 = mouseCommand(config->readEntry("CommandAll2","Toggle raise and lower"), false );
    CmdAll3 = mouseCommand(config->readEntry("CommandAll3","Resize"), false );

    // Read button tooltip animation effect from kdeglobals
    // Since we want to allow users to enable window decoration tooltips
    // and not kstyle tooltips and vise-versa, we don't read the
    // "EffectNoTooltip" setting from kdeglobals.
    KConfig globalConfig("kdeglobals");
    globalConfig.setGroup("KDE");
    topmenus = globalConfig.readBoolEntry( "macStyle", false );

    KConfig kdesktopcfg( "kdesktoprc", true );
    kdesktopcfg.setGroup( "Menubar" );
    desktop_topmenu = kdesktopcfg.readBoolEntry( "ShowMenubar", false );
    if( desktop_topmenu )
        topmenus = true;
        
    QToolTip::setGloballyEnabled( d->show_tooltips );

    return changed;
    }


// restricted should be true for operations that the user may not be able to repeat
// if the window is moved out of the workspace (e.g. if the user moves a window
// by the titlebar, and moves it too high beneath Kicker at the top edge, they
// may not be able to move it back, unless they know about Alt+LMB)
Options::WindowOperation Options::windowOperation(const QString &name, bool restricted )
    {
    if (name == "Move")
        return restricted ? MoveOp : UnrestrictedMoveOp;
    else if (name == "Resize")
        return restricted ? ResizeOp : UnrestrictedResizeOp;
    else if (name == "Maximize")
        return MaximizeOp;
    else if (name == "Minimize")
        return MinimizeOp;
    else if (name == "Close")
        return CloseOp;
    else if (name == "OnAllDesktops")
        return OnAllDesktopsOp;
    else if (name == "Shade")
        return ShadeOp;
    else if (name == "Operations")
        return OperationsOp;
    else if (name == "Maximize (vertical only)")
        return VMaximizeOp;
    else if (name == "Maximize (horizontal only)")
        return HMaximizeOp;
    else if (name == "Lower")
        return LowerOp;
    return NoOp;
    }

Options::MouseCommand Options::mouseCommand(const QString &name, bool restricted )
    {
    QString lowerName = name.lower();
    if (lowerName == "raise") return MouseRaise;
    if (lowerName == "lower") return MouseLower;
    if (lowerName == "operations menu") return MouseOperationsMenu;
    if (lowerName == "toggle raise and lower") return MouseToggleRaiseAndLower;
    if (lowerName == "activate and raise") return MouseActivateAndRaise;
    if (lowerName == "activate and lower") return MouseActivateAndLower;
    if (lowerName == "activate") return MouseActivate;
    if (lowerName == "activate, raise and pass click") return MouseActivateRaiseAndPassClick;
    if (lowerName == "activate and pass click") return MouseActivateAndPassClick;
    if (lowerName == "activate, raise and move")
        return restricted ? MouseActivateRaiseAndMove : MouseActivateRaiseAndUnrestrictedMove;
    if (lowerName == "move") return restricted ? MouseMove : MouseUnrestrictedMove;
    if (lowerName == "resize") return restricted ? MouseResize : MouseUnrestrictedResize;
    if (lowerName == "shade") return MouseShade;
    if (lowerName == "minimize") return MouseMinimize;
    if (lowerName == "nothing") return MouseNothing;
    return MouseNothing;
    }

bool Options::showGeometryTip()
    {
    return show_geometry_tip;
    }

int Options::electricBorders()
    {
    return electric_borders;
    }

int Options::electricBorderDelay()
    {
    return electric_border_delay;
    }

bool Options::checkIgnoreFocusStealing( const Client* c )
    {
    return ignoreFocusStealingClasses.contains(QString::fromLatin1(c->resourceClass()));
    }

} // namespace
