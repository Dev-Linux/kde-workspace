/*
*   Copyright 2007 by Alex Merry <alex.merry@kdemail.net>
*   Copyright 2008 by Alexis Ménard <darktears31@gmail.com>
*   Copyright 2008 by Aaron Seigo <aseigo@kde.org>
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

#include "newspaper.h"
#include "appletoverlay.h"
#include "applettitlebar.h"
#include "appletscontainer.h"
#include "appletsview.h"
#include "../common/nettoolbox.h"

#include <limits>

#include <QApplication>
#include <QPainter>
#include <QBitmap>
#include <QDesktopWidget>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QAction>
#include <QGraphicsLinearLayout>
#include <QTimer>
#include <QGraphicsSceneWheelEvent>

#include <KAction>
#include <KDebug>
#include <KIcon>
#include <KDialog>
#include <KIntNumInput>
#include <KMessageBox>

#include <Plasma/Corona>
#include <Plasma/FrameSvg>
#include <Plasma/Theme>
#include <Plasma/View>
#include <Plasma/PopupApplet>
#include <Plasma/Frame>
#include <Plasma/ToolButton>

using namespace Plasma;


Newspaper::Newspaper(QObject *parent, const QVariantList &args)
    : Containment(parent, args),
      m_orientation(Qt::Vertical),
      m_expandAll(false),
      m_appletOverlay(0),
      m_dragging(0)
{
    setContainmentType(Containment::CustomContainment);

    connect(this, SIGNAL(appletRemoved(Plasma::Applet*)),
            this, SLOT(refreshLayout()));


    connect(this, SIGNAL(toolBoxVisibilityChanged(bool)), this, SLOT(updateConfigurationMode(bool)));
}

Newspaper::~Newspaper()
{
    delete m_appletOverlay;
    config().writeEntry("orientation", (int)m_orientation);
}

void Newspaper::init()
{
    m_externalLayout = new QGraphicsLinearLayout(this);
    m_externalLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollWidget = new AppletsView(this);
    m_externalLayout->addItem(m_scrollWidget);
    m_container = new AppletsContainer(m_scrollWidget);
    m_scrollWidget->setWidget(m_container);
    connect(m_container, SIGNAL(appletActivated(Plasma::Applet *)), this, SLOT(appletActivated(Plasma::Applet *)));

    m_updateSizeTimer = new QTimer(this);
    m_updateSizeTimer->setSingleShot(true);
    connect(m_updateSizeTimer, SIGNAL(timeout()), m_container, SLOT(updateSize()));

    m_relayoutTimer = new QTimer(this);
    m_relayoutTimer->setSingleShot(true);
    connect(m_relayoutTimer, SIGNAL(timeout()), m_container, SLOT(updateSize()));
    connect(m_relayoutTimer, SIGNAL(timeout()), m_container, SLOT(cleanupColumns()));
    connect(m_container, SIGNAL(appletSizeHintChanged()), this, SLOT(appletSizeHintChanged()));

    m_orientation = (Qt::Orientation)config().readEntry("orientation", (int)Qt::Vertical);
    m_container->setOrientation(m_orientation);

    m_expandAll = config().readEntry("ExpandAllApplets", false);
    m_container->setExpandAll(m_expandAll);
    m_scrollWidget->setFiltersChildEvents(m_expandAll || m_orientation == Qt::Horizontal);


    m_container->addColumn();
    setOrientation(m_orientation);

    Plasma::Svg *borderSvg = new Plasma::Svg(this);
    borderSvg->setImagePath("newspaper/border");

    Containment::init();
    setHasConfigurationInterface(true);

    m_toolBox = new NetToolBox(this);
    setToolBox(m_toolBox);
    connect(m_toolBox, SIGNAL(toggled()), this, SIGNAL(toolBoxToggled()));
    connect(m_toolBox, SIGNAL(visibilityChanged(bool)), this, SIGNAL(toolBoxVisibilityChanged(bool)));
    m_toolBox->show();

    QAction *a = action("add widgets");
    if (a) {
        m_toolBox->addTool(a);
    }

    a = new QAction(KIcon("view-fullscreen"), i18n("Expand widgets"), this);
    addAction("expand widgets", a);
    m_toolBox->addTool(a);
    connect(a, SIGNAL(triggered()), this, SLOT(toggleExpandAllApplets()));

    a = new QAction(KIcon("view-pim-news"), i18n("Add page"), this);
    addAction("add page", a);
    m_toolBox->addTool(a);
    connect(a, SIGNAL(triggered()), this, SLOT(addNewsPaper()));

    a = action("configure");
    if (a) {
        a->setText(i18n("Configure page"));
        m_toolBox->addTool(a);
    }


    QAction *lockAction = 0;
    if (corona()) {
        lockAction = corona()->action("lock widgets");
    }

    if (!lockAction || !lockAction->isEnabled()) {
        lockAction = new QAction(this);
        addAction("lock page", lockAction);
        lockAction->setText(i18n("Lock page"));
        lockAction->setIcon(KIcon("object-locked"));
        QObject::connect(lockAction, SIGNAL(triggered(bool)), this, SLOT(toggleImmutability()));
    }
    m_toolBox->addTool(lockAction);

    a = action("remove");
    if (a) {
        a->setText(i18n("Remove page"));
        m_toolBox->addTool(a);
    }

    a = new QAction(i18n("Next activity"), this);
    addAction("next containment", a);
    a = new QAction(i18n("Previous activity"), this);
    addAction("previous containment", a);

    if (corona()) {
        connect(corona(), SIGNAL(availableScreenRegionChanged()), this, SLOT(availableScreenRegionChanged()));
        availableScreenRegionChanged();
    }
}

void Newspaper::updateSize()
{
    m_container->updateSize();
}

void Newspaper::toggleImmutability()
{
    if (immutability() == Plasma::UserImmutable) {
        setImmutability(Plasma::Mutable);
    } else if (immutability() == Plasma::Mutable) {
        setImmutability(Plasma::UserImmutable);
    }
}

void Newspaper::toggleExpandAllApplets()
{
    m_expandAll = !m_expandAll;

    QAction *a = action("expand widgets");

    if (a) {
        if (m_expandAll) {
            a->setIcon(KIcon("view-restore"));
            a->setText(i18n("Collapse widgets"));
        } else {
            a->setIcon(KIcon("view-fullscreen"));
            a->setText(i18n("Expand widgets"));
        }
    }

    m_container->setExpandAll(m_expandAll);
    m_scrollWidget->setFiltersChildEvents(m_expandAll || m_orientation == Qt::Horizontal);
    config().writeEntry("ExpandAllApplets", m_expandAll);
}

void Newspaper::refreshLayout()
{
    if (!m_relayoutTimer->isActive()) {
        m_relayoutTimer->start(200);
    }
}

void Newspaper::setOrientation(Qt::Orientation orientation)
{
    m_orientation = orientation;
    m_container->setOrientation(m_orientation);
    m_externalLayout->setOrientation(m_orientation);

    if (m_orientation == Qt::Vertical) {
        m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    } else {
        m_container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }

    for (int i = 0; i < m_container->count(); ++i) {
        QGraphicsLinearLayout *lay = dynamic_cast<QGraphicsLinearLayout *>(m_container->itemAt(i));

        if (!lay) {
            continue;
        }

        lay->setOrientation(orientation);
    }
}

Plasma::Applet *Newspaper::addApplet(const QString &appletName, const int row, const int column)
{
    m_container->setAutomaticAppletLayout(false);
    Plasma::Applet *applet = Containment::addApplet(appletName);
    m_container->addApplet(applet, row, column);
    m_container->setAutomaticAppletLayout(true);
    return applet;
}

Plasma::Applet *Newspaper::addApplet(Plasma::Applet *applet, const int row, const int column)
{
    m_container->setAutomaticAppletLayout(false);
    Containment::addApplet(applet);
    m_container->addApplet(applet, row, column);
    m_container->setAutomaticAppletLayout(true);
    return applet;
}

Qt::Orientation Newspaper::orientation() const
{
    return m_orientation;
}

void Newspaper::appletSizeHintChanged()
{
    if (m_updateSizeTimer) {
        m_updateSizeTimer->start(200);
    }
}

void Newspaper::constraintsEvent(Plasma::Constraints constraints)
{
    kDebug() << "constraints updated with" << constraints << "!!!!!!";

    if (constraints & Plasma::StartupCompletedConstraint) {
        connect(this, SIGNAL(appletAdded(Plasma::Applet*,QPointF)),
                m_container, SLOT(layoutApplet(Plasma::Applet*,QPointF)));
        Plasma::Corona *c = corona();
        if (c) {
            connect(c, SIGNAL(containmentAdded(Plasma::Containment *)),
                    this, SLOT(containmentAdded(Plasma::Containment *)));

            foreach (Plasma::Containment *containment, corona()->containments()) {
                Newspaper *news = qobject_cast<Newspaper *>(containment);
                if (news) {
                    connect(news, SIGNAL(destroyed(QObject *)),
                            this, SLOT(containmentRemoved(QObject *)));
                }
             }
             QTimer::singleShot(100, this, SLOT(updateRemoveActionVisibility()));
        }
    }

    if (constraints & Plasma::SizeConstraint && m_appletOverlay) {
        m_appletOverlay->resize(size());
    }

    if (constraints & Plasma::SizeConstraint) {
        m_container->syncColumnSizes();
    }

    if (constraints & Plasma::ImmutableConstraint) {
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

        a = action("add page");
        if (a) {
            if (immutability() == Plasma::Mutable) {
                a->setEnabled(true);
                a->setVisible(true);
            } else {
                a->setEnabled(false);
                a->setVisible(false);
            }
        }

        if (immutability() == Plasma::Mutable && !m_appletOverlay && m_toolBox->isShowing()) {
            m_appletOverlay = new AppletOverlay(this, this);
            m_appletOverlay->resize(size());
        } else if (immutability() != Plasma::Mutable && m_appletOverlay && m_toolBox->isShowing()) {
            m_appletOverlay->deleteLater();
            m_appletOverlay = 0;
        }
        updateRemoveActionVisibility();
    }
}

void Newspaper::updateConfigurationMode(bool config)
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
        m_appletOverlay = new AppletOverlay(this, this);
        m_appletOverlay->resize(size());
    } else if (!config) {
        delete m_appletOverlay;
        m_appletOverlay = 0;
    }

    m_externalLayout->setContentsMargins(extraLeft, extraTop, extraRight, extraBottom);

    if (!config) {
        m_container->cleanupColumns();
    }

}

void Newspaper::changeEvent(QEvent *event)
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

void Newspaper::addNewsPaper()
{
    Plasma::Corona *c = corona();
    if (!c) {
        return;
    }

    //count the pages
    int numNewsPapers = 0;
    if (corona()) {
        foreach (Plasma::Containment *containment, corona()->containments()) {
            if (qobject_cast<Newspaper *>(containment)) {
                ++numNewsPapers;
            }
        }
    }

    Plasma::Containment *cont = c->addContainment("newspaper");
    cont->setActivity(i18nc("Page number", "Page %1", numNewsPapers+1));
    cont->setScreen(0);
    cont->setToolBoxOpen(true);
}

void Newspaper::restore(KConfigGroup &group)
{
    Containment::restore(group);

    KConfigGroup appletsConfig(&group, "Applets");

    //the number of items in orderedApplets is the number of columns
    QMap<int, QMap<int, Applet *> > orderedApplets;
    QList<Applet *> unorderedApplets;

    foreach (Applet *applet, applets()) {
        KConfigGroup appletConfig(&appletsConfig, QString::number(applet->id()));
        KConfigGroup layoutConfig(&appletConfig, "LayoutInformation");

        int column = layoutConfig.readEntry("Column", -1);
        int order = layoutConfig.readEntry("Order", -1);

        if (order > -1) {
            if (column > -1) {
                orderedApplets[column][order] = applet;
            } else {
                unorderedApplets.append(applet);
            }
        //if LayoutInformation is not available use the usual way, as a bonus makes it retrocompatible with oler configs
        } else {
            unorderedApplets.append(applet);
        }

        connect(applet, SIGNAL(sizeHintChanged(Qt::SizeHint)), this, SLOT(appletSizeHintChanged()));
    }

    //if the required columns does not exist, create them
    if (m_container->count() < orderedApplets.count()) {
        int columnsToAdd = orderedApplets.count()-m_container->count();
        for (int i = 0; i < columnsToAdd; ++i) {
            m_container->addColumn();
        }
    }

    //finally add all the applets that had a layout information
    int column = 0;
    QMap<int, QMap<int, Applet *> >::const_iterator it = orderedApplets.constBegin();

    while (it != orderedApplets.constEnd()) {
        QGraphicsLinearLayout *lay = dynamic_cast<QGraphicsLinearLayout *>(m_container->itemAt(column));
        ++column;

        //this should never happen
        if (!lay) {
            ++it;
            continue;
        }

        foreach (Applet *applet, it.value()) {
            lay->insertItem(lay->count()-1, applet);
            m_container->createAppletTitle(applet);
        }
        ++it;
    }

    //add all the remaining applets
    foreach (Applet *applet, unorderedApplets) {
        m_container->layoutApplet(applet, applet->pos());
    }

    m_container->updateSize();
}

void Newspaper::saveContents(KConfigGroup &group) const
{
    Containment::saveContents(group);

    KConfigGroup appletsConfig(&group, "Applets");
    for (int column = 0; column < m_container->count(); ++column) {
        QGraphicsLinearLayout *lay = static_cast<QGraphicsLinearLayout *>(m_container->itemAt(column));
        for (int row = 0; row < lay->count(); ++row) {
            const Applet *applet = dynamic_cast<Applet *>(lay->itemAt(row));
            if (applet) {
                KConfigGroup appletConfig(&appletsConfig, QString::number(applet->id()));
                KConfigGroup layoutConfig(&appletConfig, "LayoutInformation");

                layoutConfig.writeEntry("Column", column);
                layoutConfig.writeEntry("Order", row);
            }
        }
    }
}

void Newspaper::updateRemoveActionVisibility()
{
    int newspapers = 0;

    foreach (Plasma::Containment *containment, corona()->containments()) {
        if (qobject_cast<Newspaper *>(containment)) {
            ++newspapers;
        }
    }

    QAction *a = action("remove");
    if (a) {
        a->setEnabled(newspapers > 1);
        a->setVisible(newspapers > 1);
    }
}

void Newspaper::appletActivated(Plasma::Applet *applet)
{
    m_scrollWidget->ensureItemVisible(applet);
}

void Newspaper::containmentAdded(Plasma::Containment *containment)
{
    //we now are sure there are at least two pages
    Newspaper *news = qobject_cast<Newspaper *>(containment);
    if (news) {
        connect(news, SIGNAL(destroyed(QObject *)), this, SLOT(containmentRemoved(QObject *)));
        QAction *a = action("remove");
        if (a) {
            a->setEnabled(true);
            a->setVisible(true);
        }
    }
}

void Newspaper::availableScreenRegionChanged()
{
    if (!corona()) {
        return;
    }

    QRect maxRect;
    int maxArea = 0;
    //we don't want the bounding rect (that could include panels too), but the maximumone representing the desktop
    foreach (QRect rect, corona()->availableScreenRegion(screen()).rects()) {
        int area = rect.width() * rect.height();
        if (area > maxArea) {
            maxRect = rect;
            maxArea = area;
        }
    }
    setContentsMargins(maxRect.left(), maxRect.top(), qMax((qreal)0.0, size().width() - maxRect.right()), qMax((qreal)0.0, size().height() - maxRect.bottom()));
}

void Newspaper::containmentRemoved(QObject *containment)
{
    if (!corona()) {
        return;
    }

    if (qobject_cast<Newspaper *>(containment)) {
        updateRemoveActionVisibility();
    }
}

//They all have to be reimplemented in order to accept them
void Newspaper::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    Containment::dragEnterEvent(event);
}

void Newspaper::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Containment::dragLeaveEvent(event);
}

void Newspaper::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    Containment::dragMoveEvent(event);
    event->accept();
}

void Newspaper::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    Containment::dropEvent(event);
    event->accept();
}

K_EXPORT_PLASMA_APPLET(newspaper, Newspaper)

#include "newspaper.moc"

