/*****************************************************************

Copyright (c) 2001 Matthias Elter <elter@kde.org>
Copyright (c) 2001 John Firebaugh <jfirebaugh@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************/

#include <assert.h>

#include <kiconloader.h>
#include <klocale.h>

#include "taskmanager.h"

#include "taskrmbmenu.h"
#include "taskrmbmenu.moc"

TaskRMBMenu::TaskRMBMenu(TaskList* theTasks, bool show, QWidget *parent, const char *name)
	: QPopupMenu( parent, name )
	, tasks( theTasks )
	, showAll( show )
{
	assert( tasks->count() > 0 );
	if( tasks->count() == 1 ) {
		fillMenu(tasks->first());
	} else {
		fillMenu(tasks);
	}
}

TaskRMBMenu::TaskRMBMenu(Task *task, bool show, QWidget *parent, const char *name)
	: QPopupMenu( parent, name )
	, tasks(0)
	, showAll( show )
{
	fillMenu(task);
}

void TaskRMBMenu::fillMenu(Task *t)
{
	int id;
	setCheckable( true );

	id = insertItem( i18n( "Mi&nimize" ), t, SLOT( iconify() ) );
	setItemEnabled( id, !t->isIconified() );
	id = insertItem( i18n( "Ma&ximize" ), t, SLOT( maximize() ) );
	setItemEnabled( id, !t->isMaximized() );
	id = insertItem( i18n( "&Restore" ), t, SLOT( restore() ) );
	setItemEnabled( id, t->isIconified() || t->isMaximized() );

	insertSeparator();

	id = insertItem( i18n( "&Shade" ), t, SLOT( toggleShaded() ) );
	setItemChecked( id, t->isShaded() );
	id = insertItem( i18n( "&Always on Top" ), t, SLOT( toggleAlwaysOnTop() ) );
	setItemChecked( id, t->isAlwaysOnTop() );

	insertSeparator();

	id = insertItem( SmallIcon( "fileclose" ), i18n( "&Close" ), t, SLOT( close() ) );

	if (TaskManager::the()->numberOfDesktops() > 1)
	{
		insertSeparator();

		insertItem(i18n("To &Desktop"), makeDesktopsMenu(t));
		if ( showAll )
		{
			id = insertItem( i18n( "&To Current Desktop" ), t, SLOT( toCurrentDesktop() ) );
			setItemEnabled( id, !t->isOnCurrentDesktop() );
		}
	}
}

void TaskRMBMenu::fillMenu(TaskList *tasks)
{
	int id;
	setCheckable( true );

	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		Task* t = (*it);

		id = insertItem( QIconSet( t->pixmap() ),
				 t->visibleNameWithState(),
		                 new TaskRMBMenu(t, this) );
		setItemChecked( id, t->isActive() );
		connectItem( id, t, SLOT( activateRaiseOrIconify() ) );
	}

	insertSeparator();

	bool enable = false;

	id = insertItem( i18n( "Mi&nimize All" ), this, SLOT( slotMinimizeAll() ) );
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		if( !(*it)->isIconified() ) {
			enable = true;
			break;
		}
	}
	setItemEnabled( id, enable );

	enable = false;

	id = insertItem( i18n( "Ma&ximize All" ), this, SLOT( slotMaximizeAll() ) );
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		if( !(*it)->isMaximized() ) {
			enable = true;
			break;
		}
	}
	setItemEnabled( id, enable );

	enable = false;

	id = insertItem( i18n( "&Restore All" ), this, SLOT( slotRestoreAll() ) );
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		if( (*it)->isIconified() || (*it)->isMaximized() ) {
			enable = true;
			break;
		}
	}
	setItemEnabled( id, enable );

	insertSeparator();

	enable = false;

	/*
	id = insertItem( i18n( "&Shade All" ), this, SLOT( slotShadeAll() ), 0, OpMenu::ShadeOp );
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		if( !(*it)->isShaded() ) {
			enable = true;
			break;
		}
	}
	setItemEnabled( id, enable );
	*/

	insertItem( SmallIcon( "remove" ), i18n( "&Close All" ), this, SLOT( slotCloseAll() ) );

	if (TaskManager::the()->numberOfDesktops() > 1)
	{
		insertSeparator();

		id = insertItem(i18n("All to &Desktop"), makeDesktopsMenu(tasks));

		enable = false;

		id = insertItem( i18n( "All &to Current Desktop" ), this, SLOT( slotAllToCurrentDesktop() ) );
		for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
			if( !(*it)->isOnCurrentDesktop() ) {
				enable = true;
				break;
			}
		}
		setItemEnabled( id, enable );
	}
}

QPopupMenu* TaskRMBMenu::makeDesktopsMenu(Task* t)
{
	QPopupMenu* m = new QPopupMenu( this );
	m->setCheckable( true );

	int id = m->insertItem( i18n("&All Desktops"), t, SLOT( toDesktop(int) ) );
	m->setItemParameter( id, 0 ); // 0 means all desktops
	m->setItemChecked( id, t->isOnAllDesktops() );

	m->insertSeparator();

	for (int i = 1; i <= TaskManager::the()->numberOfDesktops(); i++) {
		QString name = QString("&%1 %2").arg(i).arg(TaskManager::the()->desktopName(i).replace('&', "&&"));
		id = m->insertItem( name, t, SLOT( toDesktop(int) ) );
		m->setItemParameter( id, i );
		m->setItemChecked( id, !t->isOnAllDesktops() && t->desktop() == i );
	}

	return m;
}

QPopupMenu* TaskRMBMenu::makeDesktopsMenu(TaskList*)
{
	QPopupMenu* m = new QPopupMenu( this );
	m->setCheckable( true );

	int id = m->insertItem( i18n("&All Desktops"), this, SLOT( slotAllToDesktop(int) ) );
	m->setItemParameter( id, 0 ); // 0 means all desktops

	m->insertSeparator();

	for (int i = 1; i <= TaskManager::the()->numberOfDesktops(); i++) {
		QString name = QString("&%1 %2").arg(i).arg(TaskManager::the()->desktopName(i).replace('&', "&&"));
		id = m->insertItem( name, this, SLOT( slotAllToDesktop(int) ) );
		m->setItemParameter( id, i );
	}

	return m;
}

void TaskRMBMenu::slotMinimizeAll()
{
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		(*it)->iconify();
	}
}

void TaskRMBMenu::slotMaximizeAll()
{
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		(*it)->maximize();
	}
}

void TaskRMBMenu::slotRestoreAll()
{
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		(*it)->restore();
	}
}

void TaskRMBMenu::slotShadeAll()
{
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		(*it)->setShaded( !(*it)->isShaded() );
	}
}

void TaskRMBMenu::slotCloseAll()
{
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		(*it)->close();
	}
}

void TaskRMBMenu::slotAllToDesktop( int desktop )
{
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		(*it)->toDesktop( desktop );
	}
}

void TaskRMBMenu::slotAllToCurrentDesktop()
{
	for( QPtrListIterator<Task> it(*tasks); *it; ++it ) {
		(*it)->toCurrentDesktop();
	}
}
