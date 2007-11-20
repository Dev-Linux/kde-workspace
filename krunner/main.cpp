/*
 *   Copyright (C) 2006 Aaron Seigo <aseigo@kde.org>
 *   Copyright (C) 2006 Zack Rusin <zack@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <KAboutData>
#include <KCmdLineArgs>
#include <kdebug.h>
#include <KLocale>

#include "krunnerapp.h"
#include "saverengine.h"
#include "startupid.h"
#include "kscreensaversettings.h" // contains screen saver config
#include "klaunchsettings.h" // contains startup config

#include <X11/extensions/Xrender.h>

static const char description[] = I18N_NOOP( "KDE run command interface" );
static const char version[] = "0.1";

extern "C"
KDE_EXPORT int kdemain(int argc, char* argv[])
{
    KAboutData aboutData( "krunner", 0, ki18n( "Run Command Interface" ),
                          version, ki18n(description), KAboutData::License_GPL,
                          ki18n("(c) 2006, Aaron Seigo") );
    aboutData.addAuthor( ki18n("Aaron J. Seigo"),
                         ki18n( "Author and maintainer" ),
                         "aseigo@kde.org" );

    KCmdLineArgs::init(argc, argv, &aboutData);
    if (!KUniqueApplication::start()) {
        return 0;
    }

    KRunnerApp *app = KRunnerApp::self();
    app->disableSessionManagement(); // autostarted
    int rc = app->exec();
    delete app;
    return rc;
}

