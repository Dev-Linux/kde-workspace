/*
 *   Copyright 2009 by Artur Duque de Souza <asouza@kde.org>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2,
 *   or (at your option) any later version.
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

#include "stripwidget.h"

#include <QGraphicsGridLayout>
#include <QToolButton>
#include <QAction>
#include <QTimer>

#include <KIcon>
#include <KIconLoader>

#include <Plasma/Frame>
#include <Plasma/ToolButton>
#include <Plasma/IconWidget>
#include <Plasma/QueryMatch>
#include <Plasma/AbstractRunner>
#include <Plasma/RunnerManager>
#include <Plasma/ScrollWidget>
#include <Plasma/ToolTipContent>
#include <Plasma/ToolTipManager>

#include "itemview.h"

StripWidget::StripWidget(Plasma::RunnerManager *rm, QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
      m_runnermg(rm),
      m_itemView(0),
      m_offset(0),
      m_startupCompleted(false)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_arrowsLayout = new QGraphicsLinearLayout(this);
    m_arrowsLayout->setContentsMargins(0, 0, 0, 0);
    setFocusPolicy(Qt::StrongFocus);

    m_leftArrow = new Plasma::ToolButton(this);
    m_leftArrow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_leftArrow->setPreferredWidth(KIconLoader::SizeMedium);
    m_leftArrow->setImage("widgets/arrows", "left-arrow");
    connect(m_leftArrow, SIGNAL(clicked()), this, SLOT(goLeft()));
    connect(m_leftArrow, SIGNAL(pressed()), this, SLOT(scrollTimeout()));

    m_rightArrow = new Plasma::ToolButton(this);
    m_rightArrow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_rightArrow->setPreferredWidth(KIconLoader::SizeMedium);
    m_rightArrow->setImage("widgets/arrows", "right-arrow");
    connect(m_rightArrow, SIGNAL(clicked()), this, SLOT(goRight()));
    connect(m_rightArrow, SIGNAL(pressed()), this, SLOT(scrollTimeout()));

    m_leftArrow->setEnabled(false);
    m_rightArrow->setEnabled(false);
    m_leftArrow->hide();
    m_rightArrow->hide();

    m_itemView = new ItemView(this);
    m_itemView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_itemView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_itemView->installEventFilter(this);
    m_itemView->setOrientation(Qt::Horizontal);
    m_itemView->setIconSize(KIconLoader::SizeLarge);

    connect(m_itemView, SIGNAL(itemActivated(Plasma::IconWidget *)), this, SLOT(launchFavourite(Plasma::IconWidget *)));
    connect(m_itemView, SIGNAL(scrollBarsNeededChanged(ItemView::ScrollBarFlags)), this, SLOT(arrowsNeededChanged(ItemView::ScrollBarFlags)));

    m_arrowsLayout->addItem(m_leftArrow);
    m_arrowsLayout->addItem(m_itemView);
    m_arrowsLayout->addItem(m_rightArrow);

    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setSingleShot(false);
    connect(m_scrollTimer, SIGNAL(timeout()), this, SLOT(scrollTimeout()));

    m_setCurrentTimer = new QTimer(this);
    m_setCurrentTimer->setSingleShot(false);
    connect(m_setCurrentTimer, SIGNAL(timeout()), this, SLOT(highlightCurrentItem()));
}

StripWidget::~StripWidget()
{
    foreach(Plasma::QueryMatch *match, m_favouritesMatches) {
        delete match;
    }
}

void StripWidget::createIcon(Plasma::QueryMatch *match, int idx)
{
    Q_UNUSED(idx)
    // create new IconWidget for favourite strip

    Plasma::IconWidget *fav = m_itemView->createItem();
    fav->hide();
    fav->setTextBackgroundColor(QColor());
    fav->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    fav->installEventFilter(this);
    fav->setText(match->text());
    fav->setIcon(match->icon());

    Plasma::ToolTipContent toolTipData = Plasma::ToolTipContent();
    toolTipData.setAutohide(true);
    toolTipData.setMainText(match->text());
    toolTipData.setSubText(match->subtext());
    toolTipData.setImage(match->icon());

    Plasma::ToolTipManager::self()->registerWidget(this);
    Plasma::ToolTipManager::self()->setContent(fav, toolTipData);

    connect(fav, SIGNAL(activated()), this, SLOT(launchFavourite()));

    // set an action to be able to remove from favourites
    QAction *action = new QAction(fav);
    action->setIcon(KIcon("list-remove"));
    fav->addIconAction(action);
    connect(action, SIGNAL(triggered()), this, SLOT(removeFavourite()));

    m_favouritesIcons.insert(fav, match);
    m_itemView->insertItem(fav, -1);

    if (m_startupCompleted) {
        m_itemView->setCurrentItem(fav);
        m_setCurrentTimer->start(300);
    }
}

void StripWidget::highlightCurrentItem()
{
    m_itemView->setCurrentItem(m_itemView->currentItem());
}

void StripWidget::add(Plasma::QueryMatch match, const QString &query)
{
    // add to layout and data structures
    Plasma::QueryMatch *newMatch = new Plasma::QueryMatch(match);
    m_favouritesMatches.append(newMatch);
    m_favouritesQueries.insert(newMatch, query);

    int idx = m_itemView->count();
    createIcon(newMatch, idx);
}

void StripWidget::remove(Plasma::IconWidget *favourite)
{
    Plasma::QueryMatch *match = m_favouritesIcons.value(favourite);
    m_favouritesMatches.removeOne(match);
    m_favouritesQueries.remove(match);
    m_favouritesIcons.remove(favourite);

    // must be deleteLater because the IconWidget will return from the action?
    favourite->deleteLater();
    delete match;
}

void StripWidget::removeFavourite()
{
    Plasma::IconWidget *icon = qobject_cast<Plasma::IconWidget*>(sender()->parent());

    if (icon) {
        remove(icon);
    }
}

void StripWidget::launchFavourite()
{
    Plasma::IconWidget *icon = static_cast<Plasma::IconWidget*>(sender());
    Plasma::QueryMatch *match = m_favouritesIcons.value(icon);

    Plasma::RunnerContext context;
    context.setQuery(m_favouritesQueries.value(match));
    match->run(context);
}

void StripWidget::launchFavourite(Plasma::IconWidget *icon)
{
    Plasma::QueryMatch *match = m_favouritesIcons.value(icon);

    Plasma::RunnerContext context;
    context.setQuery(m_favouritesQueries.value(match));
    match->run(context);
}

void StripWidget::goRight()
{
    QRectF rect(m_itemView->boundingRect());
    rect.moveLeft(rect.right() - m_itemView->widget()->pos().x());
    rect.setWidth(rect.width()/4);

    m_itemView->ensureRectVisible(rect);
}

void StripWidget::goLeft()
{
    QRectF rect(m_itemView->boundingRect());
    rect.setWidth(rect.width()/4);
    rect.moveRight(- m_itemView->widget()->pos().x());

    m_itemView->ensureRectVisible(rect);
}

void StripWidget::scrollTimeout()
{
    if (!m_scrollTimer->isActive()) {
        m_scrollTimer->start(250);
    } else if (m_leftArrow->isDown()) {
        goLeft();
    } else if (m_rightArrow->isDown()) {
        goRight();
    } else {
        m_scrollTimer->stop();
    }
}

void StripWidget::save(KConfigGroup &cg)
{
    kDebug() << "----------------> Saving Stuff...";

    // erase the old stuff before saving the new one
    KConfigGroup oldGroup(&cg, "stripwidget");
    oldGroup.deleteGroup();

    KConfigGroup stripGroup(&cg, "stripwidget");

    int id = 0;
    foreach(Plasma::QueryMatch *match, m_favouritesMatches) {
        // Write now just saves one for tests. Later will save
        // all the strip
        KConfigGroup config(&stripGroup, QString("favourite-%1").arg(id));
        config.writeEntry("runnerid", match->runner()->id());
        config.writeEntry("query", m_favouritesQueries.value(match));
        config.writeEntry("matchId", match->id());
        ++id;
    }
}

void StripWidget::restore(KConfigGroup &cg)
{
    kDebug() << "----------------> Restoring Stuff...";

    KConfigGroup stripGroup(&cg, "stripwidget");

    // get all the favourites
    QStringList groupNames(stripGroup.groupList());
    qSort(groupNames);
    QMap<uint, KConfigGroup> favouritesConfigs;
    foreach (const QString &favouriteGroup, stripGroup.groupList()) {
        if (favouriteGroup.startsWith("favourite-")) {
            KConfigGroup favouriteConfig(&stripGroup, favouriteGroup);
            favouritesConfigs.insert(favouriteGroup.split("-").last().toUInt(), favouriteConfig);
        }
    }

    QVector<QString> runnerIds;
    QVector<QString> queries;
    QVector<QString> matchIds;

    if (favouritesConfigs.isEmpty()) {
        runnerIds.resize(4);
        queries.resize(4);
        matchIds.resize(4);
        matchIds << "services_kde-konqbrowser.desktop" << "services_kde4-KMail.desktop" << "services_kde4-systemsettings.desktop" << "places";
        queries << "konqueror" << "kmail" << "systemsettings" << "home";
        runnerIds << "services" << "services" << "services" << "places";
    } else {
        runnerIds.resize(favouritesConfigs.size());
        queries.resize(favouritesConfigs.size());
        matchIds.resize(favouritesConfigs.size());
        QMap<uint, KConfigGroup>::const_iterator it = favouritesConfigs.constBegin();
        int i = 0;
        while (it != favouritesConfigs.constEnd()) {
            KConfigGroup favouriteConfig = it.value();

            runnerIds[i] = favouriteConfig.readEntry("runnerid");
            queries[i] = favouriteConfig.readEntry("query");
            matchIds[i] = favouriteConfig.readEntry("matchId");
            ++i;
            ++it;
        }
    }

    QString currentQuery;
    for (int i = 0; i < queries.size(); ++i ) {
        // perform the query
        if (currentQuery == queries[i] || m_runnermg->execQuery(queries[i], runnerIds[i])) {
            currentQuery = queries[i];
            // find our match
            Plasma::QueryMatch match(m_runnermg->searchContext()->match(matchIds[i]));

            // we should verify some other saved information to avoid putting the
            // wrong item if the search result is different!
            if (match.isValid()) {
                add(match, queries[i]);
            }
        }
    }

    m_startupCompleted = true;
}

void StripWidget::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event)

    m_itemView->setFocus();
}

void StripWidget::arrowsNeededChanged(ItemView::ScrollBarFlags flags)
{
    bool leftNeeded = false;
    bool rightNeeded = false;

    if (flags & ItemView::HorizontalScrollBar) {
        leftNeeded = m_itemView->scrollPosition().x() > 0;
        rightNeeded = m_itemView->contentsSize().width() - m_itemView->scrollPosition().x() > m_itemView->size().width();
    }

    m_leftArrow->setEnabled(leftNeeded);
    m_rightArrow->setEnabled(rightNeeded);
    m_leftArrow->setVisible(leftNeeded|rightNeeded);
    m_rightArrow->setVisible(leftNeeded|rightNeeded);
    m_arrowsLayout->invalidate();
}

