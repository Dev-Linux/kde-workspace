/***************************************************************************
 *   Copyright 2009 by Jacopo De Simoi <wilderkde@gmail.com>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include <KDebug>

#include <QKeyEvent>
#include <QTimer>

#include <KLineEdit>
#include <kstandardshortcut.h>

#include "krunnertabfilter.h"
#include "resultscene.h"

KrunnerTabFilter::KrunnerTabFilter(ResultScene* scene, KLineEdit* edit, QWidget * parent)
    : QObject(parent),
      m_resultScene(scene),
      m_lineEdit(edit)
{
}

KrunnerTabFilter::~KrunnerTabFilter()
{
}

bool KrunnerTabFilter::eventFilter(QObject *obj, QEvent *event)
{
    bool enterPressed = false;
    if ( event->type() == QEvent::KeyPress ) {
        QKeyEvent *e = static_cast<QKeyEvent *>( event );
        enterPressed = ( e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter );

        //WORKAROUND: KHistoryComboBox does not emit any signal when rotating with the standard
        //            shortcuts, work around this fact here.
        int event_key = e->key() | e->modifiers();
        if (KStandardShortcut::rotateUp().contains(event_key) || KStandardShortcut::rotateDown().contains(event_key)) {
            QTimer::singleShot(0, this, SLOT(delayedHistoryRotated()));
        }
    }

    //WORKAROUND: KHistoryComboBox does not emit any signal when rotating with the mouse wheel,
    //            work around this fact here.
    if (event->type() == QEvent::Wheel) {
        QTimer::singleShot(0, this, SLOT(delayedHistoryRotated()));
    }

    if ((event->type() == QEvent::FocusOut) || enterPressed) {
        //FIXME: find a reliable way to see if the scene is empty; now defaults to
        //       never complete
        bool emptyScene = false;
        bool suggestedCompletion = (m_lineEdit->text() != m_lineEdit->userText());

        if (emptyScene &&  suggestedCompletion) {
            // We hit TAB with an empty scene and a suggested completion:
            // Complete but don't lose focus
            m_lineEdit->setText(m_lineEdit->text());
            return true;
        } else if (suggestedCompletion) {
            // We hit TAB with a non-empty scene and a suggested completion:
            // Assume the user wants to switch input to the results scene and discard the completion
            m_lineEdit->setText(m_lineEdit->userText());
        }
   }

    return QObject::eventFilter(obj, event);
}

void KrunnerTabFilter::delayedHistoryRotated()
{
    emit historyRotated(m_lineEdit->text());
}

#include "krunnertabfilter.moc"