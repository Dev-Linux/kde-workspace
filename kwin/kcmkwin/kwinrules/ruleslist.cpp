/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "ruleslist.h"

#include <klistwidget.h>
#include <kpushbutton.h>
#include <assert.h>
#include <kdebug.h>
#include <kconfig.h>

#include "ruleswidget.h"

namespace KWin
{

KCMRulesList::KCMRulesList( QWidget* parent)
: KCMRulesListBase( parent)
    {
    // connect both current/selected, so that current==selected (stupid QListBox :( )
    connect( rules_listbox, SIGNAL( currentChanged( QListWidgetItem* )),
        SLOT( activeChanged( QListWidgetItem*)));
    connect( rules_listbox, SIGNAL( selectionChanged( QListWidgetItem* )),
        SLOT( activeChanged( QListWidgetItem*)));
    connect( new_button, SIGNAL( clicked()),
        SLOT( newClicked()));
    connect( modify_button, SIGNAL( clicked()),
        SLOT( modifyClicked()));
    connect( delete_button, SIGNAL( clicked()),
        SLOT( deleteClicked()));
    connect( moveup_button, SIGNAL( clicked()),
        SLOT( moveupClicked()));
    connect( movedown_button, SIGNAL( clicked()),
        SLOT( movedownClicked()));
    connect( rules_listbox, SIGNAL( doubleClicked ( QListWidgetItem * ) ),
            SLOT( modifyClicked()));
    load();
    }

KCMRulesList::~KCMRulesList()
    {
    for( QVector< Rules* >::Iterator it = rules.begin();
         it != rules.end();
         ++it )
        delete *it;
    rules.clear();
    }

void KCMRulesList::activeChanged( QListWidgetItem* item )
    {
    int itemRow = rules_listbox->row(item);

    if( item != NULL )
        item->setSelected( true ); // make current==selected
    modify_button->setEnabled( item != NULL );
    delete_button->setEnabled( item != NULL );
    moveup_button->setEnabled( item != NULL && itemRow > 0 );
    movedown_button->setEnabled( item != NULL && itemRow < (rules_listbox->count()-1) );
    }

void KCMRulesList::newClicked()
    {
    RulesDialog dlg;
    Rules* rule = dlg.edit( NULL, 0, false );
    if( rule == NULL )
        return;
    int pos = rules_listbox->currentRow() + 1;
    rules_listbox->insertItem( pos , rule->description );
    rules_listbox->item(pos)->setSelected( true );
    rules.insert( rules.begin() + pos, rule );
    emit changed( true );
    }

void KCMRulesList::modifyClicked()
    {
    int pos = rules_listbox->currentRow();
    if ( pos == -1 )
        return;
    RulesDialog dlg;
    Rules* rule = dlg.edit( rules[ pos ], 0, false );
    if( rule == rules[ pos ] )
        return;
    delete rules[ pos ];
    rules[ pos ] = rule;
    rules_listbox->item(pos)->setText( rule->description );
    emit changed( true );
    }

void KCMRulesList::deleteClicked()
    {
    int pos = rules_listbox->currentRow();
    assert( pos != -1 );
    delete rules_listbox->takeItem( pos );
    rules.erase( rules.begin() + pos );
    emit changed( true );
    }

void KCMRulesList::moveupClicked()
    {
    int pos = rules_listbox->currentRow();
    assert( pos != -1 );
    if( pos > 0 )
        {
        QString txt = rules_listbox->item(pos)->text();
        delete rules_listbox->takeItem( pos );
        rules_listbox->insertItem( pos - 1 , txt );
        rules_listbox->item(pos-1)->setSelected( true );
        Rules* rule = rules[ pos ];
        rules[ pos ] = rules[ pos - 1 ];
        rules[ pos - 1 ] = rule;
        }
    emit changed( true );
    }

void KCMRulesList::movedownClicked()
    {
    int pos = rules_listbox->currentRow();
    assert( pos != -1 );
    if( pos < int( rules_listbox->count()) - 1 )
        {
        QString txt = rules_listbox->item(pos)->text();
        delete rules_listbox->takeItem( pos );
        rules_listbox->insertItem( pos + 1 , txt);
        rules_listbox->item(pos+1)->setSelected( true );
        Rules* rule = rules[ pos ];
        rules[ pos ] = rules[ pos + 1 ];
        rules[ pos + 1 ] = rule;
        }
    emit changed( true );
    }

void KCMRulesList::load()
    {
    rules_listbox->clear();
    for( QVector< Rules* >::Iterator it = rules.begin();
         it != rules.end();
         ++it )
        delete *it;
    rules.clear();
    KConfig _cfg( "kwinrulesrc" );
    KConfigGroup cfg(&_cfg, "General" );
    int count = cfg.readEntry( "count",0 );
    rules.reserve( count );
    for( int i = 1;
         i <= count;
         ++i )
        {
        cfg.changeGroup( QString::number( i ));
        Rules* rule = new Rules( cfg );
        rules.append( rule );
        rules_listbox->addItem( rule->description );
        }
    if( rules.count() > 0 )
        rules_listbox->item(0)->setSelected( true );
    else
        activeChanged( NULL );
    }

void KCMRulesList::save()
    {
    KConfig cfg( QLatin1String("kwinrulesrc") );
    QStringList groups = cfg.groupList();
    for( QStringList::ConstIterator it = groups.begin();
         it != groups.end();
         ++it )
        cfg.deleteGroup( *it );
    cfg.group("General").writeEntry( "count", rules.count());
    int i = 1;
    for( QVector< Rules* >::ConstIterator it = rules.begin();
         it != rules.end();
         ++it )
        {
            KConfigGroup cg( &cfg, QString::number( i ));
        (*it)->write( cg );
        ++i;
        }
    }

void KCMRulesList::defaults()
    {
    load();
    }

} // namespace

#include "ruleslist.moc"
