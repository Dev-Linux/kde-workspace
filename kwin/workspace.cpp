/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

//#define QT_CLEAN_NAMESPACE

#include "workspace.h"

#include <kapplication.h>
#include <kstartupinfo.h>
#include <fixx11h.h>
#include <kconfig.h>
#include <kglobal.h>
#include <qpopupmenu.h>
#include <klocale.h>
#include <qregexp.h>
#include <qpainter.h>
#include <qbitmap.h>
#include <qclipboard.h>
#include <kmenubar.h>
#include <kprocess.h>
#include <kglobalaccel.h>

#include "plugins.h"
#include "client.h"
#include "popupinfo.h"
#include "tabbox.h"
#include "atoms.h"
#include "placement.h"
#include "notifications.h"
#include "group.h"

#include <X11/extensions/shape.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/cursorfont.h>

extern Time qt_x_time;

namespace KWinInternal
{

extern int screen_number;

Workspace *Workspace::_self = 0;

// Rikkus: This class is too complex. It needs splitting further.
// It's a nightmare to understand, especially with so few comments :(

// Matthias: Feel free to ask me questions about it. Feel free to add
// comments. I dissagree that further splittings makes it easier. 2500
// lines are not too much. It's the task that is complex, not the
// code.
Workspace::Workspace( bool restore )
  : DCOPObject        ("KWinInterface"),
    QObject           (0, "workspace"),
    current_desktop   (0),
    number_of_desktops(0),
    popup_client      (0),
    desktop_widget    (0),
    active_client     (0),
    last_active_client     (0),
    most_recently_raised (0),
    movingClient(0),
    pending_take_activity ( NULL ),
    was_user_interaction (false),
    session_saving    (false),
    control_grab      (false),
    tab_grab          (false),
    mouse_emulation   (false),
    block_focus       (0),
    tab_box           (0),
    popupinfo         (0),
    popup             (0),
    advanced_popup    (0),
    desk_popup        (0),
    desk_popup_index  (0),
    keys              (0),
    root              (0),
    workspaceInit     (true),
    startup(0), electric_have_borders(false),
    electric_current_border(0),
    electric_top_border(None),
    electric_bottom_border(None),
    electric_left_border(None),
    electric_right_border(None),
    layoutOrientation(Qt::Vertical),
    layoutX(-1),
    layoutY(2),
    workarea(NULL),
    screenarea(NULL),
    set_active_client_recursion( 0 ),
    block_stacking_updates( 0 ),
    forced_global_mouse_grab( false )
    {
    _self = this;
    mgr = new PluginMgr;
    root = qt_xrootwin();
    default_colormap = DefaultColormap(qt_xdisplay(), qt_xscreen() );
    installed_colormap = default_colormap;
    session.setAutoDelete( TRUE );

    updateXTime(); // needed for proper initialization of user_time in Client ctor

    electric_time_first = qt_x_time;
    electric_time_last = qt_x_time;

    if ( restore )
      loadSessionInfo();

    loadFakeSessionInfo();

    (void) QApplication::desktop(); // trigger creation of desktop widget

    desktop_widget =
      new QWidget(
        0,
        "desktop_widget",
        Qt::WType_Desktop | Qt::WPaintUnclipped
    );

    kapp->setGlobalMouseTracking( true ); // so that this doesn't mess eventmask on root window later
    // call this before XSelectInput() on the root window
    startup = new KStartupInfo(
        KStartupInfo::DisableKWinModule | KStartupInfo::AnnounceSilenceChanges, this );

    // select windowmanager privileges
    XSelectInput(qt_xdisplay(), root,
                 KeyPressMask |
                 PropertyChangeMask |
                 ColormapChangeMask |
                 SubstructureRedirectMask |
                 SubstructureNotifyMask |
                 FocusChangeMask // for NotifyDetailNone
                 );

    Shape::init();

    // compatibility
    long data = 1;

    XChangeProperty(
      qt_xdisplay(),
      qt_xrootwin(),
      atoms->kwin_running,
      atoms->kwin_running,
      32,
      PropModeAppend,
      (unsigned char*) &data,
      1
    );

    initShortcuts();
    tab_box = new TabBox( this );
    popupinfo = new PopupInfo( );

    init();

#if (QT_VERSION-0 >= 0x030200) // XRANDR support
    connect( kapp->desktop(), SIGNAL( resized( int )), SLOT( desktopResized()));
#endif
    }


void Workspace::init()
    {
    checkElectricBorders();

    supportWindow = new QWidget;
    XLowerWindow( qt_xdisplay(), supportWindow->winId()); // see usage in layers.cpp

    XSetWindowAttributes attr;
    attr.override_redirect = 1;
    null_focus_window = XCreateWindow( qt_xdisplay(), qt_xrootwin(), -1,-1, 1, 1, 0, CopyFromParent,
        InputOnly, CopyFromParent, CWOverrideRedirect, &attr );
    XMapWindow(qt_xdisplay(), null_focus_window);

    unsigned long protocols[ 5 ] =
        {
        NET::Supported |
        NET::SupportingWMCheck |
        NET::ClientList |
        NET::ClientListStacking |
        NET::DesktopGeometry |
        NET::NumberOfDesktops |
        NET::CurrentDesktop |
        NET::ActiveWindow |
        NET::WorkArea |
        NET::CloseWindow |
        NET::DesktopNames |
        NET::KDESystemTrayWindows |
        NET::WMName |
        NET::WMVisibleName |
        NET::WMDesktop |
        NET::WMWindowType |
        NET::WMState |
        NET::WMStrut |
        NET::WMIconGeometry |
        NET::WMIcon |
        NET::WMPid |
        NET::WMMoveResize |
        NET::WMKDESystemTrayWinFor |
        NET::WMKDEFrameStrut |
        NET::WMPing
        ,
        NET::NormalMask |
        NET::DesktopMask |
        NET::DockMask |
        NET::ToolbarMask |
        NET::MenuMask |
        NET::DialogMask |
        NET::OverrideMask |
        NET::TopMenuMask |
        NET::UtilityMask |
        NET::SplashMask |
        0
        ,
        NET::Modal |
//        NET::Sticky |  // large desktops not supported (and probably never will be)
        NET::MaxVert |
        NET::MaxHoriz |
        NET::Shaded |
        NET::SkipTaskbar |
        NET::KeepAbove |
//        NET::StaysOnTop |  the same like KeepAbove
        NET::SkipPager |
        NET::Hidden |
        NET::FullScreen |
        NET::KeepBelow |
        NET::DemandsAttention |
        0
        ,
        NET::WM2UserTime |
        NET::WM2StartupId |
        NET::WM2AllowedActions |
        NET::WM2RestackWindow |
        NET::WM2MoveResizeWindow |
        NET::WM2ExtendedStrut |
        0
        ,
        NET::ActionMove |
        NET::ActionResize |
        NET::ActionMinimize |
        NET::ActionShade |
//        NET::ActionStick | // Sticky state is not supported
        NET::ActionMaxVert |
        NET::ActionMaxHoriz |
        NET::ActionFullScreen |
        NET::ActionChangeDesktop |
        NET::ActionClose |
        0
        ,
        };

    rootInfo = new RootInfo( this, qt_xdisplay(), supportWindow->winId(), "KWin",
        protocols, 5, qt_xscreen() );

    loadDesktopSettings();
    // extra NETRootInfo instance in Client mode is needed to get the values of the properties
    NETRootInfo client_info( qt_xdisplay(), NET::ActiveWindow | NET::CurrentDesktop );
    int initial_desktop;
    if( !kapp->isSessionRestored())
        initial_desktop = client_info.currentDesktop();
    else
        {
        KConfigGroupSaver saver( kapp->sessionConfig(), "Session" );
        initial_desktop = kapp->sessionConfig()->readNumEntry( "desktop", 1 );
        }
    if( !setCurrentDesktop( initial_desktop ))
        setCurrentDesktop( 1 );

    // now we know how many desktops we'll, thus, we initialise the positioning object
    initPositioning = new Placement(this);

    connect(&reconfigureTimer, SIGNAL(timeout()), this,
            SLOT(slotReconfigure()));
    connect( &updateToolWindowsTimer, SIGNAL( timeout()), this, SLOT( slotUpdateToolWindows()));

    connect(kapp, SIGNAL(appearanceChanged()), this,
            SLOT(slotReconfigure()));
    connect(kapp, SIGNAL(settingsChanged(int)), this,
            SLOT(slotSettingsChanged(int)));

    active_client = NULL;
    rootInfo->setActiveWindow( None );
    focusToNull();
    if( !kapp->isSessionRestored())
        ++block_focus; // because it will be set below

    char nm[ 100 ];
    sprintf( nm, "_KDE_TOPMENU_OWNER_S%d", DefaultScreen( qt_xdisplay()));
    Atom topmenu_atom = XInternAtom( qt_xdisplay(), nm, False );
    topmenu_selection = new KSelectionOwner( topmenu_atom );
    topmenu_watcher = new KSelectionWatcher( topmenu_atom );
    topmenu_height = 0;
    managing_topmenus = false;
    topmenu_space = NULL;
// TODO grabXServer(); - where exactly put this? topmenu selection claiming down belong must be before

        { // begin updates blocker block
        StackingUpdatesBlocker blocker( this );

        if( options->topMenuEnabled() && topmenu_selection->claim( false ))
            setupTopMenuHandling(); // this can call updateStackingOrder()
        else
            lostTopMenuSelection();

        unsigned int i, nwins;
        Window root_return, parent_return, *wins;
        XQueryTree(qt_xdisplay(), root, &root_return, &parent_return, &wins, &nwins);
        for (i = 0; i < nwins; i++) 
            {
            XWindowAttributes attr;
            XGetWindowAttributes(qt_xdisplay(), wins[i], &attr);
            if (attr.override_redirect )
                continue;
            if( topmenu_space && topmenu_space->winId() == wins[ i ] )
                continue;
            if (attr.map_state != IsUnmapped) 
                {
                if ( addSystemTrayWin( wins[i] ) )
                    continue;
                Client* c = createClient( wins[i], true );
                if ( c != NULL && root != qt_xrootwin() ) 
                    { // TODO what is this?
                // TODO may use QWidget:.create
                    XReparentWindow( qt_xdisplay(), c->frameId(), root, 0, 0 );
                    c->move(0,0);
                    }
                }
            }
        if ( wins )
            XFree((void *) wins);
    // propagate clients, will really happen at the end of the updates blocker block
        updateStackingOrder( true );

        updateClientArea();
        raiseElectricBorders();

    // NETWM spec says we have to set it to (0,0) if we don't support it
        NETPoint* viewports = new NETPoint[ number_of_desktops ];
        rootInfo->setDesktopViewport( number_of_desktops, *viewports );
        delete[] viewports;
        QRect geom = QApplication::desktop()->geometry();
        NETSize desktop_geometry;
        desktop_geometry.width = geom.width();
        desktop_geometry.height = geom.height();
    // TODO update also after gaining XRANDR support
        rootInfo->setDesktopGeometry( -1, desktop_geometry );

        } // end updates blocker block

    Client* new_active_client = NULL;
    if( !kapp->isSessionRestored())
        {
        --block_focus;
        new_active_client = findClient( WindowMatchPredicate( client_info.activeWindow()));
        }
    if( new_active_client == NULL
        && activeClient() == NULL && should_get_focus.count() == 0 ) // no client activated in manage()
        {
        if( new_active_client == NULL )
            new_active_client = topClientOnDesktop( currentDesktop());
        if( new_active_client == NULL && !desktops.isEmpty() )
            new_active_client = findDesktop( true, currentDesktop());
        }
    if( new_active_client != NULL )
        activateClient( new_active_client );
    // SELI TODO this won't work with unreasonable focus policies,
    // and maybe in rare cases also if the selected client doesn't
    // want focus
    workspaceInit = false;
// TODO ungrabXServer()
    }

Workspace::~Workspace()
    {
    blockStackingUpdates( true );
// TODO    grabXServer();
    // use stacking_order, so that kwin --replace keeps stacking order
    for( ClientList::ConstIterator it = stacking_order.begin();
         it != stacking_order.end();
         ++it )
        {
	// only release the window
        if( !(*it)->isDesktop()) // TODO ?
            storeFakeSessionInfo( *it );
        (*it)->releaseWindow( true );
        }
    delete desktop_widget;
    delete tab_box;
    delete popupinfo;
    delete popup;
    if ( root == qt_xrootwin() )
        XDeleteProperty(qt_xdisplay(), qt_xrootwin(), atoms->kwin_running);

    writeFakeSessionInfo();
    KGlobal::config()->sync();

    delete rootInfo;
    delete supportWindow;
    delete mgr;
    delete[] workarea;
    delete[] screenarea;
    delete startup;
    delete initPositioning;
    delete topmenu_watcher;
    delete topmenu_selection;
    delete topmenu_space;
    XDestroyWindow( qt_xdisplay(), null_focus_window );
// TODO    ungrabXServer();
    _self = 0;
    }

Client* Workspace::createClient( Window w, bool is_mapped )
    {
    StackingUpdatesBlocker blocker( this );
    Client* c = new Client( this );
    if( !c->manage( w, is_mapped ))
        {
        Client::deleteClient( c, Allowed );
        return NULL;
        }
    addClient( c, Allowed );
    return c;
    }

void Workspace::addClient( Client* c, allowed_t )
    {
    Group* grp = findGroup( c->window());
    if( grp != NULL )
        grp->gotLeader( c );

    if ( c->isDesktop() )
        {
        desktops.append( c );
        if( active_client == NULL && should_get_focus.isEmpty() && c->isOnCurrentDesktop())
            requestFocus( c ); // CHECKME? make sure desktop is active after startup if there's no other window active
        }
    else
        {
        if ( c->wantsTabFocus() && !focus_chain.contains( c ))
            focus_chain.append( c );
        clients.append( c );
        }
    if( !unconstrained_stacking_order.contains( c ))
        unconstrained_stacking_order.append( c );
    if( c->isTopMenu())
        addTopMenu( c );
    updateClientArea(); // this cannot be in manage(), because the client got added only now
    updateClientLayer( c );
    if( c->isDesktop())
        {
        raiseClient( c );
	// if there's no active client, make this desktop the active one
        if( activeClient() == NULL && should_get_focus.count() == 0 )
            activateClient( findDesktop( true, currentDesktop()));
        }
    if( c->isUtility() || c->isMenu() || c->isToolbar())
        updateToolWindows( true );
    checkTransients( c->window()); // SELI does this really belong here?
    updateStackingOrder( true ); // propagate new client
    }

/*
  Destroys the client \a c
 */
void Workspace::removeClient( Client* c, allowed_t )
    {
    if (c == active_client && popup)
        popup->close();
    if( c == popup_client )
        popup_client = 0;

    if( c->isDialog())
        Notify::raise( Notify::TransDelete );
    if( c->isNormalWindow())
        Notify::raise( Notify::Delete );

    storeFakeSessionInfo( c );

    Q_ASSERT( clients.contains( c ) || desktops.contains( c ));
    clients.remove( c );
    desktops.remove( c );
    unconstrained_stacking_order.remove( c );
    stacking_order.remove( c );
    focus_chain.remove( c );
    attention_chain.remove( c );
    if( c->isTopMenu())
        removeTopMenu( c );
    Group* group = findGroup( c->window());
    if( group != NULL )
        group->lostLeader();

    if ( c == most_recently_raised )
        most_recently_raised = 0;
    should_get_focus.remove( c );
    Q_ASSERT( c != active_client );
    if ( c == last_active_client )
        last_active_client = 0;
    if( c == pending_take_activity )
        pending_take_activity = NULL;

    updateStackingOrder( true );

    if (tab_grab)
       tab_box->repaint();

    updateClientArea();
    }

void Workspace::updateCurrentTopMenu()
    {
    if( !managingTopMenus())
        return;
    // toplevel menubar handling
    Client* menubar = 0;
    bool block_desktop_menubar = false;
    if( active_client )
        {
        // show the new menu bar first...
        Client* menu_client = active_client;
        for(;;)
            {
            if( menu_client->isFullScreen())
                block_desktop_menubar = true;
            for( ClientList::ConstIterator it = menu_client->transients().begin();
                 it != menu_client->transients().end();
                 ++it )
                if( (*it)->isTopMenu())
                    {
                    menubar = *it;
                    break;
                    }
            if( menubar != NULL || !menu_client->isTransient())
                break;
            if( menu_client->isModal() || menu_client->transientFor() == NULL )
                break; // don't use mainwindow's menu if this is modal or group transient
            menu_client = menu_client->transientFor();
            }
        if( !menubar )
            { // try to find any topmenu from the application (#72113)
            for( ClientList::ConstIterator it = active_client->group()->members().begin();
                 it != active_client->group()->members().end();
                 ++it )
                if( (*it)->isTopMenu())
                    {
                    menubar = *it;
                    break;
                    }
            }
        }
    if( !menubar && !block_desktop_menubar && options->desktopTopMenu())
        {
        // Find the menubar of the desktop
        Client* desktop = findDesktop( true, currentDesktop());
        if( desktop != NULL )
            {
            for( ClientList::ConstIterator it = desktop->transients().begin();
                 it != desktop->transients().end();
                 ++it )
                if( (*it)->isTopMenu())
                    {
                    menubar = *it;
                    break;
                    }
            }
        // TODO to be cleaned app with window grouping
        // Without qt-copy patch #0009, the topmenu and desktop are not in the same group,
        // thus the topmenu is not transient for it :-/.
        if( menubar == NULL )
            {
            for( ClientList::ConstIterator it = topmenus.begin();
                 it != topmenus.end();
                 ++it )
                if( (*it)->wasOriginallyGroupTransient()) // kdesktop's topmenu has WM_TRANSIENT_FOR
                    {                                     // set pointing to the root window
                    menubar = *it;                        // to recognize it here
                    break;                                // Also, with the xroot hack in kdesktop,
                    }                                     // there's no NET::Desktop window to be transient for
            }
        }

//    kdDebug() << "CURRENT TOPMENU:" << menubar << ":" << active_client << endl;
    if ( menubar )
        {
        if( active_client && !menubar->isOnDesktop( active_client->desktop()))
            menubar->setDesktop( active_client->desktop());
        menubar->hideClient( false );
        topmenu_space->hide();
        // make it appear like it's been raised manually - it's in the Dock layer anyway,
        // and not raising it could mess up stacking order of topmenus within one application,
        // and thus break raising of mainclients in raiseClient()
        unconstrained_stacking_order.remove( menubar );
        unconstrained_stacking_order.append( menubar );
        }
    else if( !block_desktop_menubar )
        { // no topmenu active - show the space window, so that there's not empty space
        topmenu_space->show();
        }

    // ... then hide the other ones. Avoids flickers.
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) 
        {
        if( (*it)->isTopMenu() && (*it) != menubar )
            (*it)->hideClient( true );
        }
    }


void Workspace::updateToolWindows( bool also_hide )
    {
    // TODO what if Client's transiency/group changes? should this be called too? (I'm paranoid, am I not?)
    const Group* group = NULL;
    const Client* client = active_client;
// Go up in transiency hiearchy, if the top is found, only tool transients for the top mainwindow
// will be shown; if a group transient is group, all tools in the group will be shown
    while( client != NULL )
        {
        if( !client->isTransient())
            break;
        if( client->groupTransient())
            {
            group = client->group();
            break;
            }
        client = client->transientFor();
        }
    // use stacking order only to reduce flicker, it doesn't matter if block_stacking_updates == 0,
    // i.e. if it's not up to date

    // SELI but maybe it should - what if a new client has been added that's not in stacking order yet?
    ClientList to_show, to_hide;
    for( ClientList::ConstIterator it = stacking_order.begin();
         it != stacking_order.end();
         ++it )
        {
        if( (*it)->isUtility() || (*it)->isMenu() || (*it)->isToolbar())
            {
            bool show = true;
            if( !(*it)->isTransient())
                {
                if( (*it)->group()->members().count() == 1 ) // has its own group, keep always visible
                    show = true;
                else if( client != NULL && (*it)->group() == client->group())
                    show = true;
                else
                    show = false;
                }
            else
                {
                if( group != NULL && (*it)->group() == group )
                    show = true;
                else if( client != NULL && client->hasTransient( (*it), true ))
                    show = true;
                else
                    show = false;
                }
            if( show )
                to_show.append( *it );
            else if( also_hide )
                to_hide.append( *it );
            }
        } // first show new ones, then hide
    for( ClientList::ConstIterator it = to_show.fromLast();
         it != to_show.end();
         --it ) // from topmost
        // TODO since this is in stacking order, the order of taskbar entries changes :(
        (*it)->hideClient( false );
    if( also_hide )
        {
        for( ClientList::ConstIterator it = to_hide.begin();
             it != to_hide.end();
             ++it ) // from bottommost
            (*it)->hideClient( true );
        updateToolWindowsTimer.stop();
        }
    else // setActiveClient() is after called with NULL client, quickly followed
        {    // by setting a new client, which would result in flickering
        updateToolWindowsTimer.start( 50, true );
        }
    }

void Workspace::slotUpdateToolWindows()
    {
    updateToolWindows( true );
    }

/*!
  Updates the current colormap according to the currently active client
 */
void Workspace::updateColormap()
    {
    Colormap cmap = default_colormap;
    if ( activeClient() && activeClient()->colormap() != None )
        cmap = activeClient()->colormap();
    if ( cmap != installed_colormap ) 
        {
        XInstallColormap(qt_xdisplay(), cmap );
        installed_colormap = cmap;
        }
    }

void Workspace::reconfigure()
    {
    reconfigureTimer.start(200, true);
    }


void Workspace::slotSettingsChanged(int category)
    {
    kdDebug(1212) << "Workspace::slotSettingsChanged()" << endl;
    if( category == (int) KApplication::SETTINGS_SHORTCUTS )
        readShortcuts();
    }

/*!
  Reread settings
 */
KWIN_PROCEDURE( CheckBorderSizesProcedure, cl->checkBorderSizes() );

void Workspace::slotReconfigure()
    {
    kdDebug(1212) << "Workspace::slotReconfigure()" << endl;
    reconfigureTimer.stop();

    KGlobal::config()->reparseConfiguration();
    unsigned long changed = options->updateSettings();
    tab_box->reconfigure();
    popupinfo->reconfigure();
    readShortcuts();
    forEachClient( CheckIgnoreFocusStealingProcedure());

    if( mgr->reset( changed ))
        { // decorations need to be recreated
#if 0 // This actually seems to make things worse now
        QWidget curtain;
        curtain.setBackgroundMode( NoBackground );
        curtain.setGeometry( QApplication::desktop()->geometry() );
        curtain.show();
#endif
        for( ClientList::ConstIterator it = clients.begin();
                it != clients.end();
                ++it )
            {
            (*it)->updateDecoration( true, true );
            }
        mgr->destroyPreviousPlugin();
        }
    else
        {
        forEachClient( CheckBorderSizesProcedure());
        }

    checkElectricBorders();

    if( options->topMenuEnabled() && !managingTopMenus())
        {
        if( topmenu_selection->claim( false ))
            setupTopMenuHandling();
        else
            lostTopMenuSelection();
        }
    else if( !options->topMenuEnabled() && managingTopMenus())
        {
        topmenu_selection->release();
        lostTopMenuSelection();
        }
    topmenu_height = 0; // invalidate used menu height
    if( managingTopMenus())
        {
        updateTopMenuGeometry();
        updateCurrentTopMenu();
        }
    }

void Workspace::loadDesktopSettings()
    {
    KConfig c("kwinrc");

    QCString groupname;
    if (screen_number == 0)
        groupname = "Desktops";
    else
        groupname.sprintf("Desktops-screen-%d", screen_number);
    c.setGroup(groupname);

    int n = c.readNumEntry("Number", 4);
    number_of_desktops = n;
    delete workarea;
    workarea = new QRect[ n + 1 ];
    delete screenarea;
    screenarea = NULL;
    rootInfo->setNumberOfDesktops( number_of_desktops );
    desktop_focus_chain.resize( n );
    for(int i = 1; i <= n; i++) 
        {
        QString s = c.readEntry(QString("Name_%1").arg(i),
                                i18n("Desktop %1").arg(i));
        rootInfo->setDesktopName( i, s.utf8().data() );
        desktop_focus_chain[i-1] = i;
        }
    }

void Workspace::saveDesktopSettings()
    {
    KConfig c("kwinrc");

    QCString groupname;
    if (screen_number == 0)
        groupname = "Desktops";
    else
        groupname.sprintf("Desktops-screen-%d", screen_number);
    c.setGroup(groupname);

    c.writeEntry("Number", number_of_desktops );
    for(int i = 1; i <= number_of_desktops; i++) 
        {
        QString s = desktopName( i );
        QString defaultvalue = i18n("Desktop %1").arg(i);
        if ( s.isEmpty() ) 
            {
            s = defaultvalue;
            rootInfo->setDesktopName( i, s.utf8().data() );
            }

        if (s != defaultvalue) 
            {
            c.writeEntry( QString("Name_%1").arg(i), s );
            }
        else 
            {
            QString currentvalue = c.readEntry(QString("Name_%1").arg(i));
            if (currentvalue != defaultvalue)
                c.writeEntry( QString("Name_%1").arg(i), "" );
            }
        }
    }

QStringList Workspace::configModules(bool controlCenter)
    {
    QStringList args;
    args <<  "kde-kwindecoration.desktop";
    if (controlCenter)
        args << "kde-kwinoptions.desktop";
    else if (kapp->authorizeControlModule("kde-kwinoptions.desktop"))
        args  << "kwinactions" << "kwinfocus" <<  "kwinmoving" << "kwinadvanced";
    return args;
    }

void Workspace::configureWM()
    {
    KApplication::kdeinitExec( "kcmshell", configModules(false) );
    }

/*!
  avoids managing a window with title \a title
 */
void Workspace::doNotManage( QString title )
    {
    doNotManageList.append( title );
    }

/*!
  Hack for java applets
 */
bool Workspace::isNotManaged( const QString& title )
    {
    for ( QStringList::Iterator it = doNotManageList.begin(); it != doNotManageList.end(); ++it ) 
        {
        QRegExp r( (*it) );
        if (r.search(title) != -1) 
            {
            doNotManageList.remove( it );
            return TRUE;
            }
        }
    return FALSE;
    }

/*!
  Refreshes all the client windows
 */
void Workspace::refresh() 
    {
    QWidget w;
    w.setGeometry( QApplication::desktop()->geometry() );
    w.show();
    w.hide();
    QApplication::flushX();
    }

/*!
  During virt. desktop switching, desktop areas covered by windows that are
  going to be hidden are first obscured by new windows with no background
  ( i.e. transparent ) placed right below the windows. These invisible windows
  are removed after the switch is complete.
  Reduces desktop ( wallpaper ) repaints during desktop switching
*/
class ObscuringWindows
    {
    public:
        ~ObscuringWindows();
        void create( Client* c );
    private:
        QValueList<Window> obscuring_windows;
        static QValueList<Window>* cached;
        static unsigned int max_cache_size;
    };

QValueList<Window>* ObscuringWindows::cached = 0;
unsigned int ObscuringWindows::max_cache_size = 0;

void ObscuringWindows::create( Client* c )
    {
    if( cached == 0 )
        cached = new QValueList<Window>;
    Window obs_win;
    XWindowChanges chngs;
    int mask = CWSibling | CWStackMode;
    if( cached->count() > 0 ) 
        {
        cached->remove( obs_win = cached->first());
        chngs.x = c->x();
        chngs.y = c->y();
        chngs.width = c->width();
        chngs.height = c->height();
        mask |= CWX | CWY | CWWidth | CWHeight;
        }
    else 
        {
        XSetWindowAttributes a;
        a.background_pixmap = None;
        a.override_redirect = True;
        obs_win = XCreateWindow( qt_xdisplay(), qt_xrootwin(), c->x(), c->y(),
            c->width(), c->height(), 0, CopyFromParent, InputOutput,
            CopyFromParent, CWBackPixmap | CWOverrideRedirect, &a );
        }
    chngs.sibling = c->frameId();
    chngs.stack_mode = Below;
    XConfigureWindow( qt_xdisplay(), obs_win, mask, &chngs );
    XMapWindow( qt_xdisplay(), obs_win );
    obscuring_windows.append( obs_win );
    }

ObscuringWindows::~ObscuringWindows()
    {
    max_cache_size = QMAX( max_cache_size, obscuring_windows.count() + 4 ) - 1;
    for( QValueList<Window>::ConstIterator it = obscuring_windows.begin();
         it != obscuring_windows.end();
         ++it ) 
        {
        XUnmapWindow( qt_xdisplay(), *it );
        if( cached->count() < max_cache_size )
            cached->prepend( *it );
        else
            XDestroyWindow( qt_xdisplay(), *it );
        }
    }


/*!
  Sets the current desktop to \a new_desktop

  Shows/Hides windows according to the stacking order and finally
  propages the new desktop to the world
 */
bool Workspace::setCurrentDesktop( int new_desktop )
    {
    if (new_desktop < 1 || new_desktop > number_of_desktops )
        return false;

    if( popup )
        popup->close();
    ++block_focus;
// TODO    Q_ASSERT( block_stacking_updates == 0 ); // make sure stacking_order is up to date
    StackingUpdatesBlocker blocker( this );

    if (new_desktop != current_desktop) 
        {
        /*
          optimized Desktop switching: unmapping done from back to front
          mapping done from front to back => less exposure events
        */
        Notify::raise((Notify::Event) (Notify::DesktopChange+new_desktop));

        ObscuringWindows obs_wins;

        int old_desktop = current_desktop;
        current_desktop = new_desktop; // change the desktop (so that Client::virtualDesktopChange() works)

        for ( ClientList::ConstIterator it = stacking_order.begin(); it != stacking_order.end(); ++it)
            if ( !(*it)->isOnDesktop( new_desktop ) && (*it) != movingClient )
                {
                if( (*it)->isShown( true ) && (*it)->isOnDesktop( old_desktop ))
                    obs_wins.create( *it );
                (*it)->virtualDesktopChange();
                }

        rootInfo->setCurrentDesktop( current_desktop ); // now propagate the change, after hiding, before showing

        if( movingClient && !movingClient->isOnDesktop( new_desktop ))
            movingClient->setDesktop( new_desktop );

        for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it)
            if ( (*it)->isOnDesktop( new_desktop ) )
                (*it)->virtualDesktopChange();
        }

    // restore the focus on this desktop
    --block_focus;
    Client* c = 0;

    if ( options->focusPolicyIsReasonable()) 
        {
        // Search in focus chain

        if ( focus_chain.contains( active_client ) && active_client->isShown( true )
            && active_client->isOnCurrentDesktop())
            {
            c = active_client; // the requestFocus below will fail, as the client is already active
            }

        if ( !c ) 
            {
            for( ClientList::ConstIterator it = focus_chain.fromLast(); it != focus_chain.end(); --it) 
                {
                if ( (*it)->isShown( false ) && !(*it)->isOnAllDesktops() && (*it)->isOnCurrentDesktop()) 
                    {
                    c = *it;
                    break;
                    }
                }
            }

        if ( !c ) 
            {
            for( ClientList::ConstIterator it = focus_chain.fromLast(); it != focus_chain.end(); --it) 
                {
                if ( (*it)->isShown( false ) && (*it)->isOnCurrentDesktop()) 
                    {
                    c = *it;
                    break;
                    }
                }
            }
        }

    //if "unreasonable focus policy"
    // and active_client is on_all_desktops and under mouse (hence == old_active_client),
    // conserve focus (thanks to Volker Schatz <V.Schatz at thphys.uni-heidelberg.de>)
    else if( active_client && active_client->isShown( true ) && active_client->isOnCurrentDesktop())
      c= active_client;

    if( c != active_client )
        setActiveClient( NULL, Allowed );

    if ( c ) 
        requestFocus( c );
    else 
        focusToNull();

    if( !desktops.isEmpty() ) 
        {
        Window w_tmp;
        int i_tmp;
        XGetInputFocus( qt_xdisplay(), &w_tmp, &i_tmp );
        if( w_tmp == null_focus_window ) // CHECKME?
            requestFocus( findDesktop( true, currentDesktop()));
        }

    // Update focus chain:
    //  If input: chain = { 1, 2, 3, 4 } and current_desktop = 3,
    //   Output: chain = { 3, 1, 2, 4 }.
//    kdDebug(1212) << QString("Switching to desktop #%1, at focus_chain index %2\n")
//      .arg(current_desktop).arg(desktop_focus_chain.find( current_desktop ));
    for( int i = desktop_focus_chain.find( current_desktop ); i > 0; i-- )
        desktop_focus_chain[i] = desktop_focus_chain[i-1];
    desktop_focus_chain[0] = current_desktop;

//    QString s = "desktop_focus_chain[] = { ";
//    for( uint i = 0; i < desktop_focus_chain.size(); i++ )
//        s += QString::number(desktop_focus_chain[i]) + ", ";
//    kdDebug(1212) << s << "}\n";
    return true;
    }

void Workspace::nextDesktop()
    {
    int desktop = currentDesktop() + 1;
    setCurrentDesktop(desktop > numberOfDesktops() ? 1 : desktop);
    popupinfo->showInfo( desktopName(currentDesktop()) );
    }

void Workspace::previousDesktop()
    {
    int desktop = currentDesktop() - 1;
    setCurrentDesktop(desktop > 0 ? desktop : numberOfDesktops());
    popupinfo->showInfo( desktopName(currentDesktop()) );
    }

/*!
  Sets the number of virtual desktops to \a n
 */
void Workspace::setNumberOfDesktops( int n )
    {
    if ( n == number_of_desktops )
        return;
    int old_number_of_desktops = number_of_desktops;
    number_of_desktops = n;

    if( currentDesktop() > numberOfDesktops())
        setCurrentDesktop( numberOfDesktops());

    // if increasing the number, do the resizing now,
    // otherwise after the moving of windows to still existing desktops
    if( old_number_of_desktops < number_of_desktops ) 
        {
        rootInfo->setNumberOfDesktops( number_of_desktops );
        NETPoint* viewports = new NETPoint[ number_of_desktops ];
        rootInfo->setDesktopViewport( number_of_desktops, *viewports );
        delete[] viewports;
        updateClientArea( true );
        }

    // if the number of desktops decreased, move all
    // windows that would be hidden to the last visible desktop
    if( old_number_of_desktops > number_of_desktops ) 
        {
        for( ClientList::ConstIterator it = clients.begin();
              it != clients.end();
              ++it) 
            {
            if( !(*it)->isOnAllDesktops() && (*it)->desktop() > numberOfDesktops())
                sendClientToDesktop( *it, numberOfDesktops(), true );
            }
        }
    if( old_number_of_desktops > number_of_desktops ) 
        {
        rootInfo->setNumberOfDesktops( number_of_desktops );
        NETPoint* viewports = new NETPoint[ number_of_desktops ];
        rootInfo->setDesktopViewport( number_of_desktops, *viewports );
        delete[] viewports;
        updateClientArea( true );
        }

    saveDesktopSettings();

    // Resize and reset the desktop focus chain.
    desktop_focus_chain.resize( n );
    for( int i = 0; i < (int)desktop_focus_chain.size(); i++ )
        desktop_focus_chain[i] = i+1;
    }

/*!
  Sends client \a c to desktop \a desk.

  Takes care of transients as well.
 */
void Workspace::sendClientToDesktop( Client* c, int desk, bool dont_activate )
    {
    if ( c->desktop() == desk )
        return;

    bool was_on_desktop = c->isOnDesktop( desk ) || c->isOnAllDesktops();
    c->setDesktop( desk );
    desk = c->desktop(); // Client did range checking

    if ( c->isOnDesktop( currentDesktop() ) )
        {
        if ( c->wantsTabFocus() && options->focusPolicyIsReasonable()
            && !was_on_desktop // for stickyness changes
            && !dont_activate )
            requestFocus( c );
        else
            restackClientUnderActive( c );
        }
    else 
        {
        raiseClient( c );
        focus_chain.remove( c );
        if ( c->wantsTabFocus() )
            focus_chain.append( c );
        }

    ClientList transients_stacking_order = ensureStackingOrder( c->transients());
    for( ClientList::ConstIterator it = transients_stacking_order.begin();
         it != transients_stacking_order.end();
         ++it )
        sendClientToDesktop( *it, desk, dont_activate );
    updateClientArea();
    }

void Workspace::setDesktopLayout(int o, int x, int y)
    {
    layoutOrientation = (Qt::Orientation) o;
    layoutX = x;
    layoutY = y;
    }

void Workspace::calcDesktopLayout(int &x, int &y)
    {
    x = layoutX;
    y = layoutY;
    if ((x == -1) && (y > 0))
       x = (numberOfDesktops()+y-1) / y;
    else if ((y == -1) && (x > 0))
       y = (numberOfDesktops()+x-1) / x;

    if (x == -1)
       x = 1;
    if (y == -1)
       y = 1;
    }

/*!
  Check whether \a w is a system tray window. If so, add it to the respective
  datastructures and propagate it to the world.
 */
bool Workspace::addSystemTrayWin( WId w )
    {
    if ( systemTrayWins.contains( w ) )
        return TRUE;

    NETWinInfo ni( qt_xdisplay(), w, root, NET::WMKDESystemTrayWinFor );
    WId trayWinFor = ni.kdeSystemTrayWinFor();
    if ( !trayWinFor )
        return FALSE;
    systemTrayWins.append( SystemTrayWindow( w, trayWinFor ) );
    XSelectInput( qt_xdisplay(), w,
                  StructureNotifyMask
                  );
    XAddToSaveSet( qt_xdisplay(), w );
    propagateSystemTrayWins();
    return TRUE;
    }

/*!
  Check whether \a w is a system tray window. If so, remove it from
  the respective datastructures and propagate this to the world.
 */
bool Workspace::removeSystemTrayWin( WId w, bool check )
    {
    if ( !systemTrayWins.contains( w ) )
        return FALSE;
    if( check )
        {
    // When getting UnmapNotify, it's not clear if it's the systray
    // reparenting the window into itself, or if it's the window
    // going away. This is obviously a flaw in the design, and we were
    // just lucky it worked for so long. Kicker's systray temporarily
    // sets _KDE_SYSTEM_TRAY_EMBEDDING property on the window while
    // embedding it, allowing KWin to figure out. Kicker just mustn't
    // crash before removing it again ... *shrug* .
        int num_props;
        Atom* props = XListProperties( qt_xdisplay(), w, &num_props );
        if( props != NULL )
            {
            for( int i = 0;
                 i < num_props;
                 ++i )
                if( props[ i ] == atoms->kde_system_tray_embedding )
                    {
                    XFree( props );
                    return false;
                    }
            XFree( props );
            }
        }
    systemTrayWins.remove( w );
    propagateSystemTrayWins();
    return TRUE;
    }


/*!
  Propagates the systemTrayWins to the world
 */
void Workspace::propagateSystemTrayWins()
    {
    Window *cl = new Window[ systemTrayWins.count()];

    int i = 0;
    for ( SystemTrayWindowList::ConstIterator it = systemTrayWins.begin(); it != systemTrayWins.end(); ++it ) 
        {
        cl[i++] =  (*it).win;
        }

    rootInfo->setKDESystemTrayWindows( cl, i );
    delete [] cl;
    }


void Workspace::killWindowId( Window window_to_kill )
    {
    if( window_to_kill == None )
        return;
    Window window = window_to_kill;
    Client* client = NULL;
    for(;;) 
        {
        client = findClient( FrameIdMatchPredicate( window ));
        if( client != NULL ) // found the client
            break;
        Window parent, root;
        Window* children;
        unsigned int children_count;
        XQueryTree( qt_xdisplay(), window, &root, &parent, &children, &children_count );
        if( children != NULL )
            XFree( children );
        if( window == root ) // we didn't find the client, probably an override-redirect window
            break;
        window = parent; // go up
        }
    if( client != NULL )
        client->killWindow();
    else
        XKillClient( qt_xdisplay(), window_to_kill );
    }


void Workspace::sendPingToWindow( Window window, Time timestamp )
    {
    rootInfo->sendPing( window, timestamp );
    }

void Workspace::sendTakeActivity( Client* c, Time timestamp, long flags )
    {
    rootInfo->takeActivity( c->window(), timestamp, flags );
    pending_take_activity = c;
    }


/*!
  Takes a screenshot of the current window and puts it in the clipboard.
*/
void Workspace::slotGrabWindow()
    {
    if ( active_client ) 
        {
        QPixmap snapshot = QPixmap::grabWindow( active_client->frameId() );

	//No XShape - no work.
        if( Shape::available()) 
            {
	    //As the first step, get the mask from XShape.
            int count, order;
            XRectangle* rects = XShapeGetRectangles( qt_xdisplay(), active_client->frameId(),
                                                     ShapeBounding, &count, &order);
	    //The ShapeBounding region is the outermost shape of the window;
	    //ShapeBounding - ShapeClipping is defined to be the border.
	    //Since the border area is part of the window, we use bounding
	    // to limit our work region
            if (rects) 
                {
		//Create a QRegion from the rectangles describing the bounding mask.
                QRegion contents;
                for (int pos = 0; pos < count; pos++)
                    contents += QRegion(rects[pos].x, rects[pos].y,
                                        rects[pos].width, rects[pos].height);
                XFree(rects);

		//Create the bounding box.
                QRegion bbox(0, 0, snapshot.width(), snapshot.height());

		//Get the masked away area.
                QRegion maskedAway = bbox - contents;
                QMemArray<QRect> maskedAwayRects = maskedAway.rects();

		//Construct a bitmap mask from the rectangles
                QBitmap mask( snapshot.width(), snapshot.height());
                QPainter p(&mask);
                p.fillRect(0, 0, mask.width(), mask.height(), Qt::color1);
                for (uint pos = 0; pos < maskedAwayRects.count(); pos++)
                    p.fillRect(maskedAwayRects[pos], Qt::color0);
                p.end();
                snapshot.setMask(mask);
                }
            }

        QClipboard *cb = QApplication::clipboard();
        cb->setPixmap( snapshot );
        }
    else
        slotGrabDesktop();
    }

/*!
  Takes a screenshot of the whole desktop and puts it in the clipboard.
*/
void Workspace::slotGrabDesktop()
    {
    QPixmap p = QPixmap::grabWindow( qt_xrootwin() );
    QClipboard *cb = QApplication::clipboard();
    cb->setPixmap( p );
    }


/*!
  Invokes keyboard mouse emulation
 */
void Workspace::slotMouseEmulation()
    {

    if ( mouse_emulation ) 
        {
        XUngrabKeyboard(qt_xdisplay(), qt_x_time);
        mouse_emulation = FALSE;
        return;
        }

    if ( XGrabKeyboard(qt_xdisplay(),
                       root, FALSE,
                       GrabModeAsync, GrabModeAsync,
                       qt_x_time) == GrabSuccess ) 
        {
        mouse_emulation = TRUE;
        mouse_emulation_state = 0;
        mouse_emulation_window = 0;
        }
    }

/*!
  Returns the child window under the mouse and activates the
  respective client if necessary.

  Auxiliary function for the mouse emulation system.
 */
WId Workspace::getMouseEmulationWindow()
    {
    Window root;
    Window child = qt_xrootwin();
    int root_x, root_y, lx, ly;
    uint state;
    Window w;
    Client * c = 0;
    do 
        {
        w = child;
        if (!c)
            c = findClient( FrameIdMatchPredicate( w ));
        XQueryPointer( qt_xdisplay(), w, &root, &child,
                       &root_x, &root_y, &lx, &ly, &state );
        } while  ( child != None && child != w );

    if ( c && !c->isActive() )
        activateClient( c );
    return (WId) w;
    }

/*!
  Sends a faked mouse event to the specified window. Returns the new button state.
 */
unsigned int Workspace::sendFakedMouseEvent( QPoint pos, WId w, MouseEmulation type, int button, unsigned int state )
    {
    if ( !w )
        return state;
    QWidget* widget = QWidget::find( w );
    if ( (!widget ||  widget->inherits("QToolButton") ) && !findClient( WindowMatchPredicate( w )) ) 
        {
        int x, y;
        Window xw;
        XTranslateCoordinates( qt_xdisplay(), qt_xrootwin(), w, pos.x(), pos.y(), &x, &y, &xw );
        if ( type == EmuMove ) 
            { // motion notify events
            XMotionEvent e;
            e.type = MotionNotify;
            e.window = w;
            e.root = qt_xrootwin();
            e.subwindow = w;
            e.time = qt_x_time;
            e.x = x;
            e.y = y;
            e.x_root = pos.x();
            e.y_root = pos.y();
            e.state = state;
            e.is_hint = NotifyNormal;
            XSendEvent( qt_xdisplay(), w, TRUE, ButtonMotionMask, (XEvent*)&e );
            }
        else 
            {
            XButtonEvent e;
            e.type = type == EmuRelease ? ButtonRelease : ButtonPress;
            e.window = w;
            e.root = qt_xrootwin();
            e.subwindow = w;
            e.time = qt_x_time;
            e.x = x;
            e.y = y;
            e.x_root = pos.x();
            e.y_root = pos.y();
            e.state = state;
            e.button = button;
            XSendEvent( qt_xdisplay(), w, TRUE, ButtonPressMask, (XEvent*)&e );

            if ( type == EmuPress ) 
                {
                switch ( button ) 
                    {
                    case 2:
                        state |= Button2Mask;
                        break;
                    case 3:
                        state |= Button3Mask;
                        break;
                    default: // 1
                        state |= Button1Mask;
                        break;
                    }
                }
            else 
                {
                switch ( button ) 
                    {
                    case 2:
                        state &= ~Button2Mask;
                        break;
                    case 3:
                        state &= ~Button3Mask;
                        break;
                    default: // 1
                        state &= ~Button1Mask;
                        break;
                    }
                }
            }
        }
    return state;
    }

/*!
  Handles keypress event during mouse emulation
 */
bool Workspace::keyPressMouseEmulation( XKeyEvent& ev )
    {
    if ( root != qt_xrootwin() )
        return FALSE;
    int kc = XKeycodeToKeysym(qt_xdisplay(), ev.keycode, 0);
    int km = ev.state & (ControlMask | Mod1Mask | ShiftMask);

    bool is_control = km & ControlMask;
    bool is_alt = km & Mod1Mask;
    bool is_shift = km & ShiftMask;
    int delta = is_control?1:is_alt?32:8;
    QPoint pos = QCursor::pos();

    switch ( kc ) 
        {
        case XK_Left:
        case XK_KP_Left:
            pos.rx() -= delta;
            break;
        case XK_Right:
        case XK_KP_Right:
            pos.rx() += delta;
            break;
        case XK_Up:
        case XK_KP_Up:
            pos.ry() -= delta;
            break;
        case XK_Down:
        case XK_KP_Down:
            pos.ry() += delta;
            break;
        case XK_F1:
            if ( !mouse_emulation_state )
                mouse_emulation_window = getMouseEmulationWindow();
            if ( (mouse_emulation_state & Button1Mask) == 0 )
                mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuPress, Button1, mouse_emulation_state );
            if ( !is_shift )
                mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button1, mouse_emulation_state );
            break;
        case XK_F2:
            if ( !mouse_emulation_state )
                mouse_emulation_window = getMouseEmulationWindow();
            if ( (mouse_emulation_state & Button2Mask) == 0 )
                mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuPress, Button2, mouse_emulation_state );
            if ( !is_shift )
                mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button2, mouse_emulation_state );
            break;
        case XK_F3:
            if ( !mouse_emulation_state )
                mouse_emulation_window = getMouseEmulationWindow();
            if ( (mouse_emulation_state & Button3Mask) == 0 )
                mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuPress, Button3, mouse_emulation_state );
            if ( !is_shift )
                mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button3, mouse_emulation_state );
            break;
        case XK_Return:
        case XK_space:
        case XK_KP_Enter:
        case XK_KP_Space: 
            {
            if ( !mouse_emulation_state ) 
                {
            // nothing was pressed, fake a LMB click
                mouse_emulation_window = getMouseEmulationWindow();
                mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuPress, Button1, mouse_emulation_state );
                mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button1, mouse_emulation_state );
                }
            else 
                { // release all
                if ( mouse_emulation_state & Button1Mask )
                    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button1, mouse_emulation_state );
                if ( mouse_emulation_state & Button2Mask )
                    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button2, mouse_emulation_state );
                if ( mouse_emulation_state & Button3Mask )
                    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button3, mouse_emulation_state );
                }
            }
    // fall through
        case XK_Escape:
            XUngrabKeyboard(qt_xdisplay(), qt_x_time);
            mouse_emulation = FALSE;
            return TRUE;
        default:
            return FALSE;
        }

    QCursor::setPos( pos );
    if ( mouse_emulation_state )
        mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuMove, 0,  mouse_emulation_state );
    return TRUE;

    }

/*!
  Returns the workspace's desktop widget. The desktop widget is
  sometimes required by clients to draw on it, for example outlines on
  moving or resizing.
 */
QWidget* Workspace::desktopWidget()
    {
    return desktop_widget;
    }



// Electric Borders
//========================================================================//
// Electric Border Window management. Electric borders allow a user
// to change the virtual desktop by moving the mouse pointer to the
// borders. Technically this is done with input only windows. Since
// electric borders can be switched on and off, we have these two
// functions to create and destroy them.
void Workspace::checkElectricBorders()
    {
    electric_current_border = 0;

    QRect r = QApplication::desktop()->geometry();
    electricTop = r.top();
    electricBottom = r.bottom();
    electricLeft = r.left();
    electricRight = r.right();

    if (options->electricBorders() == Options::ElectricAlways)
       createBorderWindows();
    else
       destroyBorderWindows();
    }

void Workspace::createBorderWindows()
    {
    if ( electric_have_borders )
        return;

    electric_have_borders = true;

    QRect r = QApplication::desktop()->geometry();
    XSetWindowAttributes attributes;
    unsigned long valuemask;
    attributes.override_redirect = True;
    attributes.event_mask =  (EnterWindowMask | LeaveWindowMask |
                              VisibilityChangeMask);
    valuemask=  (CWOverrideRedirect | CWEventMask | CWCursor );
    attributes.cursor = XCreateFontCursor(qt_xdisplay(),
                                          XC_sb_up_arrow);
    electric_top_border = XCreateWindow (qt_xdisplay(), qt_xrootwin(),
                                0,0,
                                r.width(),1,
                                0,
                                CopyFromParent, InputOnly,
                                CopyFromParent,
                                valuemask, &attributes);
    XMapWindow(qt_xdisplay(), electric_top_border);

    attributes.cursor = XCreateFontCursor(qt_xdisplay(),
                                          XC_sb_down_arrow);
    electric_bottom_border = XCreateWindow (qt_xdisplay(), qt_xrootwin(),
                                   0,r.height()-1,
                                   r.width(),1,
                                   0,
                                   CopyFromParent, InputOnly,
                                   CopyFromParent,
                                   valuemask, &attributes);
    XMapWindow(qt_xdisplay(), electric_bottom_border);

    attributes.cursor = XCreateFontCursor(qt_xdisplay(),
                                          XC_sb_left_arrow);
    electric_left_border = XCreateWindow (qt_xdisplay(), qt_xrootwin(),
                                 0,0,
                                 1,r.height(),
                                 0,
                                 CopyFromParent, InputOnly,
                                 CopyFromParent,
                                 valuemask, &attributes);
    XMapWindow(qt_xdisplay(), electric_left_border);

    attributes.cursor = XCreateFontCursor(qt_xdisplay(),
                                          XC_sb_right_arrow);
    electric_right_border = XCreateWindow (qt_xdisplay(), qt_xrootwin(),
                                  r.width()-1,0,
                                  1,r.height(),
                                  0,
                                  CopyFromParent, InputOnly,
                                  CopyFromParent,
                                  valuemask, &attributes);
    XMapWindow(qt_xdisplay(),  electric_right_border);
    }


// Electric Border Window management. Electric borders allow a user
// to change the virtual desktop by moving the mouse pointer to the
// borders. Technically this is done with input only windows. Since
// electric borders can be switched on and off, we have these two
// functions to create and destroy them.
void Workspace::destroyBorderWindows()
    {
    if( !electric_have_borders)
      return;

    electric_have_borders = false;

    if(electric_top_border)
      XDestroyWindow(qt_xdisplay(),electric_top_border);
    if(electric_bottom_border)
      XDestroyWindow(qt_xdisplay(),electric_bottom_border);
    if(electric_left_border)
      XDestroyWindow(qt_xdisplay(),electric_left_border);
    if(electric_right_border)
      XDestroyWindow(qt_xdisplay(),electric_right_border);

    electric_top_border    = None;
    electric_bottom_border = None;
    electric_left_border   = None;
    electric_right_border  = None;
    }

void Workspace::clientMoved(const QPoint &pos, Time now)
    {
    if (options->electricBorders() == Options::ElectricDisabled)
       return;

    if ((pos.x() != electricLeft) &&
        (pos.x() != electricRight) &&
        (pos.y() != electricTop) &&
        (pos.y() != electricBottom))
       return;

    Time treshold_set = options->electricBorderDelay(); // set timeout
    Time treshold_reset = 250; // reset timeout
    int distance_reset = 10; // Mouse should not move more than this many pixels

    int border = 0;
    if (pos.x() == electricLeft)
       border = 1;
    else if (pos.x() == electricRight)
       border = 2;
    else if (pos.y() == electricTop)
       border = 3;
    else if (pos.y() == electricBottom)
       border = 4;

    if ((electric_current_border == border) &&
        (timestampDiff(electric_time_last, now) < treshold_reset) &&
        ((pos-electric_push_point).manhattanLength() < distance_reset))
        {
        electric_time_last = now;

        if (timestampDiff(electric_time_first, now) > treshold_set)
            {
            electric_current_border = 0;

            QRect r = QApplication::desktop()->geometry();
            int offset;

            int desk_before = currentDesktop();
            switch(border)
                {
                case 1:
                 slotSwitchDesktopLeft();
                 if (currentDesktop() != desk_before) 
                    {
                    offset = r.width() / 5;
                    QCursor::setPos(r.width() - offset, pos.y());
                    }
                break;

               case 2:
                slotSwitchDesktopRight();
                if (currentDesktop() != desk_before) 
                    {
                    offset = r.width() / 5;
                    QCursor::setPos(offset, pos.y());
                    }
                break;

               case 3:
                slotSwitchDesktopUp();
                if (currentDesktop() != desk_before) 
                    {
                    offset = r.height() / 5;
                    QCursor::setPos(pos.x(), r.height() - offset);
                    }
                break;

               case 4:
                slotSwitchDesktopDown();
                if (currentDesktop() != desk_before) 
                    {
                    offset = r.height() / 5;
                    QCursor::setPos(pos.x(), offset);
                    }
                break;
                }
            return;
            }
        }
    else 
        {
        electric_current_border = border;
        electric_time_first = now;
        electric_time_last = now;
        electric_push_point = pos;
        }

    int mouse_warp = 1;

  // reset the pointer to find out wether the user is really pushing
    switch( border)
        {
        case 1: QCursor::setPos(pos.x()+mouse_warp, pos.y()); break;
        case 2: QCursor::setPos(pos.x()-mouse_warp, pos.y()); break;
        case 3: QCursor::setPos(pos.x(), pos.y()+mouse_warp); break;
        case 4: QCursor::setPos(pos.x(), pos.y()-mouse_warp); break;
        }
    }

// this function is called when the user entered an electric border
// with the mouse. It may switch to another virtual desktop
void Workspace::electricBorder(XEvent *e)
    {
    Time now = e->xcrossing.time;
    QPoint p(e->xcrossing.x_root, e->xcrossing.y_root);

    clientMoved(p, now);
    }

// electric borders (input only windows) have to be always on the
// top. For that reason kwm calls this function always after some
// windows have been raised.
void Workspace::raiseElectricBorders()
    {

    if(electric_have_borders)
        {
        XRaiseWindow(qt_xdisplay(), electric_top_border);
        XRaiseWindow(qt_xdisplay(), electric_left_border);
        XRaiseWindow(qt_xdisplay(), electric_bottom_border);
        XRaiseWindow(qt_xdisplay(), electric_right_border);
        }
    }

void Workspace::addTopMenu( Client* c )
    {
    assert( c->isTopMenu());
    assert( !topmenus.contains( c ));
    topmenus.append( c );
    if( managingTopMenus())
        {
        int minsize = c->minSize().height();
        if( minsize > topMenuHeight())
            {
            topmenu_height = minsize;
            updateTopMenuGeometry();
            }
        updateTopMenuGeometry( c );
        updateCurrentTopMenu();
        }
//        kdDebug() << "NEW TOPMENU:" << c << endl;
    }

void Workspace::removeTopMenu( Client* c )
    {
//    if( c->isTopMenu())
//        kdDebug() << "REMOVE TOPMENU:" << c << endl;
    assert( c->isTopMenu());
    assert( topmenus.contains( c ));
    topmenus.remove( c );
    updateCurrentTopMenu();
    // TODO reduce topMenuHeight() if possible?
    }

void Workspace::lostTopMenuSelection()
    {
//    kdDebug() << "lost TopMenu selection" << endl;
    // make sure this signal is always set when not owning the selection
    disconnect( topmenu_watcher, SIGNAL( lostOwner()), this, SLOT( lostTopMenuOwner()));
    connect( topmenu_watcher, SIGNAL( lostOwner()), this, SLOT( lostTopMenuOwner()));
    if( !managing_topmenus )
        return;
    connect( topmenu_watcher, SIGNAL( lostOwner()), this, SLOT( lostTopMenuOwner()));
    disconnect( topmenu_selection, SIGNAL( lostOwnership()), this, SLOT( lostTopMenuSelection()));
    managing_topmenus = false;
    delete topmenu_space;
    topmenu_space = NULL;
    updateClientArea();
    for( ClientList::ConstIterator it = topmenus.begin();
         it != topmenus.end();
         ++it )
        (*it)->checkWorkspacePosition();
    }

void Workspace::lostTopMenuOwner()
    {
    if( !options->topMenuEnabled())
        return;
//    kdDebug() << "TopMenu selection lost owner" << endl;
    if( !topmenu_selection->claim( false ))
        {
//        kdDebug() << "Failed to claim TopMenu selection" << endl;
        return;
        }
//    kdDebug() << "claimed TopMenu selection" << endl;
    setupTopMenuHandling();
    }

void Workspace::setupTopMenuHandling()
    {
    if( managing_topmenus )
        return;
    connect( topmenu_selection, SIGNAL( lostOwnership()), this, SLOT( lostTopMenuSelection()));
    disconnect( topmenu_watcher, SIGNAL( lostOwner()), this, SLOT( lostTopMenuOwner()));
    managing_topmenus = true;
    topmenu_space = new QWidget;
    updateTopMenuGeometry();
    topmenu_space->show();
    updateClientArea();
    updateCurrentTopMenu();
    }

int Workspace::topMenuHeight() const
    {
    if( topmenu_height == 0 )
        { // simply create a dummy menubar and use its preffered height as the menu height
        KMenuBar tmpmenu;
        tmpmenu.insertItem( "dummy" );
        topmenu_height = tmpmenu.sizeHint().height();
        }
    return topmenu_height;
    }

KDecoration* Workspace::createDecoration( KDecorationBridge* bridge )
    {
    return mgr->createDecoration( bridge );
    }

QString Workspace::desktopName( int desk ) const
    {
    return QString::fromUtf8( rootInfo->desktopName( desk ) );
    }

bool Workspace::checkStartupNotification( Window w, KStartupInfoData& data )
    {
    return startup->checkStartup( w, data ) == KStartupInfo::Match;
    }

/*!
  Puts the focus on a dummy window
  Just using XSetInputFocus() with None would block keyboard input
 */
void Workspace::focusToNull()
    {
    XSetInputFocus(qt_xdisplay(), null_focus_window, RevertToPointerRoot, qt_x_time );
    }

void Workspace::helperDialog( const QString& message, const Client* c )
    {
    QStringList args;
    QString type;
    if( message == "noborderaltf3" )
        {
        QString shortcut = QString( "%1 (%2)" ).arg( keys->label( "Window Operations Menu" ))
            .arg( keys->shortcut( "Window Operations Menu" ).seq( 0 ).toString());
        args << "--msgbox" <<
              i18n( "You have selected to show a window without its border.\n"
                    "Without the border, you won't be able to enable the border "
                    "again using the mouse. Use the window operations menu instead, "
                    "activated using the %1 keyboard shortcut." )
                .arg( shortcut );
        type = "altf3warning";
        }
    else if( message == "fullscreenaltf3" )
        {
        QString shortcut = QString( "%1 (%2)" ).arg( keys->label( "Window Operations Menu" ))
            .arg( keys->shortcut( "Window Operations Menu" ).seq( 0 ).toString());
        args << "--msgbox" <<
              i18n( "You have selected to show a window in fullscreen mode.\n"
                    "If the application itself doesn't have an option to turn the fullscreen "
                    "mode off, you won't be able to disable it "
                    "again using the mouse. Use the window operations menu instead, "
                    "activated using the %1 keyboard shortcut." )
                .arg( shortcut );
        type = "altf3warning";
        }
    else
        assert( false );
    KProcess proc;
    proc << "kdialog" << args;
    if( !type.isEmpty())
        {
        KConfig cfg( "kwin_dialogsrc" );
        cfg.setGroup( "Notification Messages" ); // this depends on KMessageBox
        if( !cfg.readBoolEntry( type, true )) // has don't show again checked
            return;                           // save launching kdialog
        proc << "--dontagain" << "kwin_dialogsrc:" + type;
        }
    if( c != NULL )
        proc << "--embed" << QString::number( c->window());
    proc.start( KProcess::DontCare );
    }

} // namespace

#include "workspace.moc"
