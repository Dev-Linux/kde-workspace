/*
 *   Copyright (C) 2009 by Ana Cecília Martins <anaceciliamb@gmail.com>
 *   Copyright 2010 Chani Armitage <chani@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library/Lesser General Public License
 *   version 2, or (at your option) any later version, as published by the
 *   Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library/Lesser General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "filterbar.h"
#include "plasmaapp.h"

#include <QGraphicsLinearLayout>
#include <QTimer>

#include <klineedit.h>
#include <kmenu.h>
#include <kpushbutton.h>

#include <plasma/theme.h>
#include <plasma/corona.h>
#include <plasma/widgets/lineedit.h>
#include <plasma/widgets/pushbutton.h>
#include <Plasma/TabBar>

FilterBar::FilterBar(Qt::Orientation orientation, QGraphicsItem *parent)
    : QGraphicsWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    //init text search
    m_textSearch = new Plasma::LineEdit();

    m_textSearch->nativeWidget()->setClickMessage(i18n("Enter Search Term"));
    m_textSearch->setAttribute(Qt::WA_NoSystemBackground);
    m_textSearch->setClearButtonShown(true);
    connect(m_textSearch, SIGNAL(textChanged(QString)), this, SIGNAL(searchTermChanged(QString)));

    m_categoriesTabs = new Plasma::TabBar(this);
    connect(m_categoriesTabs, SIGNAL(currentChanged(int)), this, SIGNAL(filterChanged(int)));
    m_categoriesTabs->setAttribute(Qt::WA_NoSystemBackground);
    m_categoriesTabs->nativeWidget()->setUsesScrollButtons(true);
    m_categoriesTabs->addTab(i18n("Running"));
    m_categoriesTabs->addTab(i18n("Stopped"));

    m_newActivityButton = new Plasma::PushButton(this);
    m_newActivityButton->setText(i18n("New Activity..."));
    m_newActivityButton->setIcon(KIcon("list-add"));
    m_newActivityMenu = new KMenu();
    connect(m_newActivityMenu, SIGNAL(aboutToShow()), this, SLOT(populateActivityMenu()));
    connect(m_newActivityMenu, SIGNAL(triggered(QAction*)), this, SLOT(createActivity(QAction*)));
    m_newActivityButton->nativeWidget()->setMenu(m_newActivityMenu);
    //m_newWidgetsButton->setMinimumWidth(m_newWidgetsButton->effectiveSizeHint(Qt::PreferredSize).width());

    //layout
    m_linearLayout = new QGraphicsLinearLayout(this);
    m_linearLayout->addItem(m_textSearch);
    m_linearLayout->addItem(m_categoriesTabs);
    m_linearLayout->addItem(m_newActivityButton);

    setOrientation(orientation);
}

FilterBar::~FilterBar()
{
}

void FilterBar::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    Q_UNUSED(event)

    QSizeF contentsSize = m_linearLayout->contentsRect().size();
//    m_linearLayout->invalidate();
//    m_linearLayout->activate();

    if(m_orientation == Qt::Horizontal) {
        //don't let it occupy the whole layout width
        m_textSearch->setMaximumWidth(contentsSize.width()/6);
        m_textSearch->setMinimumWidth(contentsSize.width()/6);
    } else {
        //let it occupy the whole width
        m_textSearch->setMaximumWidth(-1);
        m_textSearch->setMinimumWidth(-1);
    }
}

void FilterBar::setOrientation(Qt::Orientation orientation)
{
    m_orientation = orientation;
    m_linearLayout->setOrientation(orientation);

    if (orientation == Qt::Horizontal) {
        m_textSearch->setPreferredWidth(200);
        m_textSearch->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        m_textSearch->setPreferredHeight(-1);
    } else {
        m_textSearch->setPreferredHeight(30);
        m_textSearch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_textSearch->setPreferredWidth(-1);
    }
}

void FilterBar::setMenuPos()
{
    QPoint position(0, 0);
    Plasma::Corona *corona = qobject_cast<Plasma::Corona*>(scene());
    if (corona) {
        position = corona->popupPosition(m_newActivityButton,
                m_newActivityMenu->geometry().size());
    }
    m_newActivityMenu->move(position);
}

//FIXME overzealous much?
void FilterBar::setFocus()
{
    QGraphicsWidget::setFocus();
    m_textSearch->setFocus();
    m_textSearch->nativeWidget()->setFocus();
}

void FilterBar::populateActivityMenu()
{
    QTimer::singleShot(0, this, SLOT(setMenuPos()));
    if (!m_newActivityMenu->actions().isEmpty()) {
        // already populated.
        return;
    }

    QAction *action = new QAction(i18n("Clone current activity"), this);
    m_newActivityMenu->addAction(action);

    KPluginInfo::List plugins = Plasma::Containment::listContainmentsOfType("desktop");
    foreach (const KPluginInfo& info, plugins) {
        action = m_newActivityMenu->addAction(KIcon(info.icon()), info.name());
        action->setData(info.pluginName());
    }

    /*TODO: populate with containment types + clone option
    QSignalMapper *mapper = new QSignalMapper(this);
    QObject::connect(mapper, SIGNAL(mapped(QString)), this, SLOT(createActivity(QString)));

    QAction *action = new QAction(KIcon("applications-internet"),
                                  i18n("Download New Plasma Widgets"), this);
    QObject::connect(action, SIGNAL(triggered(bool)), mapper, SLOT(map()));
    mapper->setMapping(action, QString());
    m_newWidgetsMenu->addAction(action);

    KService::List offers = KServiceTypeTrader::self()->query("Plasma/PackageStructure");
    foreach (const KService::Ptr &service, offers) {
        //kDebug() << service->property("X-Plasma-ProvidesWidgetBrowser");
        if (service->property("X-Plasma-ProvidesWidgetBrowser").toBool()) {
            QAction *action = new QAction(KIcon("applications-internet"),
                                          i18nc("%1 is a type of widgets, as defined by "
                                                "e.g. some plasma-packagestructure-*.desktop files",
                                                "Download New %1", service->name()), this);
            QObject::connect(action, SIGNAL(triggered(bool)), mapper, SLOT(map()));
            mapper->setMapping(action, service->property("X-KDE-PluginInfo-Name").toString());
            m_newWidgetsMenu->addAction(action);
        }
    }

    m_newWidgetsMenu->addSeparator();

    action = new QAction(KIcon("package-x-generic"),
                         i18n("Install Widget From Local File..."), this);
    QObject::connect(action, SIGNAL(triggered(bool)), this, SLOT(openWidgetFile()));
    */
}

void FilterBar::createActivity(QAction *action)
{
    QString type = action->data().toString();
    if (type.isEmpty()) {
        PlasmaApp::self()->cloneCurrentActivity();
    } else {
        PlasmaApp::self()->createActivity(type);
    }
}


