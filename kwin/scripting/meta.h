/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Rohan Prabhu <rohan@rohanprabhu.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_SCRIPTING_META_H
#define KWIN_SCRIPTING_META_H

#include "client.h"
#include "s_clientgroup.h"
#include <QStringList>
#include <QScriptValueIterator>

typedef KWin::Client* KClientRef;
typedef KWin::ClientList KClientList;
typedef KWin::ClientGroup* KClientGroupRef;
typedef KWin::Toplevel* KToplevelRef;

namespace KWin
{
namespace Chelate
{
QScriptValue lazyLogicGenerate(QScriptContext*, QScriptEngine*);
}
}

Q_DECLARE_METATYPE(KClientRef)
Q_DECLARE_METATYPE(QPoint)
Q_DECLARE_METATYPE(QSize)
Q_DECLARE_METATYPE(QRect)
Q_DECLARE_METATYPE(KClientList)
Q_DECLARE_METATYPE(KClientGroupRef)
Q_DECLARE_METATYPE(KToplevelRef)
Q_DECLARE_METATYPE(QList<KWin::ClientGroup*>)

namespace KWin
{
namespace MetaScripting
{
/**
  * The toScriptValue and fromScriptValue functions used in qScriptRegisterMetaType.
  * Conversion functions for KWin::Client*
  */
namespace Client
{
QScriptValue toScriptValue(QScriptEngine*, const KClientRef&);
void fromScriptValue(const QScriptValue&, KClientRef&);
}

/**
  * The toScriptValue and fromScriptValue functions used in qScriptRegisterMetaType.
  * Conversion functions for KWin::ClientGroup*
  */
namespace ClientGroup
{
QScriptValue toScriptValue(QScriptEngine*, const KClientGroupRef&);
void fromScriptValue(const QScriptValue&, KClientGroupRef&);
}

/**
  * The toScriptValue and fromScriptValue functions used in qScriptRegisterMetaType.
  * Conversion functions for KWin::Toplevel*
  */
namespace Toplevel
{
QScriptValue toScriptValue(QScriptEngine*, const KToplevelRef&);
void fromScriptValue(const QScriptValue&, KToplevelRef&);
}

/**
  * The toScriptValue and fromScriptValue functions used in qScriptRegisterMetaType.
  * Conversion functions for QPoint
  */
namespace Point
{
QScriptValue toScriptValue(QScriptEngine*, const QPoint&);
void fromScriptValue(const QScriptValue&, QPoint&);
}

/**
  * The toScriptValue and fromScriptValue functions used in qScriptRegisterMetaType.
  * Conversion functions for QSize
  */
namespace Size
{
QScriptValue toScriptValue(QScriptEngine*, const QSize&);
void fromScriptValue(const QScriptValue&, QSize&);
}

/**
  * The toScriptValue and fromScriptValue functions used in qScriptRegisterMetaType.
  * Conversion functions for QRect
  * TODO: QRect conversions have to be linked from plasma as they provide a lot more
  *       features. As for QSize and QPoint, I don't really plan any such thing.
  */
namespace Rect
{
QScriptValue toScriptValue(QScriptEngine*, const QRect&);
void fromScriptValue(const QScriptValue&, QRect&);
}

/**
  * The Reference wrapping used previously for storing pointers to objects that were
  * wrapped. Can still be used for non-QObject converted QScriptValue's, but as of
  * now, it is not in use anywhere.
  */
namespace RefWrapping
{
// Simple template class to wrap pointers within QScriptValue's
template<typename T> void embed(QScriptValue&, const T);
template<typename T> T extract(const QScriptValue&);
}

/**
  * Merges the second QScriptValue in the first one.
  */
void valueMerge(QScriptValue&, QScriptValue);

/**
  * Registers all the meta conversion to the provided QScriptEngine
  */
void registration(QScriptEngine* eng);

/**
  * Get the kind of LazyLogic (tm) function you want
  */
QScriptValue getLazyLogicFunction(QScriptEngine*, const QString&);

/**
  * Functions for the JS function objects, config.exists and config.get.
  * Read scripting/IMPLIST for details on how they work
  */
QScriptValue configExists(QScriptContext*, QScriptEngine*);
QScriptValue getConfigValue(QScriptContext*, QScriptEngine*);

/**
  * Provide a config object to the given QScriptEngine depending
  * on the keys provided in the QVariant. The provided QVariant
  * MUST returns (true) on isHash()
  */
void supplyConfig(QScriptEngine*, const QVariant&);

/**
  * For engines whose scripts have no associated configuration.
  */
void supplyConfig(QScriptEngine*);

// RefWrapping may be used for objects who use pointer based scriptvalue
// storage, but do not use caching.
template <typename T>
void RefWrapping::embed(QScriptValue& value, const T datum)
    {
    QScriptEngine* eng = value.engine();
    value.setData(qScriptValueFromValue(eng,
                                        static_cast<void*>(
                                            const_cast<T>(datum)
                                        )
                                       ));
    }

template <typename T>
T RefWrapping::extract(const QScriptValue& value)
    {
    T datum = static_cast<T>(qscriptvalue_cast<void*>(value.data()));
    return datum;
    }
}
}

/**
  * Code linked from plasma for QTimer.
  */
QScriptValue constructTimerClass(QScriptEngine *eng);

#endif
