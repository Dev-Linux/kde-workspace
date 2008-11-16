/*****************************************************************************
 * Copyright (C) 2007-2008 by Shawn Starr <shawn.starr@rogers.com>           *
 * Copyright (C) 2008 by Teemu Rytilahti <tpr@d5k.net>                       *
 *                                                                           *
 * This library is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Library General Public               *
 * License as published by the Free Software Foundation; either              *
 * version 2 of the License, or (at your option) any later version.          *
 *                                                                           *
 * This library is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Library General Public License for more details.                          *
 *                                                                           *
 * You should have received a copy of the GNU Library General Public License *
 * along with this library; see the file COPYING.LIB.  If not, write to      *
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 * Boston, MA 02110-1301, USA.                                               *
 *****************************************************************************/

/* Meteorological formula class */

#ifndef _WEATHERUTILS_H
#define _WEATHERUTILS_H

#include "ion_export.h"

namespace WeatherUtils
{

// Enumerations for unit types
enum Unit { NoUnit = 0, Celsius, Fahrenheit, Kelvin, KilometersAnHour, MetersPerSecond, MilesAnHour, Kilopascals,
            InchesHG, Millibars, Hectopascals, Centimeters, Millimeters, Inches,
            Knots, Beaufort, Kilometers, Miles
          };

// Convert Units

/**
 * Convert from unit to another. See WeatherUtils::Unit for available units.
 * @param value float to convert
 * @param srcUnit from which unit to convert
 * @param destUnit to which unit to convert
 * @return converted value
*/
ION_EXPORT float convert(float value, int srcUnit, int destUnit);

/**
* Returns a string presentation of of WeatherUtils::Unit. Set plain to true in case you don't want a localized version of it.
 * @param unit unit to convert.
 * @param plain if true, returned string is not localized. defaults to false.
 * @return a string presentation of WeatherUtils::Unit. Empty string if called for invalid unit.
*/
ION_EXPORT QString getUnitString(int unit, bool plain = false);

/**
 * Converts wind/sun direction given in degrees to the nearest cardinal direction.
 * @param degrees wind direction in degrees.
 * @return a cardinal if available, empty string on error.
*/
ION_EXPORT QString degreesToCardinal(float degrees);

/**
 * @internal
*/
int knotsToBeaufort(float knots);
int milesToBeaufort(float miles);
int kilometersToBeaufort(float km);
int metersPerSecondToBeaufort(float ms);

} // WeatherUtils namespace

#endif
