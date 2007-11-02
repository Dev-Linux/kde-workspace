/*
 *   Copyright (C) 2006 Aaron Seigo <aseigo@kde.org>
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

#include "servicerunner.h"

#include <QWidget>
#include <KIcon>

#include <KLocale>
#include <KRun>
#include <KActionCollection>
#include <KService>
#include <KServiceType>
#include <KServiceTypeTrader>

QString formattedName( KService::Ptr service )
{
    QString name = service->name();

    if ( !service->genericName().isEmpty() ) {
        name = i18n( "%1 - %2", name, service->genericName() );
    } else if ( !service->comment().isEmpty() ) {
        name = i18n( "%1 - %2", name, service->comment() );
    }

    return name;
}

ServiceRunner::ServiceRunner( QObject* parent )
    : Plasma::AbstractRunner( parent )
{
    setObjectName(i18n("Application"));
}

ServiceRunner::~ServiceRunner()
{
}

void ServiceRunner::match(Plasma::SearchContext *search)
{
    QString term = search->searchTerm();
    if (term.length() <  3) {
        return;
    }

    KService::Ptr service = KService::serviceByName(term);

    if (service && !service->exec().isEmpty()) {
        setupAction(service, search->addExactMatch(this));
    }

    QString query = QString("exist Exec and ('%1' ~in Keywords or '%2' ~~ GenericName or '%3' ~~ Name) and Name != '%4'").arg(term, term, term, term);
    const KService::List services = KServiceTypeTrader::self()->query("Application", query);

    //kDebug() << "got " << services.count() << " services from " << query;

    foreach (const KService::Ptr service, services) {
        setupAction(service, search->addPossibleMatch(this));
    }
}

void ServiceRunner::exec(Plasma::SearchAction* action)
{
    KService::Ptr service = KService::serviceByStorageId(action->data().toString());
    if (service) {
        KRun::run(*service, KUrl::List(), 0);
    }
}

void ServiceRunner::setupAction(const KService::Ptr &service, QAction *action)
{
    action->setText(service->name());
    action->setData(service->storageId());

    if (service->icon().isEmpty()) {
        action->setIcon(KIcon(service->icon()));
    }
}

#include "servicerunner.moc"

