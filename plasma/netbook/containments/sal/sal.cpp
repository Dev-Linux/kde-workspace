/*
 *   Copyright 2009 by Aaron Seigo <aseigo@kde.org>
 *   Copyright 2009 by Artur Duque de Souza <asouza@kde.org>
 *   Copyright 2009 by Marco Martin <notmart@gmail.com>
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

#include "sal.h"
#include "stripwidget.h"
#include "itemview.h"
#include "runnersconfig.h"
#include "../common/linearappletoverlay.h"
#include "../common/appletmovespacer.h"
#include "../common/nettoolbox.h"
#include "iconactioncollection.h"

#include <QAction>
#include <QTimer>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsAnchorLayout>
#include <QApplication>

#include <KAction>
#include <KDebug>
#include <KIcon>
#include <KIconLoader>
#include <KLineEdit>
#include <KStandardDirs>
#include <KService>
#include <KServiceTypeTrader>
#include <KConfigDialog>

#include <Plasma/Theme>
#include <Plasma/Frame>
#include <Plasma/Corona>
#include <Plasma/LineEdit>
#include <Plasma/IconWidget>
#include <Plasma/RunnerManager>
#include <Plasma/QueryMatch>
#include <Plasma/ScrollWidget>
#include <Plasma/ToolButton>
#include <Plasma/ToolTipContent>
#include <Plasma/ToolTipManager>


SearchLaunch::SearchLaunch(QObject *parent, const QVariantList &args)
    : Containment(parent, args),
      m_backButton(0),
      m_queryCounter(0),
      m_maxColumnWidth(0),
      m_searchField(0),
      m_resultsView(0),
      m_orientation(Qt::Vertical),
      m_leftArrow(0),
      m_rightArrow(0),
      m_firstItem(0),
      m_appletsLayout(0),
      m_appletOverlay(0),
      m_iconActionCollection(0),
      m_stripUninitialized(true)
{
    setContainmentType(Containment::CustomContainment);
    m_iconActionCollection = new IconActionCollection(this, this);
    setHasConfigurationInterface(true);
    setFocusPolicy(Qt::StrongFocus);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    m_background = new Plasma::FrameSvg(this);
    m_background->setImagePath("widgets/frame");
    m_background->setElementPrefix("raised");
    m_background->setEnabledBorders(Plasma::FrameSvg::BottomBorder);
}

SearchLaunch::~SearchLaunch()
{
    KConfigGroup cg = config();
    m_stripWidget->save(cg);
    config().writeEntry("orientation", (int)m_orientation);

    delete m_runnermg;
}

void SearchLaunch::init()
{
    Containment::init();
    connect(this, SIGNAL(appletAdded(Plasma::Applet*,QPointF)),
            this, SLOT(layoutApplet(Plasma::Applet*,QPointF)));
    connect(this, SIGNAL(appletRemoved(Plasma::Applet*)),
            this, SLOT(appletRemoved(Plasma::Applet*)));

    connect(this, SIGNAL(toolBoxVisibilityChanged(bool)), this, SLOT(updateConfigurationMode(bool)));

    m_runnermg = new Plasma::RunnerManager(this);
    m_runnermg->reloadConfiguration();
    connect(m_runnermg, SIGNAL(matchesChanged(const QList<Plasma::QueryMatch>&)),
            this, SLOT(setQueryMatches(const QList<Plasma::QueryMatch>&)));

    m_toolBox = new NetToolBox(this);
    setToolBox(m_toolBox);
    connect(m_toolBox, SIGNAL(toggled()), this, SIGNAL(toolBoxToggled()));
    connect(m_toolBox, SIGNAL(visibilityChanged(bool)), this, SIGNAL(toolBoxVisibilityChanged(bool)));
    m_toolBox->show();

    QAction *a = action("add widgets");
    if (a) {
        m_toolBox->addTool(a);
    }

    a = action("configure");
    if (a) {
        m_toolBox->addTool(a);
        a->setText(i18n("Configure Search and Launch"));
    }

    KAction *lockAction = new KAction(this);
    addAction("lock page", lockAction);
    lockAction->setText(i18n("Lock Page"));
    lockAction->setIcon(KIcon("object-locked"));
    QObject::connect(lockAction, SIGNAL(triggered(bool)), this, SLOT(toggleImmutability()));
    m_toolBox->addTool(lockAction);

    KService::List services = KServiceTypeTrader::self()->query("Plasma/Sal/Menu");
    if (!services.isEmpty()) {
        foreach (const KService::Ptr &service, services) {
            const QString query = service->property("X-Plasma-Sal-Query", QVariant::String).toString();
            const QString runner = service->property("X-Plasma-Sal-Runner", QVariant::String).toString();
            const int relevance = service->property("X-Plasma-Sal-Relevance", QVariant::Int).toInt();

            Plasma::QueryMatch match(0);
            match.setType(Plasma::QueryMatch::ExactMatch);
            match.setRelevance(relevance);
            match.setId(query);
            match.setIcon(KIcon(service->icon()));
            match.setText(service->name());
            match.setSubtext(service->comment());
            QStringList data;
            data << query << runner;
            match.setData(data);

            m_defaultMatches.append(match);
        }
    }

    //SAL doesn't want to be removed:
    //FIXME: wise to do it here?
    a = action("remove");
    delete a;

    a = new QAction(i18n("Next activity"), this);
    addAction("next containment", a);
    a = new QAction(i18n("Previous activity"), this);
    addAction("previous containment", a);
}

void SearchLaunch::configChanged()
{
    setOrientation((Qt::Orientation)config().readEntry("orientation", (int)Qt::Vertical));

    m_stripWidget->setIconSize(config().readEntry("FavouritesIconSize", (int)KIconLoader::SizeLarge));

    m_resultsView->setIconSize(config().readEntry("ResultsIconSize", (int)KIconLoader::SizeHuge));
}

void SearchLaunch::toggleImmutability()
{
    if (immutability() == Plasma::UserImmutable) {
        setImmutability(Plasma::Mutable);
    } else if (immutability() == Plasma::Mutable) {
        setImmutability(Plasma::UserImmutable);
    }
}

void SearchLaunch::doSearch(const QString &query, const QString &runner)
{
    m_queryCounter = 0;
    m_firstItem = 0;
    m_resultsView->clear();

    const bool stillEmpty = query.isEmpty() && m_runnermg->query().isEmpty();
    if (!stillEmpty) {
        m_resultsView->clear();
    }

    m_maxColumnWidth = 0;
    m_matches.clear();

    m_runnermg->launchQuery(query, runner);

    if (m_resultsView && query.isEmpty()) {
        if (m_resultsView->count()) {
            m_resultsView->clear();
        }

        setQueryMatches(m_defaultMatches);
        m_backButton->hide();
        m_resultsView->setDragAndDropMode(ItemContainer::NoDragAndDrop);
    } else if (m_backButton) {
        m_backButton->show();
        if (immutability() == Plasma::Mutable) {
            m_resultsView->setDragAndDropMode(ItemContainer::CopyDragAndDrop);
        } else {
            m_resultsView->setDragAndDropMode(ItemContainer::NoDragAndDrop);
        }
    }
}

void SearchLaunch::reset()
{
    if (!m_runnermg->query().isEmpty()) {
        m_searchField->setText(QString());
        doSearch(QString());
    }
}

void SearchLaunch::setQueryMatches(const QList<Plasma::QueryMatch> &matches)
{
    if (matches.isEmpty()) {
        return;
    }

    // just add new QueryMatch

    foreach (Plasma::QueryMatch match, matches) {

        // create new IconWidget with information from the match
        Plasma::IconWidget *icon = m_resultsView->createItem();
        icon->setNumDisplayLines(4);
        icon->hide();
        icon->setOrientation(Qt::Vertical);
        icon->setText(!match.text().isEmpty()?match.text():match.subtext());
        icon->setIcon(match.icon());
        Plasma::ToolTipContent toolTipData = Plasma::ToolTipContent();
        toolTipData.setAutohide(true);
        toolTipData.setMainText(match.text());
        toolTipData.setSubText(match.subtext());
        toolTipData.setImage(match.icon());

        Plasma::ToolTipManager::self()->registerWidget(this);
        Plasma::ToolTipManager::self()->setContent(icon, toolTipData);

        m_resultsView->insertItem(icon, 1/match.relevance());

        if (match.relevance() < 0.4) {
            icon->setOrientation(Qt::Horizontal);
            icon->setMinimumSize(icon->sizeFromIconSize(KIconLoader::SizeSmall));
            icon->setMaximumSize(icon->sizeFromIconSize(KIconLoader::SizeSmall));
        }

        connect(icon, SIGNAL(activated()), this, SLOT(launch()));

        if (!m_runnermg->searchContext()->query().isEmpty()) {
            // create action to add to favourites strip
            QAction *action = new QAction(icon);
            action->setIcon(KIcon("favorites"));
            icon->addIconAction(action);
            m_iconActionCollection->addAction(action);
            connect(action, SIGNAL(triggered()), this, SLOT(addFavourite()));
        }

        // add to data structure
        m_matches.insert(icon, match);
        if (!m_firstItem || match.relevance() > m_matches.value(m_firstItem, Plasma::QueryMatch(0)).relevance()) {
            m_firstItem = icon;
        }
    }
    m_queryCounter = matches.count();
}

void SearchLaunch::launch(Plasma::IconWidget *icon)
{
    Plasma::QueryMatch match = m_matches.value(icon, Plasma::QueryMatch(0));
    if (m_runnermg->searchContext()->query().isEmpty()) {
        QStringList data = match.data().value<QStringList>();

        if (data.count() == 2) {
            doSearch(data.first(), data.last());
        } else {
            doSearch(data.first());
        }
        m_lastQuery = data.first();
    } else {
        m_runnermg->run(match);
        reset();
    }
}

void SearchLaunch::launch()
{
    Plasma::IconWidget *icon = static_cast<Plasma::IconWidget*>(sender());
    launch(icon);
}

void SearchLaunch::addFavourite()
{
    Plasma::IconWidget *icon = static_cast<Plasma::IconWidget*>(sender()->parent());
    Plasma::QueryMatch match = m_matches.value(icon, Plasma::QueryMatch(0));
    m_stripWidget->add(match, m_runnermg->searchContext()->query());
}

void SearchLaunch::layoutApplet(Plasma::Applet* applet, const QPointF &pos)
{
    Q_UNUSED(pos);

    if (!m_appletsLayout) {
        return;
    }

    if (m_appletsLayout->count() == 2) {
        m_mainLayout->removeItem(m_appletsLayout);
        m_mainLayout->addItem(m_appletsLayout);
    }

    Plasma::FormFactor f = formFactor();
    int insertIndex = -1;

    //if pos is (-1,-1) insert at the end of the panel
    if (pos != QPoint(-1, -1)) {
        for (int i = 1; i < m_appletsLayout->count()-1; ++i) {
            if (!dynamic_cast<Plasma::Applet *>(m_appletsLayout->itemAt(i)) &&
                !dynamic_cast<AppletMoveSpacer *>(m_appletsLayout->itemAt(i))) {
                continue;
            }

            QRectF siblingGeometry = m_appletsLayout->itemAt(i)->geometry();
            if (f == Plasma::Horizontal) {
                qreal middle = (siblingGeometry.left() + siblingGeometry.right()) / 2.0;
                if (pos.x() < middle) {
                    insertIndex = i;
                    break;
                } else if (pos.x() <= siblingGeometry.right()) {
                    insertIndex = i + 1;
                    break;
                }
            } else { // Plasma::Vertical
                qreal middle = (siblingGeometry.top() + siblingGeometry.bottom()) / 2.0;
                if (pos.y() < middle) {
                    insertIndex = i;
                    break;
                } else if (pos.y() <= siblingGeometry.bottom()) {
                    insertIndex = i + 1;
                    break;
                }
            }
        }
    }

    if (insertIndex != -1) {
        m_appletsLayout->insertItem(insertIndex, applet);
    } else {
        m_appletsLayout->insertItem(m_appletsLayout->count()-1, applet);
    }

    applet->setBackgroundHints(NoBackground);
    connect(applet, SIGNAL(sizeHintChanged(Qt::SizeHint)), this, SLOT(updateSize()));
}

void SearchLaunch::appletRemoved(Plasma::Applet* applet)
{
    Q_UNUSED(applet)

    if (!m_appletOverlay && m_appletsLayout->count() == 3) {
        m_mainLayout->removeItem(m_appletsLayout);
    }
}

void SearchLaunch::updateSize()
{
    Plasma::Applet *applet = qobject_cast<Plasma::Applet *>(sender());

    if (applet) {
        if (formFactor() == Plasma::Horizontal) {
            const int delta = applet->preferredWidth() - applet->size().width();
            //setting the preferred width when delta = 0 and preferredWidth() < minimumWidth()
            // leads to the same thing as setPreferredWidth(minimumWidth())
            if (delta != 0) {
                setPreferredWidth(preferredWidth() + delta);
            }
        } else if (formFactor() == Plasma::Vertical) {
            const int delta = applet->preferredHeight() - applet->size().height();
            if (delta != 0) {
                setPreferredHeight(preferredHeight() + delta);
            }
        }

        resize(preferredSize());
    }
}


void SearchLaunch::constraintsEvent(Plasma::Constraints constraints)
{
    kDebug() << "constraints updated with" << constraints << "!!!!!!";

    if (constraints & Plasma::FormFactorConstraint ||
        constraints & Plasma::StartupCompletedConstraint) {

        Plasma::FormFactor form = formFactor();

        Qt::Orientation layoutOtherDirection = form == Plasma::Vertical ? \
            Qt::Horizontal : Qt::Vertical;


        // create our layout!
        if (!layout()) {
            // create main layout
            m_mainLayout = new QGraphicsLinearLayout();
            m_mainLayout->setOrientation(layoutOtherDirection);
            m_mainLayout->setContentsMargins(0, 0, 0, 0);
            m_mainLayout->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,
                                                    QSizePolicy::Expanding));
            setLayout(m_mainLayout);

            // create launch grid and make it centered
            m_resultsLayout = new QGraphicsLinearLayout;


            m_resultsView = new ItemView(this);

            m_resultsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_resultsView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            m_resultsLayout->addItem(m_resultsView);

            connect(m_resultsView, SIGNAL(itemActivated(Plasma::IconWidget *)), this, SLOT(launch(Plasma::IconWidget *)));
            connect(m_resultsView, SIGNAL(itemDroppedOutside(Plasma::IconWidget *)), this, SLOT(resultsViewRequestedDrop(Plasma::IconWidget *)));

            m_stripWidget = new StripWidget(m_runnermg, this);
            m_stripWidget->setImmutability(immutability());

            //load all config, only at this point we are sure it won't crash
            configChanged();

            m_appletsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
            m_appletsLayout->setPreferredHeight(KIconLoader::SizeMedium);
            m_appletsLayout->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            QGraphicsWidget *leftSpacer = new QGraphicsWidget(this);
            QGraphicsWidget *rightSpacer = new QGraphicsWidget(this);
            m_appletsLayout->addItem(leftSpacer);
            m_appletsLayout->addItem(rightSpacer);

            m_backButton = new Plasma::IconWidget(this);
            m_backButton->setIcon(KIcon("go-previous"));
            m_backButton->setText(i18n("Back"));
            m_backButton->setOrientation(Qt::Horizontal);
            m_backButton->setPreferredSize(m_backButton->sizeFromIconSize(KIconLoader::SizeSmallMedium));
            connect(m_backButton, SIGNAL(activated()), this, SLOT(reset()));
            connect(m_resultsView, SIGNAL(resetRequested()), this, SLOT(reset()));

            QGraphicsAnchorLayout *searchLayout = new QGraphicsAnchorLayout();
            searchLayout->setSpacing(10);

            m_searchField = new Plasma::LineEdit(this);
            m_searchField->setPreferredWidth(200);
            m_searchField->nativeWidget()->setClearButtonShown(true);
            m_searchField->nativeWidget()->setClickMessage(i18n("Enter your query here"));
            connect(m_searchField, SIGNAL(returnPressed()), this, SLOT(searchReturnPressed()));
            connect(m_searchField->nativeWidget(), SIGNAL(textEdited(const QString &)), this, SLOT(delayedQuery()));
            m_searchTimer = new QTimer(this);
            m_searchTimer->setSingleShot(true);
            connect(m_searchTimer, SIGNAL(timeout()), this, SLOT(query()));
            searchLayout->addAnchor(m_searchField, Qt::AnchorHorizontalCenter, searchLayout, Qt::AnchorHorizontalCenter);
            searchLayout->addAnchors(m_searchField, searchLayout, Qt::Vertical);
            searchLayout->addAnchors(m_backButton, searchLayout, Qt::Vertical);
            searchLayout->addAnchor(m_backButton, Qt::AnchorRight, m_searchField, Qt::AnchorLeft);


            // add our layouts to main vertical layout
            m_mainLayout->addItem(m_stripWidget);
            m_mainLayout->addItem(searchLayout);
            m_mainLayout->addItem(m_resultsLayout);


            // correctly set margins
            m_mainLayout->activate();
            m_mainLayout->updateGeometry();

            setTabOrder(m_stripWidget, m_searchField);
            setTabOrder(m_searchField, m_resultsView);
            setFormFactorFromLocation(location());
        }
    }

    if (constraints & Plasma::LocationConstraint) {
        setFormFactorFromLocation(location());
    }

    if (constraints & Plasma::SizeConstraint) {
        if (m_appletsLayout) {
            m_appletsLayout->setMaximumHeight(size().height()/4);
        }
        if (m_appletOverlay) {
            m_appletOverlay->resize(size());
        }
    }

    if (constraints & Plasma::StartupCompletedConstraint) {
        Plasma::DataEngine *engine = dataEngine("searchlaunch");
        engine->connectSource("query", this);
    }

    if (constraints & Plasma::ScreenConstraint) {
        if (screen() != -1 && m_searchField) {
            m_searchField->setFocus();
        }
    }

    if (constraints & Plasma::ImmutableConstraint) {
        //update lock button
        QAction *a = action("lock page");
        if (a) {
            switch (immutability()) {
                case Plasma::SystemImmutable:
                    a->setEnabled(false);
                    a->setVisible(false);
                    break;

                case Plasma::UserImmutable:
                    a->setText(i18n("Unlock Page"));
                    a->setIcon(KIcon("object-unlocked"));
                    a->setEnabled(true);
                    a->setVisible(true);
                    break;

                case Plasma::Mutable:
                    a->setText(i18n("Lock Page"));
                    a->setIcon(KIcon("object-locked"));
                    a->setEnabled(true);
                    a->setVisible(true);
                    break;
            }
        }

        //kill or create the config overlay if needed
        if (immutability() == Plasma::Mutable && !m_appletOverlay && m_toolBox->isShowing()) {
            m_appletOverlay = new LinearAppletOverlay(this, m_appletsLayout);
            m_appletOverlay->resize(size());
        } else if (immutability() != Plasma::Mutable && m_appletOverlay && m_toolBox->isShowing()) {
            m_appletOverlay->deleteLater();
            m_appletOverlay = 0;
        }
kWarning()<<"AAAAAA"<<immutability()<<Plasma::Mutable<<m_lastQuery.isNull();
        //enable or disable drag and drop
        if (immutability() == Plasma::Mutable && !m_lastQuery.isNull()) {
            m_resultsView->setDragAndDropMode(ItemContainer::CopyDragAndDrop);
        } else {
            m_resultsView->setDragAndDropMode(ItemContainer::NoDragAndDrop);
        }
        m_stripWidget->setImmutability(immutability());
    }
}

void SearchLaunch::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation) {
        return;
    }

    m_orientation = orientation;
    m_resultsView->setOrientation(orientation);
    if (m_orientation == Qt::Vertical) {
        if (m_leftArrow) {
            m_leftArrow->deleteLater();
            m_rightArrow->deleteLater();
            m_leftArrow=0;
            m_rightArrow=0;
        }
    } else {
        if (!m_leftArrow) {
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
            m_resultsLayout->insertItem(0, m_leftArrow);
            m_resultsLayout->addItem(m_rightArrow);
        }
    }
}

void SearchLaunch::setFormFactorFromLocation(Plasma::Location loc)
{
    switch (loc) {
    case Plasma::BottomEdge:
    case Plasma::TopEdge:
        //kDebug() << "setting horizontal form factor";
        setFormFactor(Plasma::Horizontal);
        break;
    case Plasma::RightEdge:
    case Plasma::LeftEdge:
        //kDebug() << "setting vertical form factor";
        setFormFactor(Plasma::Vertical);
        break;
    default:
        setFormFactor(Plasma::Horizontal);
    }
}

void SearchLaunch::restoreStrip()
{
    KConfigGroup cg = config();
    m_stripWidget->restore(cg);
    reset();
}

void SearchLaunch::updateConfigurationMode(bool config)
{
    qreal extraLeft = 0;
    qreal extraTop = 0;
    qreal extraRight = 0;
    qreal extraBottom = 0;

    if (config) {
        switch (m_toolBox->location()) {
            case Plasma::LeftEdge:
            extraLeft= m_toolBox->expandedGeometry().width();
            break;
        case Plasma::RightEdge:
            extraRight = m_toolBox->expandedGeometry().width();
            break;
        case Plasma::TopEdge:
            extraTop = m_toolBox->expandedGeometry().height();
            break;
        case Plasma::BottomEdge:
        default:
            extraBottom = m_toolBox->expandedGeometry().height();
        }
    }

    if (config && !m_appletOverlay && immutability() == Plasma::Mutable) {
        if (m_appletsLayout->count() == 2) {
            m_mainLayout->addItem(m_appletsLayout);
        }
        m_appletOverlay = new LinearAppletOverlay(this, m_appletsLayout);
        m_appletOverlay->resize(size());
        connect (m_appletOverlay, SIGNAL(dropRequested(QGraphicsSceneDragDropEvent *)),
                 this, SLOT(overlayRequestedDrop(QGraphicsSceneDragDropEvent *)));
    } else if (!config) {
        delete m_appletOverlay;
        m_appletOverlay = 0;
        if (m_appletsLayout->count() == 2) {
            m_mainLayout->removeItem(m_appletsLayout);
        }
    }

    m_mainLayout->setContentsMargins(extraLeft, extraTop, extraRight, extraBottom);
}

void SearchLaunch::overlayRequestedDrop(QGraphicsSceneDragDropEvent *event)
{
    dropEvent(event);
}

void SearchLaunch::resultsViewRequestedDrop(Plasma::IconWidget *icon)
{
    if (m_stripWidget->mapToScene(m_stripWidget->geometry()).boundingRect().intersects(icon->mapToScene(icon->boundingRect()).boundingRect())) {
        if (m_matches.contains(icon)) {
            m_stripWidget->add(m_matches.value(icon, Plasma::QueryMatch(0)), m_runnermg->searchContext()->query());
        }
    }
}

void SearchLaunch::paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *, const QRect &)
{
    if (m_stripUninitialized) {
        m_stripUninitialized = false;
        QTimer::singleShot(100, this, SLOT(restoreStrip()));
    } else {
        m_background->resizeFrame(QSizeF(size().width(), m_stripWidget->geometry().bottom()));
        m_background->paintFrame(painter);
    }
}

void SearchLaunch::delayedQuery()
{
    m_searchTimer->start(500);
}

void SearchLaunch::query()
{
    QString query = m_searchField->text();
    doSearch(query);
    m_lastQuery = query;
}

void SearchLaunch::searchReturnPressed()
{
    QString query = m_searchField->text();
    //by pressing enter  do a query or
    if (m_firstItem && query == m_lastQuery && !query.isEmpty()) {
        m_runnermg->run(m_matches.value(m_firstItem, Plasma::QueryMatch(0)));
        reset();
    } else {
        doSearch(query);
        m_lastQuery = query;
    }
}

void SearchLaunch::goRight()
{
    QGraphicsSceneWheelEvent ev(QEvent::GraphicsSceneWheel);
    ev.setDelta(-120);
    scene()->sendEvent(m_resultsView, &ev);

    if (m_resultsView->widget()->geometry().right()-2 <= m_resultsView->viewportGeometry().right() && sender() == m_rightArrow) {
        action("next containment")->trigger();
    }
}

void SearchLaunch::goLeft()
{
    QGraphicsSceneWheelEvent ev(QEvent::GraphicsSceneWheel);
    ev.setDelta(120);
    scene()->sendEvent(m_resultsView, &ev);

    if (m_resultsView->widget()->geometry().left() >= -2 && sender() == m_leftArrow) {
        action("previous containment")->trigger();
    }
}

void SearchLaunch::dataUpdated(const QString &sourceName, const Plasma::DataEngine::Data &data)
{
    Q_UNUSED(sourceName);

    const QString query(data["query"].toString());
    //Take ownership of the screen if we don't have one
    //FIXME: hardcoding 0 is bad: maybe pass the screen from the dataengine?
    if (!query.isEmpty()) {
        if (screen() < 0) {
            setScreen(0);
        }
        emit activate();
    }

    doSearch(query);
    if (m_searchField) {
        m_searchField->setText(query);
    }
}

void SearchLaunch::focusInEvent(QFocusEvent *event)
{
    if (m_searchField) {
        m_searchField->setFocus();
    }
    Containment::focusInEvent(event);
}

void SearchLaunch::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ContentsRectChange) {
        qreal left, top, right, bottom;
        getContentsMargins(&left, &top, &right, &bottom);

        //left preferred over right
        if (left > top && left > right && left > bottom) {
            m_toolBox->setLocation(Plasma::RightEdge);
        } else if (right > top && right >= left && right > bottom) {
            m_toolBox->setLocation(Plasma::LeftEdge);
        } else if (bottom > top && bottom > left && bottom > right) {
            m_toolBox->setLocation(Plasma::TopEdge);
        //bottom is the default
        } else {
            m_toolBox->setLocation(Plasma::BottomEdge);
        }

        if (m_toolBox->isShowing()) {
            updateConfigurationMode(true);
        }
    }
}

void SearchLaunch::createConfigurationInterface(KConfigDialog *parent)
{
    RunnersConfig *runnersConfig = new RunnersConfig(m_runnermg, parent);
    parent->addPage(runnersConfig, i18nc("Title of the page that lets the user choose the loaded krunner plugins", "Search plugins"), "edit-find");

    connect(parent, SIGNAL(applyClicked()), runnersConfig, SLOT(accept()));
    connect(parent, SIGNAL(okClicked()), runnersConfig, SLOT(accept()));
}

K_EXPORT_PLASMA_APPLET(sal, SearchLaunch)

#include "sal.moc"

