//----------------------------------------------------------------------------
//
// This file is part of the KDE project
//
// Copyright 1999 Martin R. Jones <mjones@kde.org>
// Copyright 2003 Lubos Lunak <l.lunak@kde.org>
//
// KDE screensaver engine
//

#include <config-workspace.h>

#include "xautolock.h"
#include "xautolock_c.h"

#include <kapplication.h>
#include <kdebug.h>

#include <QTimerEvent>
#include <QX11Info>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_DPMS
extern "C" {
#include <X11/Xmd.h>
#ifndef Bool
#define Bool BOOL
#endif
#include <X11/extensions/dpms.h>

#ifndef HAVE_DPMSINFO_PROTO
Status DPMSInfo ( Display *, CARD16 *, BOOL * );
#endif
}
#endif

#include <ctime>

xautolock_corner_t xautolock_corners[ 4 ];

static XAutoLock* self = NULL;

extern "C" {
static int catchFalseAlarms(Display *, XErrorEvent *)
{
    return 0;
}
}

//===========================================================================
//
// Detect user inactivity.
// Named XAutoLock after the program that it is based on.
//
XAutoLock::XAutoLock()
{
    self = this;
#ifdef HAVE_XSCREENSAVER
    mMitInfo = 0;
    int dummy;
    if (XScreenSaverQueryExtension( QX11Info::display(), &dummy, &dummy ))
    {
        mMitInfo = XScreenSaverAllocInfo();
    }
    else
#endif
    {
        kapp->installX11EventFilter( this );
        int (*oldHandler)(Display *, XErrorEvent *);
        oldHandler = XSetErrorHandler(catchFalseAlarms);
        XSync(QX11Info::display(), False );
        xautolock_initDiy( QX11Info::display());
        XSync(QX11Info::display(), False );
        XSetErrorHandler(oldHandler);
    }

    mTimeout = DEFAULT_TIMEOUT;
    mDPMS = true;
    resetTrigger();

    mActive = false;

    mTimerId = startTimer( CHECK_INTERVAL );
    mElapsed = 0;

}

//---------------------------------------------------------------------------
//
// Destructor.
//
XAutoLock::~XAutoLock()
{
    stop();
    self = NULL;
}

//---------------------------------------------------------------------------
//
// The time in seconds of continuous inactivity.
//
void XAutoLock::setTimeout(int t)
{
    mTimeout = t;
}

void XAutoLock::setDPMS(bool s)
{
#ifdef HAVE_DPMS
    BOOL on;
    CARD16 state;
    DPMSInfo( QX11Info::display(), &state, &on );
    if (!on)
        s = false;
#endif
    mDPMS = s;
}

//---------------------------------------------------------------------------
//
// Start watching Activity
//
void XAutoLock::start()
{
    mActive = true;
    resetTrigger();
    XSetScreenSaver(QX11Info::display(), mTimeout + 10, 100, PreferBlanking, DontAllowExposures); // We'll handle blanking
    kDebug() << "XSetScreenSaver" << mTimeout + 10;
}

//---------------------------------------------------------------------------
//
// Stop watching Activity
//
void XAutoLock::stop()
{
    mActive = false;
    resetTrigger();
    XSetScreenSaver(QX11Info::display(), 0, 100, PreferBlanking, DontAllowExposures); // No blanking at all
    kDebug() << "XSetScreenSaver 0";
}

//---------------------------------------------------------------------------
//
// Reset the trigger time.
//
void XAutoLock::resetTrigger()
{
    mLastReset = mElapsed;
    mTrigger = mElapsed + mTimeout;
#ifdef HAVE_XSCREENSAVER
    mLastIdle = 0;
#endif
    XForceScreenSaver( QX11Info::display(), ScreenSaverReset );
}

//---------------------------------------------------------------------------
//
// Move the trigger time in order to postpone (repeat) emitting of timeout().
//
void XAutoLock::postpone()
{
    mTrigger = mElapsed + 60; // delay by 60sec
}

//---------------------------------------------------------------------------
//
// Set the remaining time to 't', if it's shorter than already set.
//
void XAutoLock::setTrigger( int t )
{
    time_t newT = mElapsed + qMax(t, 0);
    if (mTrigger > newT)
        mTrigger = newT;
}

//---------------------------------------------------------------------------
//
// Process new windows and check the mouse.
//
void XAutoLock::timerEvent(QTimerEvent *ev)
{
    if (ev->timerId() != mTimerId)
    {
        return;
    }
    mElapsed += CHECK_INTERVAL / 1000;

    int (*oldHandler)(Display *, XErrorEvent *) = NULL;
#ifdef HAVE_XSCREENSAVER
    if (!mMitInfo)
#endif
    { // only the diy way needs special X handler
        XSync( QX11Info::display(), False );
        oldHandler = XSetErrorHandler(catchFalseAlarms);
    }

    xautolock_processQueue();

#ifdef HAVE_XSCREENSAVER
    if (mMitInfo)
    {
        Display *d = QX11Info::display();
        XScreenSaverQueryInfo(d, DefaultRootWindow(d), mMitInfo);
        if (mLastIdle < mMitInfo->idle)
            mLastIdle = mMitInfo->idle;
        else
            resetTrigger();
    }
#endif /* HAVE_XSCREENSAVER */

    xautolock_queryPointer( QX11Info::display());

#ifdef HAVE_XSCREENSAVER
    if (!mMitInfo)
#endif
        XSetErrorHandler(oldHandler);

    bool activate = false;

    // kDebug() << now << mTrigger;
    if (mElapsed >= mTrigger)
    {
        resetTrigger();
        activate = true;
    }

#ifdef HAVE_DPMS
    BOOL on;
    CARD16 state;
    DPMSInfo( QX11Info::display(), &state, &on );

    // kDebug() << "DPMSInfo " << state << on;
    // If DPMS is active, it makes XScreenSaverQueryInfo() report idle time
    // that is always smaller than DPMS timeout (X bug I guess). So if DPMS
    // saving is active, simply always activate our saving too, otherwise
    // this could prevent locking from working.
    if(state == DPMSModeStandby || state == DPMSModeSuspend || state == DPMSModeOff)
        activate = true;
    if(!on && mDPMS) {
        activate = false;
        resetTrigger();
    }
#endif

#ifdef HAVE_XSCREENSAVER
    static XScreenSaverInfo* mitInfo = 0;
    if (!mitInfo) mitInfo = XScreenSaverAllocInfo ();
    if (XScreenSaverQueryInfo (QX11Info::display(), QX11Info::appRootWindow(), mitInfo)) {
        // kDebug() << "XScreenSaverQueryInfo " << mitInfo->state << ScreenSaverDisabled;
        if (mitInfo->state == ScreenSaverDisabled)
            activate = false;
    }
#endif

    if(mActive && activate)
        emit timeout();
}

bool XAutoLock::x11Event( XEvent* ev )
{
    xautolock_processEvent( ev );
// don't futher process key events that were received only because XAutoLock wants them
    if( ev->type == KeyPress && !ev->xkey.send_event
#ifdef HAVE_XSCREENSAVER
        && !mMitInfo
#endif
        && !QWidget::find( ev->xkey.window ))
        return true;
    return false;
}

bool XAutoLock::ignoreWindow( WId w )
{
    if( w != QX11Info::appRootWindow() && QWidget::find( w ))
        return true;
    return false;
}

time_t XAutoLock::idleTime()
{
#ifdef HAVE_XSCREENSAVER
    if (mMitInfo)
        return mMitInfo->idle / 1000;
#endif
    return mElapsed - mLastReset;
}

extern "C"
void xautolock_resetTriggers()
{
  self->resetTrigger();
}

extern "C"
void xautolock_setTrigger( int t )
{
  self->setTrigger( t );
}

extern "C"
int xautolock_ignoreWindow( Window w )
{
   return self->ignoreWindow( w );
}

#include "xautolock.moc"
