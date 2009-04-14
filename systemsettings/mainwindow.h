/**
 * This file is part of the System Settings package
 * Copyright (C) 2005 Benjamin C Meyer (ben+systempreferences at meyerhome dot net)
 *           (C) 2007 Will Stephenson <wstephenson@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// KDE
#include <KService>
#include <KXmlGuiWindow>

class QAbstractItemModel;
class QAction;
class QLabel;
class QModelIndex;
class QStackedWidget;
class KTabWidget;
class KAction;
class KCModuleModel;
class KCMultiWidget;
class KLineEdit;
class MenuItem;

class MainWindow : public KXmlGuiWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent=0);
	~MainWindow();

    /**
     * Called when the mainwindow is closed.
     */
    virtual bool queryClose();

protected:
    virtual QSize sizeHint() const;

private slots:
    void selectionChanged( const QModelIndex & selected );
    void updateSearchHits();
    void toolBarIconSizeChanged();
	void showOverview();
	void widgetChange();

private:
    void readMenu( MenuItem * );

	KCModuleModel *model;
	const KService::List categories;
	const KService::List modules;
	MenuItem * rootItem;
	QStackedWidget *windowStack;
	KTabWidget *moduleTabs;
    KLineEdit * search;
    QLabel *searchIcon;

	KCMultiWidget *groupWidget;

	QHash<KService::Ptr,KCMultiWidget*> moduleItemToWidgetDict;
	QHash<const QAbstractItemModel *,int> modelToTabHash;

	KAction *showOverviewAction;
	KAction *searchText;
	KAction *searchClear;
	KAction *searchAction;

	void buildMainWidget();
	void buildActions();

	QLabel *generalHitLabel;
	QLabel *advancedHitLabel;
};

bool pageLessThan( MenuItem *page1, MenuItem *page2 );

#endif

