/*
 * main.cpp
 *
 * Copyright (c) 1999 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
 *
 * Requires the Qt widget libraries, available at no cost at
 * http://www.troll.no/
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "commandShortcuts.h"
#include "main.h"
#include "modifiers.h"
#include "shortcuts.h"

#include <qlayout.h>

#include <kdebug.h>
#include <klocale.h>
#include <ksimpleconfig.h>
/*
| Shortcut Schemes | Modifier Keys |

o Current scheme  o New scheme  o Pre-set scheme
| KDE Traditional |v|| <Save Scheme...> <Remove Scheme>
[] Prefer 4-modifier defaults

o Current scheme
o New scheme       <Save Scheme>
o Pre-set scheme   <Remove Scheme>
  | KDE Traditional |v||
[] Prefer 4-modifier defaults

Global Shortcuts
*/
KeyModule::KeyModule( QWidget *parent, const char *name )
: KCModule( parent, name )
{
	initGUI();
}

void KeyModule::initGUI()
{
	m_pTab = new QTabWidget( this );
	QVBoxLayout *l = new QVBoxLayout(this);
	l->addWidget(m_pTab);

	m_pShortcuts = new ShortcutsModule( this );
	m_pTab->addTab( m_pShortcuts, i18n("Shortcut Schemes") );
	connect( m_pShortcuts, SIGNAL(changed(bool)), SLOT(slotModuleChanged(bool)) );

	m_pCommandShortcuts = new CommandShortcutsModule ( this );
	m_pTab->addTab( m_pCommandShortcuts, i18n("Command Shortcuts") );
	connect( m_pCommandShortcuts, SIGNAL(changed(bool)), SLOT(slotModuleChanged(bool)) );
    connect( m_pTab, SIGNAL(currentChanged(QWidget*)), m_pCommandShortcuts, SLOT(showing(QWidget*)) );

	m_pModifiers = new ModifiersModule( this );
	m_pTab->addTab( m_pModifiers, i18n("Modifier Keys") );
	connect( m_pModifiers, SIGNAL(changed(bool)), SLOT(slotModuleChanged(bool)) );
}

// Called when [Reset] is pressed
void KeyModule::load()
{
	kdDebug(125) << "KeyModule::load()" << endl;
	m_pShortcuts->load();
	m_pCommandShortcuts->load();
	m_pModifiers->load();
	setChanged( false );
}

// When [Apply] or [OK] are clicked.
void KeyModule::save()
{
	kdDebug(125) << "KeyModule::save()" << endl;
	m_pShortcuts->save();
	m_pCommandShortcuts->save();
	m_pModifiers->save();
	setChanged( false );
}

void KeyModule::defaults()
{
	kdDebug(125) << "KeyModule::defaults()" << endl;
	m_pShortcuts->defaults();
	m_pCommandShortcuts->defaults();
	m_pModifiers->defaults();
	setChanged( true );
}

QString KeyModule::quickHelp() const
{
  return i18n("<h1>Keyboard Shortcuts</h1> Using shortcuts you can configure certain actions to be"
    " triggered when you press a key or a combination of keys, e.g. Ctrl+C is normally bound to"
    " 'Copy'. KDE allows you to store more than one 'scheme' of shortcuts, so you might want"
    " to experiment a little setting up your own scheme while you can still change back to the"
    " KDE defaults.<p> In the tab 'Global Shortcuts' you can configure non-application specific"
    " bindings like how to switch desktops or maximize a window. In the tab 'Application Shortcuts'"
    " you'll find bindings typically used in applications, such as copy and paste.");
}

void KeyModule::resizeEvent( QResizeEvent * )
{
	m_pTab->setGeometry( 0, 0, width(), height() );
}

void KeyModule::slotModuleChanged( bool bState )
{
	setChanged( bState );
}

//----------------------------------------------------

extern "C"
{
  KCModule *create_keys(QWidget *parent, const char */*name*/)
  {
	// What does this do?  Why not insert klipper and kxkb, too? --ellis, 2002/01/15
	KGlobal::locale()->insertCatalogue("kwin");
	KGlobal::locale()->insertCatalogue("kdesktop");
	KGlobal::locale()->insertCatalogue("kicker");
	return new KeyModule(parent, "kcmkeys");
  }

  void initModifiers()
  {
	kdDebug(125) << "KeyModule::initModifiers()" << endl;

	KConfigGroupSaver cgs( KGlobal::config(), "Keyboard" );
	bool bMacSwap = KGlobal::config()->readBoolEntry( "Mac Modifier Swap", false );
	if( bMacSwap )
		ModifiersModule::setupMacModifierKeys();
  }

  // write all the global keys to kdeglobals
  // this is needed to be able to check for conflicts with global keys in app's keyconfig
  // dialogs, kdeglobals is empty as long as you don't apply any change in controlcenter/keys
  void init_keys()
  {
	kdDebug(125) << "KeyModule::init()\n";

	/*kdDebug(125) << "KKeyModule::init() - Initialize # Modifier Keys Settings\n";
	KConfigGroupSaver cgs( KGlobal::config(), "Keyboard" );
	QString fourMods = KGlobal::config()->readEntry( "Use Four Modifier Keys", KAccel::keyboardHasMetaKey() ? "true" : "false" );
	KAccel::useFourModifierKeys( fourMods == "true" );
	bool bUseFourModifierKeys = KAccel::useFourModifierKeys();
	KGlobal::config()->writeEntry( "User Four Modifier Keys", bUseFourModifierKeys ? "true" : "false", true, true );
	*/
	KAccelActions* keys = new KAccelActions();

	kdDebug(125) << "KeyModule::init() - Load Included Bindings\n";
// this should match the included files above
#define NOSLOTS
#define SHIFT Qt::SHIFT
#define CTRL Qt::CTRL
#define ALT Qt::ALT
#include "../../klipper/klipperbindings.cpp"
#include "../../kwin/kwinbindings.cpp"
#define KICKER_ALL_BINDINGS
#include "../../kicker/core/kickerbindings.cpp"
#include "../../kdesktop/kdesktopbindings.cpp"
#include "../../kxkb/kxkbbindings.cpp"

	kdDebug(125) << "KeyModule::init() - Read Config Bindings\n";
	// Check for old group,
	if( KGlobal::config()->hasGroup( "Global Keys" ) ) {
		keys->readActions( "Global Keys" );
		KGlobal::config()->deleteGroup( "Global Keys", true, true );
	}
	keys->readActions( "Global Shortcuts" );

	// Why are the braces here? -- ellis
	{
	KSimpleConfig cfg( "kdeglobals" );
	cfg.deleteGroup( "Global Shortcuts" );
	}

	kdDebug(125) << "KeyModule::init() - Write Config Bindings\n";
	keys->writeActions( "Global Shortcuts", 0, true, true );

	delete keys;
	initModifiers();
  }
}

#include "main.moc"
