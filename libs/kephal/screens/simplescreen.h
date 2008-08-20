/*
 *   Copyright 2008 Aike J Sommer <dev@aikesommer.name>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2,
 *   or (at your option) any later version.
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


#ifndef KEPHAL_SIMPLESCREEN_H
#define KEPHAL_SIMPLESCREEN_H

#include "screen.h"

#include <QSize>
#include <QPoint>
#include <QObject>


namespace kephal {

    class SimpleScreen : public Screen {
        Q_OBJECT
        public:
            SimpleScreen(QObject * parent, int id, QSize resolution, QPoint position, bool privacy, bool primary);
            SimpleScreen(QObject * parent);
            
            virtual int id();

            virtual QSize size();
            virtual void setSize(QSize size);
            virtual QPoint position();
            //QList<PositionType> getRelativePosition();

            virtual bool isPrivacyMode();
            virtual void setPrivacyMode(bool b);
            virtual bool isPrimary();
            virtual void setAsPrimary();
            
            QList<Output *> outputs();
            
            void _setId(const int & id);
            void _setSize(const QSize & size);
            void _setPosition(const QPoint & position);
            void _setGeom(const QRect & geom);
            void _setPrimary(const bool & primary);
            
        Q_SIGNALS:
            void selectedAsPrimary(SimpleScreen * screen);
            void privacyModeChangeRequested(SimpleScreen * screen, bool privacy);
            void sizeChangeRequested(SimpleScreen * screen, QSize oldSize, QSize newSize);
            
        private:
            int m_id;
            QSize m_size;
            QPoint m_position;
            bool m_privacy;
            bool m_primary;
    };
    
}


#endif // KEPHAL_SIMPLESCREEN_H

