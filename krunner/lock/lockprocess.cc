//===========================================================================
//
// This file is part of the KDE project
//
// Copyright (c) 1999 Martin R. Jones <mjones@kde.org>
// Copyright (c) 2003 Oswald Buddenhagen <ossi@kde.org>
//

//kdesktop keeps running and checks user inactivity
//when it should show screensaver (and maybe lock the session),
//it starts kdesktop_lock, who does all the locking and who
//actually starts the screensaver

//It's done this way to prevent screen unlocking when kdesktop
//crashes (e.g. because it's set to multiple wallpapers and
//some image will be corrupted).


#include <config.h>

#include "lockprocess.h"
#include "lockdlg.h"

#include <kstandarddirs.h>
#include <kapplication.h>
#include <kservicegroup.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kglobalsettings.h>
#include <klocale.h>
#include <klibloader.h>
#include <kpushbutton.h>
#include <kstdguiitem.h>

#include <qframe.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qcursor.h>
#include <qtimer.h>
#include <qfile.h>
#include <qsocketnotifier.h>
#include <qvaluevector.h>

#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#ifdef HAVE_SETPRIORITY
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

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

#ifdef HAVE_XF86MISC
#include <X11/extensions/xf86misc.h>
#endif

#define LOCK_GRACE_DEFAULT          5000

static Window gVRoot = 0;
static Window gVRootData = 0;
static Atom   gXA_VROOT;
static Atom   gXA_SCREENSAVER_VERSION;

//===========================================================================
//
// Screen saver handling process.  Handles screensaver window,
// starting screensaver hacks, and password entry.
//
LockProcess::LockProcess(bool child, bool useBlankOnly)
    : QWidget(0L, "saver window", WX11BypassWM),
      child_saver(child),
      mParent(0),
      mUseBlankOnly(useBlankOnly),
      mSuspended(false),
      mVisibility(false),
      mRestoreXF86Lock(false),
      mForbidden(false)
{
    setupSignals();

    kapp->installX11EventFilter(this);

    // Get root window size
    XWindowAttributes rootAttr;
    XGetWindowAttributes(qt_xdisplay(), RootWindow(qt_xdisplay(),
                        qt_xscreen()), &rootAttr);
    mRootWidth = rootAttr.width;
    mRootHeight = rootAttr.height;
    XSelectInput( qt_xdisplay(), qt_xrootwin(),
        SubstructureNotifyMask | rootAttr.your_event_mask );

    // Add non-KDE path
    KGlobal::dirs()->addResourceType("scrsav",
                                    KGlobal::dirs()->kde_default("apps") +
                                    "System/ScreenSavers/");

    // Add KDE specific screensaver path
    QString relPath="System/ScreenSavers/";
    KServiceGroup::Ptr servGroup = KServiceGroup::baseGroup( "screensavers");
    if (servGroup)
    {
      relPath=servGroup->relPath();
      kdDebug(1204) << "relPath=" << relPath << endl;
    }
    KGlobal::dirs()->addResourceType("scrsav",
                                     KGlobal::dirs()->kde_default("apps") +
                                     relPath);

    // virtual root property
    gXA_VROOT = XInternAtom (qt_xdisplay(), "__SWM_VROOT", False);
    gXA_SCREENSAVER_VERSION = XInternAtom (qt_xdisplay(), "_SCREENSAVER_VERSION", False);

    XWindowAttributes attrs;
    XGetWindowAttributes(qt_xdisplay(), winId(), &attrs);
    mColorMap = attrs.colormap;

    connect(&mHackProc, SIGNAL(processExited(KProcess *)),
                        SLOT(hackExited(KProcess *)));

    connect(&mSuspendTimer, SIGNAL(timeout()), SLOT(suspend()));

    QStringList dmopt =
        QStringList::split(QChar(','),
                            QString::fromLatin1( ::getenv( "XDM_MANAGED" )));
    if (dmopt.findIndex( "rsvd" ) >= 0)
        mXdmFifoName = dmopt.first();
    for (QStringList::ConstIterator it = dmopt.begin(); it != dmopt.end(); ++it)
        if ((*it).startsWith("method="))
            mMethod = (*it).mid(7);

    configure();

#ifdef HAVE_DPMS
    if (mDPMSDepend) {
        BOOL on;
        CARD16 state;
        DPMSInfo(qt_xdisplay(), &state, &on);
        if (on)
        {
            connect(&mCheckDPMS, SIGNAL(timeout()), SLOT(checkDPMSActive()));
            mCheckDPMS.start(60000);
        }
    }
#endif

    greetPlugin.library = 0;
}

//---------------------------------------------------------------------------
//
// Destructor - usual cleanups.
//
LockProcess::~LockProcess()
{
    if (greetPlugin.library) {
        if (greetPlugin.info->done)
            greetPlugin.info->done();
        greetPlugin.library->unload();
    }
}

static int sigterm_pipe[2];

static void sigterm_handler(int)
{
    char tmp;
    ::write( sigterm_pipe[1], &tmp, 1);
}

void LockProcess::setupSignals()
{
    // ignore SIGHUP
    struct sigaction act;
    act.sa_handler=SIG_IGN;
    sigemptyset(&(act.sa_mask));
    sigaddset(&(act.sa_mask), SIGHUP);
    act.sa_flags = 0;
    sigaction(SIGHUP, &act, 0L);
    // ignore SIGINT
    act.sa_handler=SIG_IGN;
    sigemptyset(&(act.sa_mask));
    sigaddset(&(act.sa_mask), SIGINT);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, 0L);
    // ignore SIGQUIT
    act.sa_handler=SIG_IGN;
    sigemptyset(&(act.sa_mask));
    sigaddset(&(act.sa_mask), SIGQUIT);
    act.sa_flags = 0;
    sigaction(SIGQUIT, &act, 0L);
    // exit cleanly on SIGTERM
    act.sa_handler= sigterm_handler;
    sigemptyset(&(act.sa_mask));
    sigaddset(&(act.sa_mask), SIGTERM);
    act.sa_flags = 0;
    sigaction(SIGTERM, &act, 0L);

    pipe(sigterm_pipe);
    QSocketNotifier* notif = new QSocketNotifier(sigterm_pipe[0],
	QSocketNotifier::Read, this );
    connect( notif, SIGNAL(activated(int)), SLOT(sigtermPipeSignal()));
}


void LockProcess::sigtermPipeSignal()
{
    quitSaver();
}

//---------------------------------------------------------------------------
bool LockProcess::lock()
{
    if (startSaver()) {
        if (startLock())
            return true;
        stopSaver();
    }
    return false;
}

//---------------------------------------------------------------------------
bool LockProcess::defaultSave()
{
    mLocked = false;
    if (startSaver()) {
        if (mLockGrace >= 0)
            QTimer::singleShot(mLockGrace, this, SLOT(startLock()));
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
bool LockProcess::dontLock()
{
    mLocked = false;
    return startSaver();
}

//---------------------------------------------------------------------------
void LockProcess::quitSaver()
{
    stopSaver();
    kapp->quit();
}

//---------------------------------------------------------------------------
//
// Read and apply configuration.
//
void LockProcess::configure()
{
    // the configuration is stored in kdesktop's config file
    KConfig config( "kdesktoprc", true );

    config.setGroup("ScreenSaver");

    if(config.readBoolEntry("Lock", false))
    {
        mLockGrace = config.readNumEntry("LockGrace", LOCK_GRACE_DEFAULT);
        if (mLockGrace < 0)
            mLockGrace = 0;
        else if (mLockGrace > 300000)
            mLockGrace = 300000; // 5 minutes, keep the value sane
    }
    else
        mLockGrace = -1;

#ifdef HAVE_DPMS
    //if the user  decided that the screensaver should run independent from
    //dpms, we shouldn't check for it, aleXXX
    mDPMSDepend = config.readBoolEntry("DPMS-dependent", false);
#endif

    mPriority = config.readNumEntry("Priority", 19);
    if (mPriority < 0) mPriority = 0;
    if (mPriority > 19) mPriority = 19;

    mSaver = config.readEntry("Saver");
    if (mSaver.isEmpty() || mUseBlankOnly)
        mSaver = "KBlankscreen.desktop";

    readSaver();

    mPlugins = config.readListEntry("PluginsUnlock");
    if (mPlugins.isEmpty())
        mPlugins = QStringList("classic");
    mPluginOptions = config.readListEntry("PluginOptions");
}

//---------------------------------------------------------------------------
//
// Read the command line needed to run the screensaver given a .desktop file.
//
void LockProcess::readSaver()
{
    if (!mSaver.isEmpty())
    {
        QString file = locate("scrsav", mSaver);

	bool opengl = kapp->authorize("opengl_screensavers");
	bool manipulatescreen = kapp->authorize("manipulatescreen_screensavers");
        KDesktopFile config(file, true);
	if (config.readEntry("X-KDE-Type").utf8())
	{
		QString saverType = config.readEntry("X-KDE-Type").utf8();
		QStringList saverTypes = QStringList::split(";", saverType);
		for (uint i = 0; i < saverTypes.count(); i++)
		{
			if ((saverTypes[i] == "ManipulateScreen") && !manipulatescreen)
			{
				kdDebug(1204) << "Screensaver is type ManipulateScreen and ManipulateScreen is forbidden" << endl;
				mForbidden = true;
			}
			if ((saverTypes[i] == "OpenGL") && !opengl)
			{
				kdDebug(1204) << "Screensaver is type OpenGL and OpenGL is forbidden" << endl;
				mForbidden = true;
			}
		}
	}

	kdDebug(1204) << "mForbidden: " << (mForbidden ? "true" : "false") << endl;

        if (config.hasActionGroup("Root"))
        {
            config.setActionGroup("Root");
            mSaverExec = config.readPathEntry("Exec");
        }
    }
}

//---------------------------------------------------------------------------
//
// Create a window to draw our screen saver on.
//
void LockProcess::createSaverWindow()
{
    // We only create the window once, but we reset its attributes every
    // time.

    // Some xscreensaver hacks check for this property
    const char *version = "KDE 2.0";
    XChangeProperty (qt_xdisplay(), winId(),
                     gXA_SCREENSAVER_VERSION, XA_STRING, 8, PropModeReplace,
                     (unsigned char *) version, strlen(version));

    XSetWindowAttributes attr;
    if (mColorMap != None)
    {
        attr.colormap = mColorMap;
    }
    else
    {
        attr.colormap = DefaultColormapOfScreen(
                                ScreenOfDisplay(qt_xdisplay(), qt_xscreen()));
    }
    attr.event_mask = KeyPressMask | ButtonPressMask | PointerMotionMask |
                        VisibilityChangeMask | ExposureMask;
    XChangeWindowAttributes(qt_xdisplay(), winId(),
                            CWEventMask | CWColormap, &attr);

    // erase();

    // set NoBackground so that the saver can capture the current
    // screen state if necessary
    setBackgroundMode(QWidget::NoBackground);

    setCursor( blankCursor );
    setGeometry(0, 0, mRootWidth, mRootHeight);
    hide();

    kdDebug(1204) << "Saver window Id: " << winId() << endl;
}

//---------------------------------------------------------------------------
//
// Hide the screensaver window
//
void LockProcess::hideSaverWindow()
{
  hide();
  lower();
  removeVRoot(winId());
  XDeleteProperty(qt_xdisplay(), winId(), gXA_SCREENSAVER_VERSION);
  if ( gVRoot ) {
      unsigned long vroot_data[1] = { gVRootData };
      XChangeProperty(qt_xdisplay(), gVRoot, gXA_VROOT, XA_WINDOW, 32,
                      PropModeReplace, (unsigned char *)vroot_data, 1);
      gVRoot = 0;
  }
  XSync(qt_xdisplay(), False);
}

//---------------------------------------------------------------------------
static int ignoreXError(Display *, XErrorEvent *)
{
    return 0;
}

//---------------------------------------------------------------------------
//
// Save the current virtual root window
//
void LockProcess::saveVRoot()
{
  Window rootReturn, parentReturn, *children;
  unsigned int numChildren;
  Window root = RootWindowOfScreen(ScreenOfDisplay(qt_xdisplay(), qt_xscreen()));

  gVRoot = 0;
  gVRootData = 0;

  int (*oldHandler)(Display *, XErrorEvent *);
  oldHandler = XSetErrorHandler(ignoreXError);

  if (XQueryTree(qt_xdisplay(), root, &rootReturn, &parentReturn,
      &children, &numChildren))
  {
    for (unsigned int i = 0; i < numChildren; i++)
    {
      Atom actual_type;
      int actual_format;
      unsigned long nitems, bytesafter;
      Window *newRoot = (Window *)0;

      if ((XGetWindowProperty(qt_xdisplay(), children[i], gXA_VROOT, 0, 1,
          False, XA_WINDOW, &actual_type, &actual_format, &nitems, &bytesafter,
          (unsigned char **) &newRoot) == Success) && newRoot)
      {
        gVRoot = children[i];
        gVRootData = *newRoot;
        break;
      }
    }
    if (children)
    {
      XFree((char *)children);
    }
  }

  XSetErrorHandler(oldHandler);
}

//---------------------------------------------------------------------------
//
// Set the virtual root property
//
void LockProcess::setVRoot(Window win, Window vr)
{
    if (gVRoot)
        removeVRoot(gVRoot);

    unsigned long rw = RootWindowOfScreen(ScreenOfDisplay(qt_xdisplay(), qt_xscreen()));
    unsigned long vroot_data[1] = { vr };

    Window rootReturn, parentReturn, *children;
    unsigned int numChildren;
    Window top = win;
    while (1) {
        XQueryTree(qt_xdisplay(), top , &rootReturn, &parentReturn,
                                 &children, &numChildren);
        if (children)
            XFree((char *)children);
        if (parentReturn == rw) {
            break;
        } else
            top = parentReturn;
    }

    XChangeProperty(qt_xdisplay(), top, gXA_VROOT, XA_WINDOW, 32,
                     PropModeReplace, (unsigned char *)vroot_data, 1);
}

//---------------------------------------------------------------------------
//
// Remove the virtual root property
//
void LockProcess::removeVRoot(Window win)
{
    XDeleteProperty (qt_xdisplay(), win, gXA_VROOT);
}

//---------------------------------------------------------------------------
//
// Grab the keyboard. Returns true on success
//
bool LockProcess::grabKeyboard()
{
    int rv = XGrabKeyboard( qt_xdisplay(), QApplication::desktop()->winId(),
        True, GrabModeAsync, GrabModeAsync, CurrentTime );

    return (rv == GrabSuccess);
}

#define GRABEVENTS ButtonPressMask | ButtonReleaseMask | PointerMotionMask | \
		   EnterWindowMask | LeaveWindowMask

//---------------------------------------------------------------------------
//
// Grab the mouse.  Returns true on success
//
bool LockProcess::grabMouse()
{
    int rv = XGrabPointer( qt_xdisplay(), QApplication::desktop()->winId(),
            True, GRABEVENTS, GrabModeAsync, GrabModeAsync, None,
            blankCursor.handle(), CurrentTime );

    return (rv == GrabSuccess);
}

//---------------------------------------------------------------------------
//
// Grab keyboard and mouse.  Returns true on success.
//
bool LockProcess::grabInput()
{
    XSync(qt_xdisplay(), False);

    if (!grabKeyboard())
    {
        sleep(1);
        if (!grabKeyboard())
        {
            return false;
        }
    }

    if (!grabMouse())
    {
        sleep(1);
        if (!grabMouse())
        {
            XUngrabKeyboard(qt_xdisplay(), CurrentTime);
            return false;
        }
    }

    lockXF86();

    return true;
}

//---------------------------------------------------------------------------
//
// Release mouse an keyboard grab.
//
void LockProcess::ungrabInput()
{
    XUngrabKeyboard(qt_xdisplay(), CurrentTime);
    XUngrabPointer(qt_xdisplay(), CurrentTime);
    unlockXF86();
}

//---------------------------------------------------------------------------
//
// Send KDM (or XDM, if it gets adapted) a command
//
void LockProcess::xdmFifoCmd(const char *cmd)
{
    QFile fifo(mXdmFifoName);
    if (fifo.open(IO_WriteOnly | IO_Raw)) {
        fifo.writeBlock( cmd, ::strlen(cmd) );
        fifo.close();
    }
}

void LockProcess::xdmFifoLockCmd(const char *cmd)
{
    if (!mXdmFifoName.isNull() && mLocked)
        xdmFifoCmd(cmd);
}

//---------------------------------------------------------------------------
//
// Start the screen saver.
//
bool LockProcess::startSaver()
{
    if (!child_saver && !grabInput())
    {
        kdWarning(1204) << "LockProcess::startSaver() grabInput() failed!!!!" << endl;
        return false;
    }
    mBusy = false;

    saveVRoot();

    if (mParent) {
        QSocketNotifier *notifier = new QSocketNotifier(mParent, QSocketNotifier::Read, this, "notifier");
        connect(notifier, SIGNAL( activated (int)), SLOT( quitSaver()));
    }
    createSaverWindow();
    move(0, 0);
    show();
    setCursor( blankCursor );

    raise();
    XSync(qt_xdisplay(), False);
    setVRoot( winId(), winId() );
    if (!startHack())
        setBackgroundColor(black); // failed to start a hack.  Just show a blank screen

    return true;
}

//---------------------------------------------------------------------------
//
// Stop the screen saver.
//
void LockProcess::stopSaver()
{
    kdDebug(1204) << "LockProcess: stopping saver" << endl;
    resume();
    stopHack();
    hideSaverWindow();
    mVisibility = false;
    if (!child_saver) {
        xdmFifoLockCmd("unlock\n");
        ungrabInput();
        const char *out = "GOAWAY!";
        for (QValueList<int>::ConstIterator it = child_sockets.begin(); it != child_sockets.end(); ++it)
            write(*it, out, sizeof(out));
    }
}

// private static
QVariant LockProcess::getConf(void *ctx, const char *key, const QVariant &dflt)
{
    LockProcess *that = (LockProcess *)ctx;
    QString fkey = QString::fromLatin1( key ) + '=';
    for (QStringList::ConstIterator it = that->mPluginOptions.begin();
         it != that->mPluginOptions.end(); ++it)
        if ((*it).startsWith( fkey ))
            return (*it).mid( fkey.length() );
    return dflt;
}

void LockProcess::cantLock( const QString &txt)
{
    msgBox( QMessageBox::Critical, i18n("Will not lock the session, as unlocking would be impossible:\n") + txt );
}

#if 0 // placeholders for later
i18n("Cannot start <i>kcheckpass</i>.");
i18n("<i>kcheckpass</i> is unable to operate. Possibly it is not SetUID root.");
#endif

//---------------------------------------------------------------------------
//
// Make the screen saver password protected.
//
bool LockProcess::startLock()
{
    for (QStringList::ConstIterator it = mPlugins.begin(); it != mPlugins.end(); ++it) {
        GreeterPluginHandle plugin;
        QString path = KLibLoader::self()->findLibrary(
                    ((*it)[0] == '/' ? *it : "kgreet_" + *it ).latin1() );
        if (path.isEmpty()) {
            kdWarning(1204) << "GreeterPlugin " << *it << " does not exist" << endl;
            continue;
        }
        if (!(plugin.library = KLibLoader::self()->library( path.latin1() ))) {
            kdWarning(1204) << "Cannot load GreeterPlugin " << *it << " (" << path << ")" << endl;
            continue;
        }
        if (!plugin.library->hasSymbol( "kgreeterplugin_info" )) {
            kdWarning(1204) << "GreeterPlugin " << *it << " (" << path << ") is no valid greet widget plugin" << endl;
            plugin.library->unload();
            continue;
        }
        plugin.info = (kgreeterplugin_info*)plugin.library->symbol( "kgreeterplugin_info" );
        if (plugin.info->method && !mMethod.isEmpty() && mMethod != plugin.info->method) {
            kdDebug(1204) << "GreeterPlugin " << *it << " (" << path << ") serves " << plugin.info->method << ", not " << mMethod << endl;
            plugin.library->unload();
            continue;
        }
        if (!plugin.info->init( mMethod, getConf, this )) {
            kdDebug(1204) << "GreeterPlugin " << *it << " (" << path << ") refuses to serve " << mMethod << endl;
            plugin.library->unload();
            continue;
        }
        kdDebug(1204) << "GreeterPlugin " << *it << " (" << plugin.info->method << ", " << plugin.info->name << ") loaded" << endl;
        greetPlugin = plugin;
	KGlobal::locale()->insertCatalogue( "kdmgreet" );
	mLocked = true;
	xdmFifoLockCmd("lock\n");
	return true;
    }
    cantLock( i18n("No appropriate greeter plugin configured.") );
    return false;
}


//---------------------------------------------------------------------------
//
bool LockProcess::startHack()
{
    if (mSaverExec.isEmpty())
    {
        return false;
    }

    if (mHackProc.isRunning())
    {
        stopHack();
    }

    mHackProc.clearArguments();

    QTextStream ts(&mSaverExec, IO_ReadOnly);
    QString word;
    ts >> word;
    QString path = KStandardDirs::findExe(word);

    if (!path.isEmpty())
    {
        mHackProc << path;

        kdDebug(1204) << "Starting hack: " << path << endl;

        while (!ts.atEnd())
        {
            ts >> word;
            if (word == "%w")
            {
                word = word.setNum(winId());
            }
            mHackProc << word;
        }

	if (!mForbidden)
	{
		if (mHackProc.start() == true)
		{
#ifdef HAVE_SETPRIORITY
			setpriority(PRIO_PROCESS, mHackProc.pid(), mPriority);
#endif
			return true;
		}
	}
	else // we aren't allowed to start the specified screensaver according to the kiosk restrictions
	{
		setBackgroundColor(black);
	}
    }
    return false;
}

//---------------------------------------------------------------------------
//
void LockProcess::stopHack()
{
    if (mHackProc.isRunning())
    {
        mHackProc.kill();
    }
}

//---------------------------------------------------------------------------
//
void LockProcess::hackExited(KProcess *)
{
	// Hack exited while we're supposed to be saving the screen.
	// Make sure the saver window is black.
        setBackgroundColor(black);
}

void LockProcess::suspend()
{
    if(!mSuspended)
    {
        mHackProc.kill(SIGSTOP);
        QApplication::syncX();
        mSavedScreen = QPixmap::grabWindow( winId());
    }
    mSuspended = true;
}

void LockProcess::resume()
{
    if (!mDialogs.isEmpty())
        return; // no resuming with dialog visible
    if(!mVisibility)
        return; // no need to resume, not visible
    if(mSuspended)
    {
        bitBlt( this, 0, 0, &mSavedScreen );
        QApplication::syncX();
        mHackProc.kill(SIGCONT);
    }
    mSuspended = false;
}

//---------------------------------------------------------------------------
//
// Show the password dialog
// This is called only in the master process
//
bool LockProcess::checkPass()
{
    PasswordDlg passDlg( this, &greetPlugin, !mXdmFifoName.isNull());
    connect(&passDlg, SIGNAL(startNewSession()), SLOT(startNewSession()));

    return execDialog( &passDlg ) == QDialog::Accepted;
}

static void fakeFocusIn( WId window )
{
    // We have keyboard grab, so this application will
    // get keyboard events even without having focus.
    // Fake FocusIn to make Qt realize it has the active
    // window, so that it will correctly show cursor in the dialog.
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xfocus.display = qt_xdisplay();
    ev.xfocus.type = FocusIn;
    ev.xfocus.window = window;
    ev.xfocus.mode = NotifyNormal;
    ev.xfocus.detail = NotifyAncestor;
    XSendEvent( qt_xdisplay(), window, False, NoEventMask, &ev );
}

int LockProcess::execDialog( QDialog *dlg )
{
    dlg->adjustSize();

    QRect rect = dlg->geometry();
    rect.moveCenter(KGlobalSettings::desktopGeometry(QCursor::pos()).center());
    dlg->move( rect.topLeft() );

    if (mDialogs.isEmpty())
    {
        suspend();
        XChangeActivePointerGrab( qt_xdisplay(), GRABEVENTS,
                arrowCursor.handle(), CurrentTime);
    }
    mDialogs.prepend( dlg );
    fakeFocusIn( dlg->winId());
    int rt = dlg->exec();
    mDialogs.remove( dlg );
    if( mDialogs.isEmpty() ) {
        XChangeActivePointerGrab( qt_xdisplay(), GRABEVENTS,
                blankCursor.handle(), CurrentTime);
        resume();
    } else
        fakeFocusIn( mDialogs.first()->winId());
    return rt;
}


//---------------------------------------------------------------------------
//
// X11 Event.
//
bool LockProcess::x11Event(XEvent *event)
{
    switch (event->type)
    {
        case KeyPress:
        case ButtonPress:
        case MotionNotify:
            if (mBusy || !mDialogs.isEmpty())
                break;
            mBusy = true;
            if (!mLocked || checkPass())
            {
                stopSaver();
                kapp->quit();
            }
            mBusy = false;
            return true;

        case VisibilityNotify:
            if( event->xvisibility.window == winId())
            {  // mVisibility == false means the screensaver is not visible at all
               // e.g. when switched to text console
                mVisibility = !(event->xvisibility.state == VisibilityFullyObscured);
                if(!mVisibility)
                    mSuspendTimer.start(2000, true);
                else
                {
                    mSuspendTimer.stop();
                    resume();
                }
                if (event->xvisibility.state != VisibilityUnobscured)
                    stayOnTop();
            }
            break;

        case ConfigureNotify: // from SubstructureNotifyMask on the root window
            if(event->xconfigure.event == qt_xrootwin())
                stayOnTop();
            break;
        case MapNotify: // from SubstructureNotifyMask on the root window
            if( event->xmap.event == qt_xrootwin())
                stayOnTop();
            break;
    }

    // We have grab with the grab window being the root window.
    // This results in key events being sent to the root window,
    // but they should be sent to the dialog if it's visible.
    // It could be solved by setFocus() call, but that would mess
    // the focus after this process exits.
    // Qt seems to be quite hard to persuade to redirect the event,
    // so let's simply dupe it with correct destination window,
    // and ignore the original one.
    if(!mDialogs.isEmpty() && ( event->type == KeyPress || event->type == KeyRelease)
        && event->xkey.window != mDialogs.first()->winId())
    {
        XEvent ev2 = *event;
        ev2.xkey.window = ev2.xkey.subwindow = mDialogs.first()->winId();
        qApp->x11ProcessEvent( &ev2 );
        return true;
    }

    return false;
}

void LockProcess::stayOnTop()
{
    if(!mDialogs.isEmpty())
    {
        // this restacking is written in a way so that
        // if the stacking positions actually don't change,
        // all restacking operations will be no-op,
        // and no ConfigureNotify will be generated,
        // thus avoiding possible infinite loops
        XRaiseWindow( qt_xdisplay(), mDialogs.first()->winId()); // raise topmost
        // and stack others below it
        Window* stack = new Window[ mDialogs.count() + 1 ];
        int count = 0;
        for( QValueList< QWidget* >::ConstIterator it = mDialogs.begin();
             it != mDialogs.end();
             ++it )
            stack[ count++ ] = (*it)->winId();
        stack[ count++ ] = winId();
        XRestackWindows( x11Display(), stack, count );
        delete[] stack;
    }
    else
        XRaiseWindow(qt_xdisplay(), winId());
}

void LockProcess::startNewSession()
{
    xdmFifoCmd("reserve\n");
}

void LockProcess::checkDPMSActive()
{
#ifdef HAVE_DPMS
    BOOL on;
    CARD16 state;
    DPMSInfo(qt_xdisplay(), &state, &on);
    if (state == DPMSModeStandby || state == DPMSModeSuspend || state == DPMSModeOff)
    {
       suspend();
    }
#endif
}

#if defined(HAVE_XF86MISC) && defined(HAVE_XF86MISCSETGRABKEYSSTATE)
// see http://cvsweb.xfree86.org/cvsweb/xc/programs/Xserver/hw/xfree86/common/xf86Events.c#rev3.113
// This allows enabling the "Allow{Deactivate/Closedown}Grabs" options in XF86Config,
// and kdesktop_lock will still lock the session.
static enum { Unknown, Yes, No } can_do_xf86_lock = Unknown;
void LockProcess::lockXF86()
{
    if( can_do_xf86_lock == Unknown )
    {
        int major, minor;
        if( XF86MiscQueryVersion( qt_xdisplay(), &major, &minor )
            && major >= 0 && minor >= 5 )
            can_do_xf86_lock = Yes;
        else
            can_do_xf86_lock = No;
    }
    if( can_do_xf86_lock != Yes )
        return;
    if( mRestoreXF86Lock )
        return;
    if( XF86MiscSetGrabKeysState( qt_xdisplay(), False ) != MiscExtGrabStateSuccess )
        return;
    // success
    mRestoreXF86Lock = true;
}

void LockProcess::unlockXF86()
{
    if( can_do_xf86_lock != Yes )
        return;
    if( !mRestoreXF86Lock )
        return;
    XF86MiscSetGrabKeysState( qt_xdisplay(), True );
    mRestoreXF86Lock = false;
}
#else
void LockProcess::lockXF86()
{
}

void LockProcess::unlockXF86()
{
}
#endif

void LockProcess::msgBox( QMessageBox::Icon type, const QString &txt )
{
    QDialog box( 0, "messagebox", true, WX11BypassWM );
    QFrame *winFrame = new QFrame( &box );
    winFrame->setFrameStyle( QFrame::WinPanel | QFrame::Raised );
    winFrame->setLineWidth( 2 );
    QLabel *label1 = new QLabel( winFrame );
    label1->setPixmap( QMessageBox::standardIcon( type ) );
    QLabel *label2 = new QLabel( txt, winFrame );
    KPushButton *button = new KPushButton( KStdGuiItem::ok(), winFrame );
    button->setDefault( true );
    button->setSizePolicy( QSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred ) );
    connect( button, SIGNAL( clicked() ), &box, SLOT( accept() ) );

    QVBoxLayout *vbox = new QVBoxLayout( &box );
    vbox->addWidget( winFrame );
    QGridLayout *grid = new QGridLayout( winFrame, 2, 2, 10 );
    grid->addWidget( label1, 0, 0, Qt::AlignCenter );
    grid->addWidget( label2, 0, 1, Qt::AlignCenter );
    grid->addMultiCellWidget( button, 1,1, 0,1, Qt::AlignCenter );

    execDialog( &box );
}


#include "lockprocess.moc"
