/*
 *   Copyright (C) 2007 Aaron Seigo <aseigo@kde.org>
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

#include "animator.h"

#include <QPainter>

namespace Plasma
{

Animator::Animator(QObject *parent, const QStringList& list)
    : QObject(parent)
{
    Q_UNUSED(list)
}

Animator::~Animator()
{
}

int Animator::framesPerSecond(Plasma::Phase::Animation animation)
{
    Q_UNUSED(animation)
    return 0;
}

int Animator::framesPerSecond(Plasma::Phase::Movement movement)
{
    Q_UNUSED(movement)
    return 5;
}

int Animator::framesPerSecond(Plasma::Phase::ElementAnimation animation)
{
    Q_UNUSED(animation)
    return 0;
}

Phase::CurveShape Animator::curve(Plasma::Phase::Animation)
{
    return Phase::EaseInOutCurve;
}

Phase::CurveShape Animator::curve(Plasma::Phase::Movement)
{
    return Phase::EaseInOutCurve;
}

Phase::CurveShape Animator::curve(Plasma::Phase::ElementAnimation)
{
    return Phase::EaseInOutCurve;
}

QPixmap Animator::elementAppear(qreal frame, const QPixmap& pixmap)
{
    Q_UNUSED(frame)
    return pixmap;
}

QPixmap Animator::elementDisappear(qreal frame, const QPixmap& pixmap)
{
    Q_UNUSED(frame)
    QPixmap pix(pixmap.size());
    pix.fill(Qt::transparent);

    return pix;
}

void Animator::appear(qreal frame, QGraphicsItem* item)
{
    Q_UNUSED(frame)
    Q_UNUSED(item)
}

void Animator::disappear(qreal frame, QGraphicsItem* item)
{
    Q_UNUSED(frame)
    Q_UNUSED(item)
}

void Animator::activate(qreal frame, QGraphicsItem* item)
{
    Q_UNUSED(frame)
    Q_UNUSED(item)
}

void Animator::frameAppear(qreal frame, QGraphicsItem* item, const QRegion& drawable)
{
    Q_UNUSED(frame)
    Q_UNUSED(item)
    Q_UNUSED(drawable)
}

void Animator::slideIn(qreal progress, QGraphicsItem* item, QPoint destination)
{
    //TODO: implement
}

void Animator::slideOut(qreal progress, QGraphicsItem* item, QPoint destination)
{
    //TODO: implement
}

void Animator::renderBackground(QImage& background)
{
    Q_UNUSED(background)
}

} // Plasma namespace

#include "animator.moc"
