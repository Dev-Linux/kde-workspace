/*
 * Copyright (c) 2000 Yves Arrouye <yves@realnames.com>
 * Copyright (c) 2002, 2003 Dawit Alemayehu <adawit@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __IKWSOPTS_H___
#define __IKWSOPTS_H___

#include <qlayout.h>
#include <qtabwidget.h>

#include <kcmodule.h>
#include <kservice.h>

class FilterOptionsUI;
class SearchProvider;
class SearchProviderItem;

class FilterOptions : public KCModule
{
    Q_OBJECT

public:
    FilterOptions(KInstance *instance, QWidget *parent = 0, const char *name = 0);

    void load();
    void save();
    void defaults();
    QString quickHelp() const;

protected slots:
    void moduleChanged();

    void setAutoWebSearchState();
    void setWebShortcutState();

    void addSearchProvider();
    void changeSearchProvider();
    void deleteSearchProvider();
    void updateSearchProvider();

private:
    SearchProviderItem *displaySearchProvider(SearchProvider *p, bool fallback = false);

    void setDelimiter (char);
    char delimiter ();

    // The names of the providers that the user deleted,
    // these are marked as deleted in the user's homedirectory
    // on save if a global service file exists for it.
    QStringList m_deletedProviders;
    QMap <QString, QString> m_defaultEngineMap;

    FilterOptionsUI* m_dlg;
};

#endif
