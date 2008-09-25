/***************************************************************************
 *   Copyright (C) 2007-2008 by Shawn Starr <shawn.starr@rogers.com>       *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA          *
 ***************************************************************************/

#ifndef _ION_H
#define _ION_H

#include <QtCore/QObject>
#include <KDE/KGenericFactory>
#include <plasma/dataengine.h>

#include "ion_export.h"

/**
* @author Shawn Starr
* This is the base class to be used to implement new ions for the WeatherEngine.
* The idea is that you can have multiple ions which provide weather information from different services to the engine from which an applet will request the data from.
*
* Basically an ion is a Plasma::DataEngine, which is queried by the WeatherEngine instead of some applet.
*/
class ION_EXPORT IonInterface : public Plasma::DataEngine
{
    Q_OBJECT

public:

    enum ConditionIcons { ClearDay, FewCloudsDay, PartlyCloudyDay, Overcast,
                          Showers, ScatteredShowers, Thunderstorm, Snow,
                          FewCloudsNight, PartlyCloudyNight, ClearNight,
                          Mist, NotAvailable };

    typedef QHash<QString, IonInterface*> IonDict; // Define Dict as a QHash for Ions

    /**
     * Constructor for the ion
     * @param parent The parent object.
     */
    explicit IonInterface(QObject *parent = 0, const QVariantList &args = QVariantList());
    /**
     * Destructor for the ion
     */
    virtual ~IonInterface() {}

    /**
     * Increment ion counter. This is used to check if the ion is being used.
     */
    void ref();

    /**
     * Decrement ion counter. Called when ion is destroyed/unloaded.
     */
    void deref();

    /**
     * Returns whether the ion is being used.
     * @return true if the ion is being used, false otherwise
     */
    bool isUsed() const;

    /**
     * Returns weather icon filename to display in applet.
     * @param conditionList a QList map pair of icons mapped to a enumeration of conditions.
     * @param condition the current condition being reported.
     * @return icon name
     */
    QString getWeatherIcon(const QMap<QString, ConditionIcons> &ConditionList, const QString& condition);
  
public Q_SLOTS:

    /**
     * Reimplemented from Plasma::DataEngine
     * @param source the name of the datasource to be updated
     */
    bool updateSourceEvent(const QString& source);

protected:
    /**
     * Call this method to flush waiting source requests that may be pending
     * initialization
     *
     * @arg initialized whether or not the ion is currently ready to fetch data
     */
    void setInitialized(bool initialized);

    /**
     * Reimplemented from Plasma::DataEngine
     * @param source The datasource being requested
     */
    bool sourceRequestEvent(const QString &source);

    /**
     * Reimplement to fetch the data from the ion.
     * @arg source the name of the datasource.
     * @return true if update was successful, false if failed
     */
    virtual bool updateIonSource(const QString &source) = 0;

    friend class WeatherEngine;

private:
    class Private;
    Private* const d;
};

#define K_EXPORT_PLASMA_ION(name, classname) \
    K_PLUGIN_FACTORY(factory, registerPlugin<classname>();) \
    K_EXPORT_PLUGIN(factory("ion_" #name))
#endif
