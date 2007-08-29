/***************************************************************************
 *   Copyright 2005,2006,2007 by Siraj Razick <siraj@kdemail.net>          *
 *   Copyright 2007 by Riccardo Iaconelli <riccardo@kde.org>               *
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

#ifndef CLOCK_H
#define CLOCK_H

#include <QImage>
#include <QPaintDevice>
#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QPaintEvent>
#include <QPainter>
#include <QTime>
#include <QX11Info>
#include <QGraphicsItem>
#include <QColor>

#include <plasma/applet.h>
#include <plasma/dataengine.h>
#include "ui_clockConfig.h"

class QTimer;

class KDialog;

namespace Plasma
{
    class Svg;
}

class Clock : public Plasma::Applet
{
    Q_OBJECT
    public:
        Clock(QObject *parent, const QVariantList &args);
        ~Clock();

        void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect &contentsRect);
        void setPath(const QString&);
        QSizeF contentSize() const;
        void constraintsUpdated();
        QPainterPath shape() const;

    public slots:
        void updated(const QString &name, const Plasma::DataEngine::Data &data);
        void showConfigurationInterface();

    protected slots:
//         void acceptedTimeStringState(bool);
        void configAccepted();

    private:
        void drawHand(QPainter *p, int rotation, const QString &handName);

        bool m_showTimeString;
        bool m_showSecondHand;
        QSizeF m_size;
        int m_pixelSize;
        QString m_timezone;
        Plasma::Svg* m_theme;
        QTime m_time;
        KDialog *m_dialog; //should we move this into another class?
        QTime m_lastTimeSeen;
        /// Designer Config file
        Ui::clockConfig ui;
};

K_EXPORT_PLASMA_APPLET(clock, Clock)

#endif
