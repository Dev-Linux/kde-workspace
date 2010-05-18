/*
 *   Copyright 2008 Marco Martin <notmart@gmail.com>
 *   Copyright 2009 Aaron Seigo <aseigo@kde.org>
 *   Copyright 2010 Chani Armitage <chani@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
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

#include "controllerwindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDesktopWidget>
#include <QPainter>

#include <kwindowsystem.h>
#include <netwm.h>

#include <Plasma/Containment>
#include <Plasma/Corona>
#include <Plasma/Theme>
#include <Plasma/FrameSvg>
#include <Plasma/Dialog>
#include <Plasma/WindowEffects>

#include "widgetsexplorer/widgetexplorer.h"
#include "activitymanager/activitymanager.h"

#include <kephal/screens.h>

ControllerWindow::ControllerWindow(QWidget* parent)
   : Plasma::Dialog(parent),
     m_location(Plasma::Floating),
     m_layout(new QBoxLayout(QBoxLayout::TopToBottom, this)),
     m_background(new Plasma::FrameSvg(this)),
     m_containment(0),
     m_corona(0),
     m_view(0),
     m_watchedWidget(0),
     m_activityManager(0),
     m_widgetExplorer(0)
{
    Q_UNUSED(parent)

    m_background->setImagePath("dialogs/background");
    m_background->setContainsMultipleImages(true);

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    KWindowSystem::setState(winId(), NET::SkipTaskbar | NET::SkipPager | NET::Sticky | NET::KeepAbove);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocus(Qt::ActiveWindowFocusReason);
    setLocation(Plasma::BottomEdge);

    QPalette pal = palette();
    pal.setBrush(backgroundRole(), Qt::transparent);
    setPalette(pal);

    Plasma::WindowEffects::overrideShadow(winId(), true);


    m_layout->setContentsMargins(0, 0, 0, 0);

    connect(KWindowSystem::self(), SIGNAL(activeWindowChanged(WId)), this, SLOT(onActiveWindowChanged(WId)));
    connect(m_background, SIGNAL(repaintNeeded()), SLOT(backgroundChanged()));
    Kephal::Screens *screens = Kephal::Screens::self();
    connect(screens, SIGNAL(screenResized(Kephal::Screen *, QSize, QSize)),
            this, SLOT(adjustSize(Kephal::Screen *)));
    adjustSize(0);
}

ControllerWindow::~ControllerWindow()
{

    if (m_activityManager) {
        if (m_corona) {
            m_corona->removeOffscreenWidget(m_activityManager);
        }
        //FIXME the qt4.6 comment below applies here too
    }
    if (m_widgetExplorer) {
        if (m_corona) {
            m_corona->removeOffscreenWidget(m_widgetExplorer);
        }

        if (m_widgetExplorer->scene()) {
            //FIXME: causes a crash in Qt 4.6 *sigh*
            //m_widgetExplorer->scene()->removeItem(m_widgetExplorer);
        }
    }

    delete m_activityManager;
    delete m_widgetExplorer;
    delete m_view;
}


void ControllerWindow::adjustSize(Kephal::Screen *screen)
{
    Q_UNUSED(screen)

    QSize screenSize = Kephal::ScreenUtils::screenGeometry(Kephal::ScreenUtils::screenId(pos())).size();
    setMaximumSize(screenSize);
}

void ControllerWindow::backgroundChanged()
{
    Plasma::Location l = m_location;
    m_location = Plasma::Floating;
    setLocation(l);
    update();
}

void ControllerWindow::setContainment(Plasma::Containment *containment)
{
    if (containment == m_containment) {
        return;
    }
    m_containment = containment;

    if (m_containment) {
        disconnect(m_containment, 0, this, 0);
    }

    if (!containment) {
        return;
    }
    m_corona = m_containment->corona();

    if (m_widgetExplorer) {
        m_widgetExplorer->setContainment(m_containment);
    }
}

Plasma::Containment *ControllerWindow::containment() const
{
    return m_containment;
}


void ControllerWindow::setLocation(const Plasma::Location &loc)
{
    if (m_location == loc) {
        return;
    }

    Plasma::WindowEffects::slideWindow(this, loc);

    m_location = loc;

    switch (loc) {
    case Plasma::LeftEdge:
        m_background->setEnabledBorders(Plasma::FrameSvg::RightBorder);
        m_layout->setDirection(QBoxLayout::TopToBottom);
        setContentsMargins(0, 0, m_background->marginSize(Plasma::RightMargin), 0);
        break;

    case Plasma::RightEdge:
        m_background->setEnabledBorders(Plasma::FrameSvg::LeftBorder);
        m_layout->setDirection(QBoxLayout::TopToBottom);
        setContentsMargins(m_background->marginSize(Plasma::LeftMargin), 0, 0, 0);
        break;

    case Plasma::TopEdge:
        m_background->setEnabledBorders(Plasma::FrameSvg::BottomBorder);
        m_layout->setDirection(QBoxLayout::BottomToTop);
        setContentsMargins(0, 0, 0, m_background->marginSize(Plasma::BottomMargin));
        break;

    case Plasma::BottomEdge:
    default:
        m_background->setEnabledBorders(Plasma::FrameSvg::TopBorder);
        m_layout->setDirection(QBoxLayout::TopToBottom);
        setContentsMargins(0, m_background->marginSize(Plasma::TopMargin), 0, 0);
        break;
    }

    if (m_watchedWidget) {
        //FIXME maybe I should make these two inherit from something
        //or make orientation a slot.
        if (m_watchedWidget == (QGraphicsWidget*)m_widgetExplorer) {
            m_widgetExplorer->setOrientation(orientation());
        } else {
            m_activityManager->setOrientation(orientation());
        }
    }
}

QPoint ControllerWindow::positionForPanelGeometry(const QRect &panelGeom) const
{
    QRect screenGeom = Kephal::ScreenUtils::screenGeometry(containment()->screen());

    switch (m_location) {
    case Plasma::LeftEdge:
        return QPoint(panelGeom.right(), screenGeom.top());
        break;
    case Plasma::RightEdge:
        return QPoint(panelGeom.left() - width(), screenGeom.top());
        break;
    case Plasma::TopEdge:
        return QPoint(screenGeom.left(), panelGeom.bottom());
        break;
    case Plasma::BottomEdge:
    default:
        return QPoint(screenGeom.left(), panelGeom.top() - height());
        break;
    }
}

Plasma::Location ControllerWindow::location() const
{
    return m_location;
}

Qt::Orientation ControllerWindow::orientation() const
{
    if (m_location == Plasma::LeftEdge || m_location == Plasma::RightEdge) {
        return Qt::Vertical;
    }

    return Qt::Horizontal;
}


void ControllerWindow::showWidgetExplorer()
{
    if (!m_containment) {
        return;
    }

    if (!m_widgetExplorer) {
        m_widgetExplorer = new Plasma::WidgetExplorer(orientation());
        m_watchedWidget = m_widgetExplorer;
        m_widgetExplorer->setContainment(m_containment);
        m_widgetExplorer->populateWidgetList();
        m_widgetExplorer->setIconSize(KIconLoader::SizeHuge);

        m_containment->corona()->addOffscreenWidget(m_widgetExplorer);
        m_widgetExplorer->show();

        m_widgetExplorer->setIconSize(KIconLoader::SizeHuge);

        if (orientation() == Qt::Horizontal) {
            m_widgetExplorer->resize(width(), m_widgetExplorer->size().height());
        } else {
            m_widgetExplorer->resize(m_widgetExplorer->size().width(), height());
        }

        setGraphicsWidget(m_widgetExplorer);

        connect(m_widgetExplorer, SIGNAL(closeClicked()), this, SLOT(close()));
    } else {
        m_widgetExplorer->setOrientation(orientation());
        m_widgetExplorer->show();
        m_watchedWidget = m_widgetExplorer;
        setGraphicsWidget(m_widgetExplorer);
    }

}

void ControllerWindow::showActivityManager()
{
    if (!m_activityManager) {
        m_activityManager = new ActivityManager(orientation());
        m_watchedWidget = m_activityManager;

        m_corona->addOffscreenWidget(m_activityManager);
        m_activityManager->show();

        if (orientation() == Qt::Horizontal) {
            m_activityManager->resize(width(), m_activityManager->size().height());
        } else {
            m_activityManager->resize(m_activityManager->size().width(), height());
        }

        m_activityManager->setIconSize(KIconLoader::SizeHuge);

        setGraphicsWidget(m_activityManager);

        connect(m_activityManager, SIGNAL(closeClicked()), this, SLOT(close()));
    } else {
        m_activityManager->setOrientation(orientation());
        m_watchedWidget = m_activityManager;
        m_activityManager->show();
        setGraphicsWidget(m_activityManager);
    }

}

bool ControllerWindow::isControllerViewVisible() const
{
    return m_view && m_view->isVisible();
}

Plasma::FrameSvg *ControllerWindow::background() const
{
    return m_background;
}

void ControllerWindow::onActiveWindowChanged(WId id)
{
    Q_UNUSED(id)

    //if the active window isn't the plasma desktop and the widgets explorer is visible,
    //then close the panel controller
    if (QApplication::activeWindow() == 0 || (QApplication::activeWindow()->winId() != KWindowSystem::activeWindow())) {
        if (m_view && m_view->isVisible() && !isActiveWindow()) {
            //close();
        }
    }
}

void ControllerWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source );

    m_background->paintFrame(&painter);
}

void ControllerWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
    }
}

void ControllerWindow::resizeEvent(QResizeEvent * event)
{
    m_background->resizeFrame(size());

    Plasma::WindowEffects::enableBlurBehind(effectiveWinId(), true, m_background->mask());

    qDebug() << "ControllerWindow::resizeEvent" << event->oldSize();

    Plasma::Dialog::resizeEvent(event);
}

#include "controllerwindow.moc"
