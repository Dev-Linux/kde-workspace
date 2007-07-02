//-----------------------------------------------------------------------------
//
// Screen savers for KDE
//
// Copyright (c)  Martin R. Jones 1999
//
// This is an extremely simple program that starts a random screensaver.
//

#include <config-workspace.h>

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <QTextStream>
#include <QLayout>
#include <QCheckBox>
#include <QWidget>
//Added by qt3to4:
#include <QGridLayout>

#include <kapplication.h>
#include <kstandarddirs.h>
#include <kglobal.h>
#include <klocale.h>
#include <kdesktopfile.h>
#include <krandomsequence.h>
#include <kdebug.h>
#include <kcmdlineargs.h>
#include <kdialog.h>
#include <kconfig.h>
#include <kservice.h>
#include "kscreensaver_vroot.h"
#include "random.h"
#include <QX11Info>
#include <QFrame>
#include <kservicetypetrader.h>
#define MAX_ARGS    20

void usage(char *name)
{
	puts(i18n("Usage: %1 [-setup] [args]\n"
				"Starts a random screen saver.\n"
				"Any arguments (except -setup) are passed on to the screen saver.", name ).toLocal8Bit().data());
}

static const char appName[] = "random";

static const char description[] = I18N_NOOP("Start a random KDE screen saver");

static const char version[] = "2.0.0";

//----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	KCmdLineArgs::init(argc, argv, appName, "kscreensaver", ki18n("Random screen saver"), version, ki18n(description));


	KCmdLineOptions options;

	options.add("setup", ki18n("Setup screen saver"));

	options.add("window-id wid", ki18n("Run in the specified XWindow"));

	options.add("root", ki18n("Run in the root XWindow"));

	KCmdLineArgs::addCmdLineOptions(options);

	KApplication app;

	Window windowId = 0;

	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	if (args->isSet("setup"))
	{
		KRandomSetup setup;
		setup.exec();
		exit(0);
	}

	if (args->isSet("window-id"))
	{
		windowId = args->getOption("window-id").toInt();
	}

	if (args->isSet("root"))
	{
		QX11Info info;
		windowId = RootWindow(QX11Info::display(), info.screen());
	}
	args->clear();
	KService::List lst = KServiceTypeTrader::self()->query( "ScreenSaver");
	QStringList saverFileList;

	KConfig type("krandom.kssrc", KConfig::NoGlobals);
        const KConfigGroup configGroup = type.group("Settings");
	bool opengl = configGroup.readEntry("OpenGL", false);
	bool manipulatescreen = configGroup.readEntry("ManipulateScreen", false);
        bool fortune = !KStandardDirs::findExe("fortune").isEmpty();
        for( KService::List::const_iterator it = lst.begin();
            it != lst.end(); it++)
	{
		QString file = KStandardDirs::locate("services", (*it)->desktopEntryPath());
		kDebug() << "Looking at " << file << endl;
		KDesktopFile saver( file );
		kDebug() << "read X-KDE-Type" << endl;
		QString saverType = saver.desktopGroup().readEntry("X-KDE-Type");
		if (saverType.isEmpty()) // no X-KDE-Type defined so must be OK
		{
			saverFileList.append(file);
		}
		else
		{
			QStringList saverTypes = saverType.split( ";");
			for (QStringList::ConstIterator it =  saverTypes.begin(); it != saverTypes.end(); ++it )
			{
				kDebug() << "saverTypes is "<< *it << endl;
				if (*it == "ManipulateScreen")
				{
					if (manipulatescreen)
					{
						saverFileList.append(file);
					}
				}
				else
				if (*it == "OpenGL")
				{
					if (opengl)
					{
						saverFileList.append(file);
					}
				}
				if (*it == "Fortune")
				{
					if (fortune)
					{
						saverFileList.append(file);
					}
				}

			}
		}
	}

	KRandomSequence rnd;
	int indx = rnd.getLong(saverFileList.count());
	QString filename = saverFileList.at(indx);

	KDesktopFile config( filename );

        KConfigGroup cg = config.desktopGroup();
	QString cmd;
	if (windowId && config.hasActionGroup("InWindow"))
	{
		cg = config.actionGroup("InWindow");
	}
	else if ((windowId == 0) && config.hasActionGroup("Root"))
	{
		cg = config.actionGroup("Root");
	}
	cmd = cg.readPathEntry("Exec");

	QTextStream ts(&cmd, QIODevice::ReadOnly);
	QString word;
	ts >> word;
	QString exeFile = KStandardDirs::findExe(word);

	if (!exeFile.isEmpty())
	{
		char *sargs[MAX_ARGS];
		sargs[0] = new char [strlen(word.toAscii())+1];
		strcpy(sargs[0], word.toAscii());

		int i = 1;
		while (!ts.atEnd() && i < MAX_ARGS-1)
		{
			ts >> word;
			if (word == "%w")
			{
				word = word.setNum(windowId);
			}

			sargs[i] = new char [strlen(word.toAscii())+1];
			strcpy(sargs[i], word.toAscii());
			kDebug() << "word is " << word.toAscii() << endl;

			i++;
		}

		sargs[i] = 0;

		execv(exeFile.toAscii(), sargs);
	}

	// If we end up here then we couldn't start a saver.
	// If we have been supplied a window id or root window then blank it.
	QX11Info info;
	Window win = windowId ? windowId : RootWindow(QX11Info::display(), info.screen());
	XSetWindowBackground(QX11Info::display(), win,
			BlackPixel(QX11Info::display(), info.screen()));
	XClearWindow(QX11Info::display(), win);
}


KRandomSetup::KRandomSetup( QWidget *parent, const char *name )
	: KDialog( parent )
{
  setObjectName( name );
  setModal( true );
  setCaption( i18n( "Setup Random Screen Saver" ) );
  setButtons( Ok | Cancel );
  showButtonSeparator( true );

	QFrame *main = new QFrame( this );
  setMainWidget( main );
	QGridLayout *grid = new QGridLayout(main );
        grid->setSpacing( spacingHint() );

	openGL = new QCheckBox( i18n("Use OpenGL screen savers"), main );
	grid->addWidget(openGL, 0, 0);

	manipulateScreen = new QCheckBox(i18n("Use screen savers that manipulate the screen"), main);
	grid->addWidget(manipulateScreen, 1, 0);

	setMinimumSize( sizeHint() );

	KConfig config("krandom.kssrc", KConfig::NoGlobals);
        const KConfigGroup configGroup = config.group("Settings");
	openGL->setChecked(configGroup.readEntry("OpenGL", true));
	manipulateScreen->setChecked(configGroup.readEntry("ManipulateScreen", true));

  connect( this, SIGNAL( okClicked() ), SLOT( slotOk() ) );
}

void KRandomSetup::slotOk()
{
    KConfig config("krandom.kssrc");
    KConfigGroup configGroup = config.group("Settings");
    configGroup.writeEntry("OpenGL", openGL->isChecked());
    configGroup.writeEntry("ManipulateScreen", manipulateScreen->isChecked());

    accept();
}

#include "random.moc"
