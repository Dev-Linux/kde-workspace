/* -------------------------------------------------------------

   Klipper - Cut & paste history for KDE

   (C) by Andrew Stanley-Jones

   Generated with the KDE Application Generator

 ------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>

#include <mykapp.h>
#include <ktmainwindow.h>
#include <klocale.h>
#include <kcmdlineargs.h>
#include <kwin.h>
#include <kaboutdata.h>

#include "toplevel.h"


static const char *description =
	I18N_NOOP("KDE Cut & Paste history utility");

static const char *version = "v0.7";

int main(int argc, char *argv[])
{
  KAboutData aboutData("klipper", I18N_NOOP("Klipper"),
    version, description, KAboutData::License_GPL,
    "(c) 1998-2000, Andrew Stanley-Jones");
  aboutData.addAuthor("Andrew Stanley-Jones", 0, "asj@cban.com");
  aboutData.addAuthor("Carsten Pfeiffer", 0, "pfeiffer@kde.org");
  KCmdLineArgs::init( argc, argv, &aboutData );
  KUniqueApplication::addCmdLineOptions();

  if (!KUniqueApplication::start()) {
       fprintf(stderr, "%s is already running!\n", aboutData.appName());
       exit(0);
  }
  MyKApplication app;

  TopLevel *toplevel = new TopLevel();
  app.setGlobalKeys( toplevel->globalKeys );

  KWin::setSystemTrayWindowFor( toplevel->winId(), 0 );
  toplevel->show();

  return app.exec();
}
