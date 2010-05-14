/*
 * Copyright (c) 2010 Ivan Cukic <ivan.cukic(at)kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef ACTIVITY_INFO_PH
#define ACTIVITY_INFO_PH

#include "activitymanager_interface.h"
#include "nepomukactivitiesservice_interface.h"
#include "kactivityinfo.h"

class KActivityInfo::Private {
public:
    Private(KActivityInfo *info, const QString &activityId);

    KUrl urlForType(KActivityInfo::ResourceType resourceType) const;
    void activityNameChanged(const QString &, const QString &) const;

    KActivityInfo *q;
    QString id;

    //TODO: reference counted
    static org::kde::nepomuk::services::NepomukActivitiesService * s_store;
    static org::kde::ActivityManager * s_manager;

    static org::kde::ActivityManager * manager();
};

#endif // ACTIVITY_INFO_PH
