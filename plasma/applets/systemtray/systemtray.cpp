/***************************************************************************
 *   systemtray.cpp                                                        *
 *                                                                         *
 *   Copyright (C) 2007 Alexander Rodin <rodin.alexander@gmail.com>        *
 *   Copyright (C) 2007 Jason Stubbs <jasonbstubbs@gmail.com>              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

// Own
#include "systemtray.h"

// Qt
#include <QApplication>
#include <QGraphicsView>
#include <QTimer>

//Plasma
#include <plasma/panelsvg.h>

K_EXPORT_PLASMA_APPLET(systemtray, SystemTray)

SystemTray::SystemTray(QObject *parent, const QVariantList &arguments)
    : Plasma::Applet(parent, arguments),
      m_startUpDelayShowTimer(0),
      m_showOwnBackground(false)
{
    // Revert to Qt-4.4.0 behaviour so that the SystemTrayWidget is always on
    // top. Note, if a sibling widget requires a native window for other
    // reasons the problem could reoccur.
    // Another alternative is to raise() during SystemTrayWidget construction
    // and then have it listen for ChildAdded events on its parent. However,
    // this causes a lot of flashes of white and so should be avoided if
    // possible.
    // See http://bugs.kde.org/show_bug.cgi?id=168007
    // and http://mail.kde.org/pipermail/plasma-devel/2008-August/000258.html

#if QT_VERSION < 0x040401
    Qt::ApplicationAttribute attr = (Qt::ApplicationAttribute)4;
#else
    Qt::ApplicationAttribute attr = Qt::AA_DontCreateNativeWidgetSiblings;
#endif
    if (!QApplication::testAttribute(attr)) {
        QApplication::setAttribute(attr);
    }

    m_background = new Plasma::PanelSvg(this);
    m_background->setImagePath("widgets/systemtray");
}

void SystemTray::init()
{
    connect(this, SIGNAL(geometryChanged()), this, SLOT(updateWidgetGeometry()));
}

SystemTray::~SystemTray()
{
    // Get rid of our SystemTrayWidget if we still have one
    delete m_systemTrayWidget;
}

void SystemTray::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & (Plasma::LocationConstraint | Plasma::FormFactorConstraint)) {
        if (formFactor() == Plasma::Planar) {
            qreal leftMargin, topMargin, rightMargin, bottomMargin;
            getContentsMargins(&leftMargin, &topMargin, &rightMargin, &bottomMargin);
            setMinimumSize(22 + leftMargin + rightMargin, 22 + topMargin + bottomMargin);
        }
        updateWidgetOrientation();
    }

    if (constraints & Plasma::StartupCompletedConstraint) {
        updateWidgetGeometry();
    }
}

void SystemTray::paintInterface(QPainter *painter,
                                const QStyleOptionGraphicsItem *option,
                                const QRect& contentsRect)
{
    Q_UNUSED(option)
    Q_UNUSED(contentsRect)

    if (m_showOwnBackground) {
        m_background->paintPanel(painter);
    }
}

void SystemTray::updateWidgetOrientation()
{
    if (!m_systemTrayWidget) {
        return;
    }
    // TODO: Handle other form factors
    if (formFactor() == Plasma::Horizontal) {
        m_systemTrayWidget->setOrientation(Qt::Horizontal);
    } else if (formFactor() == Plasma::Vertical) {
        m_systemTrayWidget->setOrientation(Qt::Vertical);
    } else { // desktop
        m_systemTrayWidget->setOrientation(Qt::Horizontal);
    }
}

void SystemTray::updateWidgetGeometry()
{
    QGraphicsView *parentView = view();
    if (!parentView) {
        kDebug() << "Problem view is NULL";
        return;
    }

    if (!m_systemTrayWidget || m_systemTrayWidget->parentWidget() != parentView) {
        delete m_systemTrayWidget;
        m_systemTrayWidget = new SystemTrayWidget(parentView);
        updateWidgetOrientation();
        connect(m_systemTrayWidget, SIGNAL(sizeShouldChange()),
                this, SLOT(updateWidgetGeometry()));

        if (! m_startUpDelayShowTimer) {
            m_startUpDelayShowTimer = new QTimer(this);
            connect(m_startUpDelayShowTimer, SIGNAL(timeout()), this, SLOT(startupDelayer()));
        }
    }

    if (m_startUpDelayShowTimer) {
        kDebug() << "start up delay";
        m_startUpDelayShowTimer->start(STARTUP_TIMER_DELAY);
        return; // don't relayout yet.
    }

    // Figure out the margins set by the background svg and disable the svg
    // if there won't be enough room for a single row of icons
    qreal leftMargin, topMargin, rightMargin, bottomMargin;
    if (formFactor() == Plasma::Vertical || formFactor() == Plasma::Horizontal) {
        m_background->setElementPrefix(QString());
        m_background->getMargins(leftMargin, topMargin, rightMargin, bottomMargin);

        if (geometry().width() - leftMargin - rightMargin < 22 ||
            geometry().height() - topMargin - bottomMargin < 22) {
            m_showOwnBackground = false;
            leftMargin = topMargin = rightMargin = bottomMargin = 0;
        } else {
            m_showOwnBackground = true;
            m_background->resizePanel(size());
            update();
        }
    } else {
        getContentsMargins(&leftMargin, &topMargin, &rightMargin, &bottomMargin);
    }

    // Update our preferred size based on the wystem tray widget's size and any margins
    QRectF rf = mapFromView(parentView, QRect(m_systemTrayWidget->pos(),
                                              m_systemTrayWidget->minimumSize()));
    rf.setWidth(rf.width() + leftMargin + rightMargin);
    rf.setHeight(rf.height() + topMargin + bottomMargin);

    kDebug() << "rf:" << rf;

    bool hintChanged = false;
    if (rf.size() != preferredSize()) {
        hintChanged = true;
    }

    if (m_systemTrayWidget->orientation() == Qt::Vertical) {
        setMinimumHeight(rf.height());
        setMinimumWidth(22);
    } else {
        setMinimumWidth(rf.width());
        setMinimumHeight(22);
    }
    setPreferredSize(rf.size());

    // Calculate the rect usable by the system tray widget
    rf = rect();
    rf.moveLeft(rf.left() + leftMargin);
    rf.setWidth(rf.width() - leftMargin - rightMargin);
    rf.moveTop(rf.top() + topMargin);
    rf.setHeight(rf.height() - topMargin - bottomMargin);

    // Set the widget's maximum size to the size to the available size.
    // The widget will use this size to calculate how many rows/columns
    // can be displayed.
    QRect r = mapToView(parentView, rf);
    m_systemTrayWidget->setMaximumSize(r.size());

    // Center the widget within the available area
    QSize s = m_systemTrayWidget->minimumSize();
    r.moveLeft(r.left() + (r.width() - s.width()) / 2);
    r.moveTop(r.top() + (r.height() - s.height()) / 2);
    r.setSize(s);
    m_systemTrayWidget->setGeometry(r);

    if (hintChanged) {
        emit sizeHintChanged(Qt::PreferredSize);
    }
}

void SystemTray::startupDelayer()
{
    m_startUpDelayShowTimer->deleteLater();
    m_startUpDelayShowTimer = 0;
    m_systemTrayWidget->setVisible(true);
    m_systemTrayWidget->init();
}

#include "systemtray.moc"
