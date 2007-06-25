/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 This file is for (very) small utility functions/classes.

*/

#include "utils.h"

#include <unistd.h>

#ifndef KCMRULES

#include <kxerrorhandler.h>
#include <assert.h>
#include <kdebug.h>
#include <kshortcut.h>
#include <kkeyserver.h>

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>
#include <QX11Info>

#ifdef HAVE_XRENDER
#include <X11/extensions/Xrender.h>
#endif
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#ifdef HAVE_XCOMPOSITE
#include <X11/extensions/Xcomposite.h>
#endif
#ifdef HAVE_OPENGL
#include <GL/glx.h>
#endif
#ifdef HAVE_XSYNC
#include <X11/extensions/sync.h>
#endif

#include <stdio.h>

#include "atoms.h"
#include "notifications.h"
#include "workspace.h"

#endif

namespace KWin
{

#ifndef KCMRULES

int Extensions::shape_version = 0;
int Extensions::shape_event_base = 0;
bool Extensions::has_randr = false;
int Extensions::randr_event_base = 0;
bool Extensions::has_damage = false;
int Extensions::damage_event_base = 0;
int Extensions::composite_version = 0;
int Extensions::fixes_version = 0;
int Extensions::render_version = 0;
bool Extensions::has_glx = false;
bool Extensions::has_sync = false;
int Extensions::sync_event_base = 0;

void Extensions::init()
    {
    int dummy;
    shape_version = 0;
    if( XShapeQueryExtension( display(), &shape_event_base, &dummy ))
        {
        int major, minor;
        if( XShapeQueryVersion( display(), &major, &minor ))
            shape_version = major * 0x10 + minor;
        }
#ifdef HAVE_XRANDR
    has_randr = XRRQueryExtension( display(), &randr_event_base, &dummy );
    if( has_randr )
        {
        int major, minor;
        XRRQueryVersion( display(), &major, &minor );
        has_randr = ( major > 1 || ( major == 1 && minor >= 1 ) );
        }
#else
    has_randr = false;
#endif
#ifdef HAVE_XDAMAGE
    has_damage = XDamageQueryExtension( display(), &damage_event_base, &dummy );
#else
    has_damage = false;
#endif
    composite_version = 0;
#ifdef HAVE_XCOMPOSITE
    if( XCompositeQueryExtension( display(), &dummy, &dummy ))
        {
        int major = 0, minor = 0;
        XCompositeQueryVersion( display(), &major, &minor );
        composite_version = major * 0x10 + minor;
        }
#endif
    fixes_version = 0;
#ifdef HAVE_XFIXES
    if( XFixesQueryExtension( display(), &dummy, &dummy ))
        {
        int major = 0, minor = 0;
        XFixesQueryVersion( display(), &major, &minor );
        fixes_version = major * 0x10 + minor;
        }
#endif
    render_version = 0;
#ifdef HAVE_XRENDER
    if( XRenderQueryExtension( display(), &dummy, &dummy ))
        {
        int major = 0, minor = 0;
        XRenderQueryVersion( display(), &major, &minor );
        render_version = major * 0x10 + minor;
        }
#endif
    has_glx = false;
#ifdef HAVE_OPENGL
    has_glx = glXQueryExtension( display(), &dummy, &dummy );
#endif
#ifdef HAVE_XSYNC
    if( XSyncQueryExtension( display(), &sync_event_base, &dummy ))
        {
        int major = 0, minor = 0;
        if( XSyncInitialize( display(), &major, &minor ))
            has_sync = true;
        }
#endif
    kDebug( 1212 ) << "Extensions: shape: 0x" << QString::number( shape_version, 16 )
        << " composite: 0x" << QString::number( composite_version, 16 )
        << " render: 0x" << QString::number( render_version, 16 )
        << " fixes: 0x" << QString::number( fixes_version, 16 ) << endl;
    }

int Extensions::shapeNotifyEvent()
    {
    return shape_event_base + ShapeNotify;
    }

// does the window w need a shape combine mask around it?
bool Extensions::hasShape( Window w )
    {
    int xws, yws, xbs, ybs;
    unsigned int wws, hws, wbs, hbs;
    int boundingShaped = 0, clipShaped = 0;
    if( !shapeAvailable())
        return false;
    XShapeQueryExtents(display(), w,
                       &boundingShaped, &xws, &yws, &wws, &hws,
                       &clipShaped, &xbs, &ybs, &wbs, &hbs);
    return boundingShaped != 0;
    }

bool Extensions::shapeInputAvailable()
    {
    return shape_version >= 0x11; // 1.1
    }

int Extensions::randrNotifyEvent()
    {
#ifdef HAVE_XRANDR
    return randr_event_base + RRScreenChangeNotify;
#else
    return 0;
#endif
    }

int Extensions::damageNotifyEvent()
    {
#ifdef HAVE_XDAMAGE
    return damage_event_base + XDamageNotify;
#else
    return 0;
#endif
    }

bool Extensions::compositeOverlayAvailable()
    {
    return composite_version >= 0x03; // 0.3
    }

bool Extensions::fixesRegionAvailable()
    {
    return fixes_version >= 0x30; // 3
    }

int Extensions::syncAlarmNotifyEvent()
    {
#ifdef HAVE_XSYNC
    return sync_event_base + XSyncAlarmNotify;
#else
    return 0;
#endif
    }

void Motif::readFlags( WId w, bool& noborder, bool& resize, bool& move,
    bool& minimize, bool& maximize, bool& close )
    {
    Atom type;
    int format;
    unsigned long length, after;
    unsigned char* data;
    MwmHints* hints = 0;
    if ( XGetWindowProperty( display(), w, atoms->motif_wm_hints, 0, 5,
                             false, atoms->motif_wm_hints, &type, &format,
                             &length, &after, &data ) == Success ) 
        {
        if ( data )
            hints = (MwmHints*) data;
        }
    noborder = false;
    resize = true;
    move = true;
    minimize = true;
    maximize = true;
    close = true;
    if ( hints ) 
        {
    // To quote from Metacity 'We support those MWM hints deemed non-stupid'
        if ( hints->flags & MWM_HINTS_FUNCTIONS ) 
            {
            // if MWM_FUNC_ALL is set, other flags say what to turn _off_
            bool set_value = (( hints->functions & MWM_FUNC_ALL ) == 0 );
            resize = move = minimize = maximize = close = !set_value;
            if( hints->functions & MWM_FUNC_RESIZE )
                resize = set_value;
            if( hints->functions & MWM_FUNC_MOVE )
                move = set_value;
            if( hints->functions & MWM_FUNC_MINIMIZE )
                minimize = set_value;
            if( hints->functions & MWM_FUNC_MAXIMIZE )
                maximize = set_value;
            if( hints->functions & MWM_FUNC_CLOSE )
                close = set_value;
            }
        if ( hints->flags & MWM_HINTS_DECORATIONS ) 
            {
            if ( hints->decorations == 0 )
                noborder = true;
            }
        XFree( data );
        }
    }

//************************************
// KWinSelectionOwner
//************************************

KWinSelectionOwner::KWinSelectionOwner( int screen_P )
    : KSelectionOwner( make_selection_atom( screen_P ), screen_P )
    {
    }

Atom KWinSelectionOwner::make_selection_atom( int screen_P )
    {
    if( screen_P < 0 )
        screen_P = DefaultScreen( display());
    char tmp[ 30 ];
    sprintf( tmp, "WM_S%d", screen_P );
    return XInternAtom( display(), tmp, False );
    }

void KWinSelectionOwner::getAtoms()
    {
    KSelectionOwner::getAtoms();
    if( xa_version == None )
        {
        Atom atoms[ 1 ];
        const char* const names[] =
            { "VERSION" };
        XInternAtoms( display(), const_cast< char** >( names ), 1, False, atoms );
        xa_version = atoms[ 0 ];
        }
    }

void KWinSelectionOwner::replyTargets( Atom property_P, Window requestor_P )
    {
    KSelectionOwner::replyTargets( property_P, requestor_P );
    Atom atoms[ 1 ] = { xa_version };
    // PropModeAppend !
    XChangeProperty( display(), requestor_P, property_P, XA_ATOM, 32, PropModeAppend,
        reinterpret_cast< unsigned char* >( atoms ), 1 );
    }

bool KWinSelectionOwner::genericReply( Atom target_P, Atom property_P, Window requestor_P )
    {
    if( target_P == xa_version )
        {
        long version[] = { 2, 0 };
        XChangeProperty( display(), requestor_P, property_P, XA_INTEGER, 32,
            PropModeReplace, reinterpret_cast< unsigned char* >( &version ), 2 );
        }
    else
        return KSelectionOwner::genericReply( target_P, property_P, requestor_P );
    return true;    
    }

Atom KWinSelectionOwner::xa_version = None;


QByteArray getStringProperty(WId w, Atom prop, char separator)
    {
    Atom type;
    int format, status;
    unsigned long nitems = 0;
    unsigned long extra = 0;
    unsigned char *data = 0;
    QByteArray result = "";
    KXErrorHandler handler; // ignore errors
    status = XGetWindowProperty( display(), w, prop, 0, 10000,
                                 false, XA_STRING, &type, &format,
                                 &nitems, &extra, &data );
    if ( status == Success) 
        {
        if (data && separator) 
            {
            for (int i=0; i<(int)nitems; i++)
                if (!data[i] && i+1<(int)nitems)
                    data[i] = separator;
            }
        if (data)
            result = (const char*) data;
        XFree(data);
        }
    return result;
    }

static Time next_x_time;
static Bool update_x_time_predicate( Display*, XEvent* event, XPointer )
{
    if( next_x_time != CurrentTime )
        return False;
    // from qapplication_x11.cpp
    switch ( event->type ) {
    case ButtonPress:
	// fallthrough intended
    case ButtonRelease:
	next_x_time = event->xbutton.time;
	break;
    case MotionNotify:
	next_x_time = event->xmotion.time;
	break;
    case KeyPress:
	// fallthrough intended
    case KeyRelease:
	next_x_time = event->xkey.time;
	break;
    case PropertyNotify:
	next_x_time = event->xproperty.time;
	break;
    case EnterNotify:
    case LeaveNotify:
	next_x_time = event->xcrossing.time;
	break;
    case SelectionClear:
	next_x_time = event->xselectionclear.time;
	break;
    default:
	break;
    }
    return False;
}

/*
 Updates xTime(). This used to simply fetch current timestamp from the server,
 but that can cause xTime() to be newer than timestamp of events that are
 still in our events queue, thus e.g. making XSetInputFocus() caused by such
 event to be ignored. Therefore events queue is searched for first
 event with timestamp, and extra PropertyNotify is generated in order to make
 sure such event is found.
*/
void updateXTime()
    {
    static QWidget* w = 0;
    if ( !w )
        w = new QWidget;
    long data = 1;
    XChangeProperty(display(), w->winId(), atoms->kwin_running, atoms->kwin_running, 32,
                    PropModeAppend, (unsigned char*) &data, 1);
    next_x_time = CurrentTime;
    XEvent dummy;
    XCheckIfEvent( display(), &dummy, update_x_time_predicate, NULL );
    if( next_x_time == CurrentTime )
        {
        XSync( display(), False );
        XCheckIfEvent( display(), &dummy, update_x_time_predicate, NULL );
        }
    assert( next_x_time != CurrentTime );
    QX11Info::setAppTime( next_x_time );
    XEvent ev; // remove the PropertyNotify event from the events queue
    XWindowEvent( display(), w->winId(), PropertyChangeMask, &ev );
    }

static int server_grab_count = 0;

void grabXServer()
    {
    if( ++server_grab_count == 1 )
        XGrabServer( display());
    }

void ungrabXServer()
    {
    assert( server_grab_count > 0 );
    if( --server_grab_count == 0 )
        {
        XUngrabServer( display());
        XFlush( display());
        Notify::sendPendingEvents();
        }
    }

bool grabbedXServer()
    {
    return server_grab_count > 0;
    }

static bool keyboard_grabbed = false;

bool grabXKeyboard( Window w )
    {
    if( QWidget::keyboardGrabber() != NULL )
        return false;
    if( keyboard_grabbed )
        return false;
    if( qApp->activePopupWidget() != NULL )
        return false;
    if( w == None )
        w = rootWindow();
    if( XGrabKeyboard( display(), w, False,
        GrabModeAsync, GrabModeAsync, xTime()) != GrabSuccess )
        return false;
    keyboard_grabbed = true;
    return true;
    }

void ungrabXKeyboard()
    {
    assert( keyboard_grabbed );
    keyboard_grabbed = false;
    XUngrabKeyboard( display(), xTime());
    }

QPoint cursorPos()
    {
    return Workspace::self()->cursorPos();
    }

// converting between X11 mouse/keyboard state mask and Qt button/keyboard states

int qtToX11Button( Qt::MouseButton button )
    {
    if( button == Qt::LeftButton )
        return Button1;
    else if( button == Qt::MidButton )
        return Button2;
    else if( button == Qt::RightButton )
        return Button3;
    return AnyButton; // 0
    }

Qt::MouseButton x11ToQtMouseButton( int button )
    {
    if( button == Button1 )
        return Qt::LeftButton;
    if( button == Button2 )
        return Qt::MidButton;
    if( button == Button3 )
        return Qt::RightButton;
    return Qt::NoButton;
    }

int qtToX11State( Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers )
    {
    int ret = 0;
    if( buttons & Qt::LeftButton )
        ret |= Button1Mask;
    if( buttons & Qt::MidButton )
        ret |= Button2Mask;
    if( buttons & Qt::RightButton )
        ret |= Button3Mask;
    if( modifiers & Qt::ShiftModifier )
        ret |= ShiftMask;
    if( modifiers & Qt::ControlModifier )
        ret |= ControlMask;
    if( modifiers & Qt::AltModifier )
        ret |= KKeyServer::modXAlt();
    if( modifiers & Qt::MetaModifier )
        ret |= KKeyServer::modXMeta();
    return ret;
    }

Qt::MouseButtons x11ToQtMouseButtons( int state )
    {
    Qt::MouseButtons ret = 0;
    if( state & Button1Mask )
        ret |= Qt::LeftButton;
    if( state & Button2Mask )
        ret |= Qt::MidButton;
    if( state & Button3Mask )
        ret |= Qt::RightButton;
    return ret;
    }

Qt::KeyboardModifiers x11ToQtKeyboardModifiers( int state )
    {
    Qt::KeyboardModifiers ret = 0;
    if( state & ShiftMask )
        ret |= Qt::ShiftModifier;
    if( state & ControlMask )
        ret |= Qt::ControlModifier;
    if( state & KKeyServer::modXAlt())
        ret |= Qt::AltModifier;
    if( state & KKeyServer::modXMeta())
        ret |= Qt::MetaModifier;
    return ret;
    }

#endif

bool isLocalMachine( const QByteArray& host )
    {
#ifdef HOST_NAME_MAX
    char hostnamebuf[HOST_NAME_MAX];
#else
    char hostnamebuf[256];
#endif
    if (gethostname (hostnamebuf, sizeof hostnamebuf) >= 0) 
        {
        hostnamebuf[sizeof(hostnamebuf)-1] = 0;
        if (host == hostnamebuf)
            return true;
        if( char *dot = strchr(hostnamebuf, '.'))
            {
            *dot = '\0';
            if( host == hostnamebuf )
                return true;
            }
        }
    return false;
    }

#ifndef KCMRULES
#ifdef __GNUC__
#warning KShortcutDialog is gone
#endif //__GNUC__
#if 0
ShortcutDialog::ShortcutDialog( const KShortcut& cut )
    : KShortcutDialog( cut, false /*TODO: ???*/ )
    {
    // make it a popup, so that it has the grab
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    XChangeWindowAttributes( display(), winId(), CWOverrideRedirect, &attrs );
    setWindowFlags( Qt::Popup );
    }

void ShortcutDialog::accept()
    {
    foreach( const QKeySequence &seq, shortcut() )
        {
        if( seq.isEmpty())
            break;
        if( seq[0] == Qt::Key_Escape )
            {
            reject();
            return;
            }
        if( seq[0] == Qt::Key_Space )
            { // clear
            setShortcut( KShortcut());
            KShortcutDialog::accept();
            return;
            }
        if( (seq[0] & Qt::KeyboardModifierMask) == 0 )
            { // no shortcuts without modifiers
            KShortcut cut = shortcut();
            cut.remove( seq );
            setShortcut( cut );
            return;
            }
        }
    KShortcutDialog::accept();
    }
#endif //0
#endif //KCMRULES
} // namespace

#ifndef KCMRULES
#include "utils.moc"
#endif
