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

#include <KDebug>

#include "kactivityinfo.h"
#include "kactivityinfo_p.h"

#include "nfo.h"
#include "nco.h"
#include "pimo.h"

// Private

org::kde::nepomuk::services::NepomukActivitiesService * KActivityInfo::Private::s_store = 0;
org::kde::ActivityManager * KActivityInfo::Private::s_manager = 0;

KActivityInfo::Private::Private(KActivityInfo *info, const QString &activityId)
    : q(info),
      id(activityId)
{
    if (!s_store) {
        s_store = new org::kde::nepomuk::services::NepomukActivitiesService(
                "org.kde.nepomuk.services.nepomukactivitiesservice",
                "/nepomukactivitiesservice",
                QDBusConnection::sessionBus()
                );
    }

    if (!s_manager) {
        s_manager = new org::kde::ActivityManager(
                "org.kde.ActivityManager",
                "/ActivityManager",
                QDBusConnection::sessionBus()
                );
    }
}

KUrl KActivityInfo::Private::urlForType(KActivityInfo::ResourceType resourceType) const
{
    switch (resourceType) {
        case KActivityInfo::DocumentResource:
            return Ontologies::nfo::Document();

        case KActivityInfo::FolderResource:
            return Ontologies::nfo::Folder();

        case KActivityInfo::ApplicationResource:
            return Ontologies::nfo::Application();

        case KActivityInfo::ContactResource:
            return Ontologies::nco::Contact();

        case KActivityInfo::LocationResource:
            return Ontologies::pimo::Location();

        default:
            return KUrl(QString());
    }
}

void KActivityInfo::Private::activityNameChanged(const QString &idChanged, const QString &toId) const
{
    if (idChanged == id) {
        emit q->nameChanged(toId);
    }
}

// KActivityInfo
KActivityInfo::KActivityInfo(const QString &activityId, QObject *parent)
    : QObject(parent),
      d(new Private(this, activityId))
{
    d->id = activityId;
    connect(Private::s_manager, SIGNAL(ActivityNameChanged(const QString &, const QString &)),
            this, SLOT(activityNameChanged(const QString &, const QString &)));
}

KActivityInfo::~KActivityInfo()
{
    delete d;
}

bool KActivityInfo::isValid() const
{
    return Private::s_manager->AvailableActivities().value().contains(d->id) ||
           Private::s_store->listAvailable().value().contains(d->id);
}

void KActivityInfo::associateResource(const KUrl & resource, ResourceType resourceType)
{
    associateResource(resource, d->urlForType(resourceType));
}

void KActivityInfo::associateResource(const KUrl & resource, const KUrl & resourceType)
{
    Private::s_store
        ->associateResource(d->id, resource.url(), resourceType.url());
}

void KActivityInfo::disassociateResource(const KUrl & resource)
{
    Private::s_store
        ->disassociateResource(d->id, resource.url());
}

QList < KUrl > KActivityInfo::associatedResources(ResourceType resourceType) const
{
    return associatedResources(d->urlForType(resourceType));
}

QList < KUrl > KActivityInfo::associatedResources(const KUrl & resourceType) const
{
    QList < KUrl > result;
    QStringList associatedResources = Private::s_store
        ->associatedResources(d->id, resourceType.url());

    foreach (const QString & uri, associatedResources) {
        result << KUrl(uri);
    }

    return result;
}

KUrl KActivityInfo::uri() const
{
    return KUrl(Private::s_store->uri(d->id));
}

KUrl KActivityInfo::resourceUri() const
{
    return KUrl(Private::s_store->resourceUri(d->id));
}

QString KActivityInfo::id() const
{
    return d->id;
}

QString KActivityInfo::name() const
{
    return Private::s_manager->ActivityName(d->id);
}

QString KActivityInfo::icon() const
{
    return Private::s_manager->ActivityIcon(d->id);
}

QString KActivityInfo::name(const QString & id)
{
    return Private::s_manager->ActivityName(id);
}

#include "kactivityinfo.moc"

