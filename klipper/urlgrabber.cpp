/* -------------------------------------------------------------

   urlgrabber.cpp (part of Klipper - Cut & paste history for KDE)

   $Id$

   (C) 2000 by Carsten Pfeiffer <pfeiffer@kde.org>

   Licensed under the Artistic License

 ------------------------------------------------------------- */

#include <kapp.h>
#include <kconfig.h>
#include <kdialogbase.h>
#include <keditcl.h>
#include <klocale.h>
#include <kpopupmenu.h>
#include <kdebug.h>
#include <netwm.h>

#include "urlgrabber.h"

// TODO:
// - script-interface?
// - Bug in KPopupMenu::clear() (insertTitle() doesn't go away sometimes)

#define URL_EDIT_ITEM 10

URLGrabber::URLGrabber()
{
    myCurrentAction = 0L;

    myActions = new ActionList();
    myActions->setAutoDelete( true );
    myMatches.setAutoDelete( false );

    readConfiguration( kapp->config() );

    // testing
    /*
    ClipAction *action;
    action = new ClipAction( "^http:\\/\\/", "Web-URL" );
    action->addCommand("kfmclient exec %s", "Open with Konqi", true);
    action->addCommand("netscape -no-about-splash -remote \"openURL(%s, new-window)\"", "Open with Netscape", true);
    myActions->append( action );

    action = new ClipAction( "^mailto:", "Mail-URL" );
    action->addCommand("kmail --composer %s", "Launch kmail", true);
    myActions->append( action );

    action = new ClipAction( "^\\/.+\\.jpg$", "Jpeg-Image" );
    action->addCommand("kuickshow %s", "Launch KuickShow", true);
    action->addCommand("kview %s", "Launch KView", true);
    myActions->append( action );
    */
}


URLGrabber::~URLGrabber()
{
    delete myActions;
}


void URLGrabber::setActionList( ActionList *list )
{
    delete myActions;
    myActions = list;
}


const ActionList& URLGrabber::matchingActions( const QString& clipData )
{
    myMatches.clear();
    ClipAction *action = 0L;
    ActionListIterator it( *myActions );
    for ( action = it.current(); action; action = ++it ) {
        if ( action->matches( clipData ) )
	    myMatches.append( action );
    }

    return myMatches;
}


bool URLGrabber::checkNewData( const QString& clipData )
{
    // kdDebug() << "** checking new data: " << clipData << endl;
    myClipData = clipData;
    if ( myActions->isEmpty() )
	return false;

    slotActionMenu(); // also creates myMatches

    return ( !myMatches.isEmpty() &&
	     (!kapp->config()->readBoolEntry("Put Matching URLs in history", true)));
}


void URLGrabber::slotActionMenu()
{
    if ( myClipData.isEmpty() )
	return;

    ActionListIterator it( matchingActions( myClipData ) );
    ClipAction *action = 0L;
    ClipCommand *command = 0L;

    if ( it.count() > 0 ) {
	if ( isAvoidedWindow() ) // don't react on konqi's/netscape's urls
	    return;
	
	QString item;
	myCommandMapper.clear();

	KPopupMenu *menu = new KPopupMenu;
	connect( menu, SIGNAL( activated( int )),
		 SLOT( slotItemSelected( int )));

	for ( action = it.current(); action; action = ++it ) {
	    QListIterator<ClipCommand> it2( action->commands() );
	    if ( it2.count() > 0 )
		menu->insertTitle( action->description() +
				   i18n(" -  Actions for: ") + myClipData );
	    for ( command = it2.current(); command; command = ++it2 ) {
		item = command->description;
		if ( item.isEmpty() )
		    item = command->command;
	
		int id = menu->insertItem( item );
		myCommandMapper.insert( id, command );
	    }
	}

	// add an edit-possibility
	menu->insertSeparator();
	menu->insertSeparator();
	menu->insertItem( i18n("&Edit and process again"), URL_EDIT_ITEM );
	menu->insertItem(i18n("Do &Nothing"), menu, SLOT(close()));

	emit sigPopup( menu );
	delete menu;
    }
}


void URLGrabber::slotItemSelected( int id )
{
    switch ( id ) {
    case -1:
	break;
    case URL_EDIT_ITEM:
	editData();
	break;
    default:
	ClipCommand *command = myCommandMapper.find( id );
	if ( !command )
	    warning("Klipper: can't find associated action");
	else
	    execute( command );
    }
}


void URLGrabber::execute( const struct ClipCommand *command ) const
{
    QString cmdLine;

    if ( command->isEnabled ) {
	cmdLine = command->command;

	// replace "%s" with the clipboard contents
	// replace \%s to %s
	// currently, only the first %s will be replaced... fix this?
	int pos = cmdLine.find("%s");
	if ( pos >= 0 ) {
	    bool doReplace = true;
	    if ( pos > 0 && cmdLine.at( pos -1 ) == '\\' ) {
		cmdLine.remove( pos -1, 1 ); // \%s -> %s
		doReplace = false;
	    }
	
	    if ( doReplace )
		cmdLine.replace( pos, 2, myClipData );
	}
	
	startProcess( cmdLine );
    }
}


void URLGrabber::startProcess( const QString& cmdLine ) const
{
    kdDebug() << "Klipper: now starting " << cmdLine << endl;
    if ( cmdLine.isEmpty() )
	return;

    int argIdx;
    KShellProcess proc;
    QString cl = cmdLine.simplifyWhiteSpace().stripWhiteSpace();

    while( !cl.isEmpty() ) {
	argIdx = cl.find(" ");

	proc << cl.left( argIdx );

	if (argIdx == -1)
	    argIdx = cl.length();

	cl = cl.remove( 0, argIdx + 1 );
    }

    if ( !proc.start(KProcess::DontCare, KProcess::NoCommunication ))
	warning("Klipper: Couldn't start process!");
}


void URLGrabber::editData()
{
    KDialogBase *dlg = new KDialogBase( 0, 0, true,
					i18n("Edit text before processing"),
					KDialogBase::Ok | KDialogBase::Cancel);
    KEdit *edit = new KEdit( dlg );
    edit->setText( myClipData );
    edit->setMinimumSize( 300, 40 );
    dlg->setMainWidget( edit );
    dlg->adjustSize();

    if ( dlg->exec() == QDialog::Accepted ) {
	myClipData = edit->text();
	delete dlg;
	slotActionMenu();
    }
    else
	delete dlg;
}


void URLGrabber::readConfiguration( KConfig *kc )
{
    myActions->clear();
    kc->setGroup( "General" );
    int num = kc->readNumEntry("Number of Actions", 0);
    myAvoidWindows = kc->readListEntry("No Actions for WM_CLASS");

    QString group;
    for ( int i = 0; i < num; i++ ) {
	group = QString("Action_%1").arg( i );
	kc->setGroup( group );
	myActions->append( new ClipAction( kc ) );
    }
}


void URLGrabber::writeConfiguration( KConfig *kc )
{
    kc->setGroup( "General" );
    kc->writeEntry("Number of Actions", myActions->count() );

    ActionListIterator it( *myActions );
    ClipAction *action;

    int i = 0;
    QString group;
    while ( (action = it.current()) ) {
	group = QString("Action_%1").arg( i );
	kc->setGroup( group );
	action->save( kc );
	++i;
	++it;
    }
}

// find out whether the active window's WM_CLASS is in our avoid-list
// digged a little bit in netwm.cpp
bool URLGrabber::isAvoidedWindow() const
{
    Display *d = qt_xdisplay();
    static Atom wm_class = XInternAtom( d, "WM_CLASS", true );
    static Atom active_window = XInternAtom( d, "_NET_ACTIVE_WINDOW", true );
    Atom type_ret;
    int format_ret;
    unsigned long nitems_ret, unused;
    unsigned char *data_ret;
    long BUFSIZE = 2048;
    bool ret = false;
    Window active = 0L;
    QString wmClass;

    // get the active window
    if (XGetWindowProperty(d, DefaultRootWindow( d ), active_window, 0l, 1l,
			   False, XA_WINDOW, &type_ret, &format_ret,
			   &nitems_ret, &unused, &data_ret)
	== Success) {
	if (type_ret == XA_WINDOW && format_ret == 32 && nitems_ret == 1) {
	    active = *((Window *) data_ret);
	}
	XFree(data_ret);
    }
    if ( !active )
	return false;
    
    // get the class of the active window
    if ( XGetWindowProperty(d, active, wm_class, 0L, BUFSIZE, False, XA_STRING,
			    &type_ret, &format_ret, &nitems_ret,
			    &unused, &data_ret ) == Success) {
	if ( type_ret == XA_STRING && format_ret == 8 && nitems_ret > 0 ) {
	    wmClass = QString::fromUtf8( (const char *) data_ret );
	    ret = (myAvoidWindows.find( wmClass ) != myAvoidWindows.end());
	}
	
	XFree( data_ret );
    }
    
    return ret;
}


///////////////////////////////////////////////////////////////////////////
////////



ClipAction::ClipAction( const QString& regExp, const QString& description )
{
    myCommands.setAutoDelete( true );
    myRegExp      = regExp;
    myDescription = description;
}


ClipAction::ClipAction( const ClipAction& action )
{
    myCommands.setAutoDelete( true );
    myRegExp      = action.myRegExp;
    myDescription = action.myDescription;

    ClipCommand *command = 0L;
    QListIterator<ClipCommand> it( myCommands );
    for ( ; it.current(); ++it ) {
	command = it.current();
	addCommand(command->command, command->description, command->isEnabled);
    }
}


ClipAction::ClipAction( KConfig *kc )
{
    myCommands.setAutoDelete( true );
    myRegExp      = kc->readEntry( "Action regexp" );
    myDescription = kc->readEntry( "Action description" );
    int num = kc->readNumEntry( "Number of commands" );

    // read the commands
    QString prefix;
    for ( int i = 0; i < num; i++ ) {
	prefix = QString("Command_%1: ").arg( i );

	addCommand( kc->readEntry( prefix + "commandline" ),
		    kc->readEntry( prefix + "description" ),
		    kc->readBoolEntry( prefix + "enabled" ) );
    }
}


ClipAction::~ClipAction()
{
}


void ClipAction::addCommand( const QString& command,
			     const QString& description, bool enabled )
{
    if ( command.isEmpty() )
	return;

    struct ClipCommand *cmd = new ClipCommand;
    cmd->command = command;
    cmd->description = description;
    cmd->isEnabled = enabled;
    //    cmd->id = myCommands.count(); // superfluous, I think...
    myCommands.append( cmd );
}


// precondition: we're in the correct action's group of the KConfig object
void ClipAction::save( KConfig *kc ) const
{
    kc->writeEntry( "Action description", description() );
    kc->writeEntry( "Action regexp", regExp() );
    kc->writeEntry( "Number of commands", myCommands.count() );

    QString prefix;
    struct ClipCommand *cmd;
    QListIterator<struct ClipCommand> it( myCommands );

    // now iterate over all commands of this action
    int i = 0;
    while ( (cmd = it.current()) ) {
	prefix = QString("Command_%1: ").arg( i );
	
	kc->writeEntry( prefix + "commandline", cmd->command );
	kc->writeEntry( prefix + "description", cmd->description );
	kc->writeEntry( prefix + "enabled", cmd->isEnabled );
	
	++i;
	++it;
    }
}

#include "urlgrabber.moc"
