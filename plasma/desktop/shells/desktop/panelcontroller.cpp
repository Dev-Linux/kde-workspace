/*
 *   Copyright 2008 Marco Martin <notmart@gmail.com>
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

#include "panelcontroller.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QVBoxLayout>
#include <QDesktopWidget>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#ifdef Q_WS_X11
#include <QX11Info>
#endif

#include <KColorUtils>
#include <KIconLoader>
#include <KIcon>
#include <kwindowsystem.h>
#include <netwm.h>

#include <Plasma/Containment>
#include <Plasma/Corona>
#include <Plasma/PaintUtils>
#include <Plasma/Theme>
#include <Plasma/FrameSvg>
#include <Plasma/Dialog>
#include <Plasma/WindowEffects>

#include "plasmaapp.h"
#include "positioningruler.h"
#include "toolbutton.h"
#include "widgetsExplorer/widgetexplorer.h"

#include <kephal/screens.h>

class PanelController::ButtonGroup: public QFrame
{
public:
    ButtonGroup(QWidget *parent)
       : QFrame(parent)
    {
        background = new Plasma::FrameSvg(this);
        background->setImagePath("widgets/frame");
        background->setElementPrefix("plain");
    }

    void paintEvent(QPaintEvent *event)
    {
        Q_UNUSED(event)

        QPainter painter(this);
        background->resizeFrame(size());
        background->paintFrame(&painter);
    }

    Plasma::FrameSvg *background;
};



static const int MINIMUM_HEIGHT = 10;

PanelController::PanelController(QWidget* parent)
   : ControllerWindow(parent),
     m_extLayout(0),
     m_layout(0),
     m_dragging(NoElement),
     m_startDragPos(0,0),
     m_leftAlignTool(0),
     m_centerAlignTool(0),
     m_rightAlignTool(0),
     m_drawMoveHint(false)
{
    Q_UNUSED(parent)

    setAttribute(Qt::WA_TranslucentBackground);
    QPalette pal = palette();
    pal.setBrush(backgroundRole(), Qt::transparent);
    setPalette(pal);

    m_iconSvg = new Plasma::Svg(this);
    m_iconSvg->setImagePath("widgets/configuration-icons");
    m_iconSvg->setContainsMultipleImages(true);
    m_iconSvg->resize(KIconLoader::SizeSmall, KIconLoader::SizeSmall);

    //setWindowFlags(Qt::Popup);
    setWindowFlags(Qt::FramelessWindowHint);
    KWindowSystem::setState(winId(), NET::SkipTaskbar | NET::SkipPager | NET::Sticky);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocus(Qt::ActiveWindowFocusReason);

    //layout setup
    m_configWidget = new QWidget(this);
    layout()->addWidget(m_configWidget);

    m_extLayout = new QBoxLayout(QBoxLayout::TopToBottom, m_configWidget);
    m_extLayout->setContentsMargins(0, background()->marginSize(Plasma::TopMargin), 0, 0);

    m_layout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_layout->setContentsMargins(0, 0, 0, 0);

    if (QApplication::layoutDirection() == Qt::RightToLeft) {
        m_layout->setDirection(QBoxLayout::RightToLeft);
    } else {
        m_layout->setDirection(QBoxLayout::LeftToRight);
    }

    m_layout->addStretch();
    m_extLayout->addLayout(m_layout);

    //Add buttons

    //alignment
    //first the container
    QFrame *alignFrame = new ButtonGroup(m_configWidget);
    QVBoxLayout *alignLayout = new QVBoxLayout(alignFrame);

    m_alignLabel = new QLabel(i18n("Panel Alignment"), m_configWidget);
    alignLayout->addWidget(m_alignLabel);

    m_leftAlignTool = addTool("format-justify-left", i18n("Left"), alignFrame,  Qt::ToolButtonTextBesideIcon, true);
    m_leftAlignTool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    alignLayout->addWidget(m_leftAlignTool);
    m_leftAlignTool->setChecked(true);
    connect(m_leftAlignTool, SIGNAL(toggled(bool)), this, SLOT(alignToggled(bool)));

    m_centerAlignTool = addTool("format-justify-center", i18n("Center"), alignFrame,  Qt::ToolButtonTextBesideIcon, true);
    m_centerAlignTool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    alignLayout->addWidget(m_centerAlignTool);
    connect(m_centerAlignTool, SIGNAL(clicked(bool)), this, SLOT(alignToggled(bool)));

    m_rightAlignTool = addTool("format-justify-right", i18n("Right"), alignFrame,  Qt::ToolButtonTextBesideIcon, true);
    m_rightAlignTool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    alignLayout->addWidget(m_rightAlignTool);
    connect(m_rightAlignTool, SIGNAL(clicked(bool)), this, SLOT(alignToggled(bool)));


    //Panel mode
    //first the container
    QFrame *modeFrame = new ButtonGroup(m_configWidget);
    QVBoxLayout *modeLayout = new QVBoxLayout(modeFrame);

    m_modeLabel = new QLabel(i18n("Visibility"), m_configWidget);
    modeLayout->addWidget(m_modeLabel);

    m_normalPanelTool = addTool("checkmark", i18n("Always visible"), modeFrame,  Qt::ToolButtonTextBesideIcon, true);
    m_normalPanelTool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    modeLayout->addWidget(m_normalPanelTool);
    connect(m_normalPanelTool, SIGNAL(toggled(bool)), this, SLOT(panelVisibilityModeChanged(bool)));

    m_autoHideTool = addTool("video-display", i18n("Auto-hide"), modeFrame,  Qt::ToolButtonTextBesideIcon, true);
    m_autoHideTool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    modeLayout->addWidget(m_autoHideTool);
    connect(m_autoHideTool, SIGNAL(toggled(bool)), this, SLOT(panelVisibilityModeChanged(bool)));

    m_underWindowsTool = addTool("view-fullscreen", i18n("Windows can cover"), modeFrame,  Qt::ToolButtonTextBesideIcon, true);
    m_underWindowsTool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    modeLayout->addWidget(m_underWindowsTool);
    connect(m_underWindowsTool, SIGNAL(toggled(bool)), this, SLOT(panelVisibilityModeChanged(bool)));

    m_overWindowsTool = addTool("view-restore", i18n("Windows go below"), modeFrame,  Qt::ToolButtonTextBesideIcon, true);
    m_overWindowsTool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    modeLayout->addWidget(m_overWindowsTool);
    connect(m_overWindowsTool, SIGNAL(toggled(bool)), this, SLOT(panelVisibilityModeChanged(bool)));

    m_layout->addStretch();
    m_moveTool = addTool(QString(), i18n("Screen Edge"), m_configWidget);
    m_moveTool->setIcon(m_iconSvg->pixmap("move"));
    m_moveTool->installEventFilter(this);
    m_moveTool->setCursor(Qt::SizeAllCursor);
    m_layout->addWidget(m_moveTool);

    m_sizeTool = addTool(QString(), i18n("Height"), m_configWidget);
    m_sizeTool->installEventFilter(this);
    m_sizeTool->setCursor(Qt::SizeVerCursor);
    m_layout->addWidget(m_sizeTool);
    m_layout->addStretch();

    //other buttons
    m_layout->addSpacing(20);

    //Settings popup menu
    m_settingsTool = addTool("configure", i18n("More Settings"), m_configWidget);
    m_layout->addWidget(m_settingsTool);
    connect(m_settingsTool, SIGNAL(pressed()), this, SLOT(settingsPopup()));
    m_optionsDialog = new Plasma::Dialog(0); // don't pass in a parent; breaks with some lesser WMs
    m_optionsDialog->installEventFilter(this);
    KWindowSystem::setState(m_optionsDialog->winId(), NET::SkipTaskbar | NET::SkipPager | NET::Sticky);
    m_optDialogLayout = new QVBoxLayout(m_optionsDialog);
    m_optDialogLayout->setMargin(0);
    m_optDialogLayout->addWidget(alignFrame);
    m_optDialogLayout->addWidget(modeFrame);


    m_expandTool = addTool(QString(), i18n("Maximize Panel"), m_configWidget);
    m_expandTool->setIcon(m_iconSvg->pixmap("size-horizontal"));
    m_expandTool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_optDialogLayout->addWidget(m_expandTool);
    connect(m_expandTool, SIGNAL(clicked()), this, SLOT(maximizePanel()));

    ToolButton *closeControllerTool = addTool("window-close", i18n("Close this configuration window"), m_configWidget, Qt::ToolButtonIconOnly, false);
    m_layout->addWidget(closeControllerTool);
    connect(closeControllerTool, SIGNAL(clicked()), this, SLOT(close()));

    m_ruler = new PositioningRuler(m_configWidget);
    connect(m_ruler, SIGNAL(rulersMoved(int, int, int)), this, SLOT(rulersMoved(int, int, int)));
    m_extLayout->addWidget(m_ruler);

    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), SLOT(themeChanged()));
    themeChanged();
}

PanelController::~PanelController()
{
    //TODO: should we try and only call this when something has actually been
    //      altered that we care about?
    PlasmaApp::self()->corona()->requestConfigSync();
    delete m_optionsDialog;
}

void PanelController::setContainment(Plasma::Containment *c)
{
    if (!c) {
        return;
    }

    ControllerWindow::setContainment(c);

    QWidget *child;
    while (!m_actionWidgets.isEmpty()) {
        child = m_actionWidgets.first();
        //try to remove from both layouts
        m_layout->removeWidget(child);
        m_optDialogLayout->removeWidget(child);
        m_actionWidgets.removeFirst();
        child->deleteLater();
    }

    int insertIndex = m_layout->count() - 3;

    QAction *action = containment()->action("add widgets");
    if (action && action->isEnabled()) {
        ToolButton *addWidgetTool = addTool(action, this);
        m_layout->insertWidget(insertIndex, addWidgetTool);
        ++insertIndex;
        connect(containment(), SIGNAL(showAddWidgetsInterface(QPointF)), this, SLOT(switchToWidgetExplorer()));
    }

    action = new QAction(i18n("Add Spacer"), this);
    ToolButton *addSpaceTool = addTool(action, this);
    m_layout->insertWidget(insertIndex, addSpaceTool);
    ++insertIndex;
    connect(action, SIGNAL(triggered()), this, SLOT(addSpace()));

    action = containment()->action("lock widgets");
    if (action && action->isEnabled()) {
        ToolButton *lockWidgetsTool = addTool(action, this);
        m_layout->insertWidget(insertIndex, lockWidgetsTool);
        ++insertIndex;
        connect(lockWidgetsTool, SIGNAL(clicked()), this, SLOT(hide()));
    }

    action = containment()->action("remove");
    if (action && action->isEnabled()) {
        ToolButton *removePanelTool = addTool(action, this);
        removePanelTool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_optDialogLayout->insertWidget(insertIndex, removePanelTool);
        connect(removePanelTool, SIGNAL(clicked()), this, SLOT(hide()));
    }

    syncRuler();
}

QPoint PanelController::positionForPanelGeometry(const QRect &panelGeom) const
{
    QRect screenGeom = Kephal::ScreenUtils::screenGeometry(containment()->screen());

    switch (location()) {
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

void PanelController::setLocation(const Plasma::Location &loc)
{
    if (location() == loc) {
        return;
    }

    ControllerWindow::setLocation(loc);

    m_ruler->setLocation(loc);

    //The external layout gwts auto flipped when QApplication::layoutDirection() changes
    //and it shouldn't, the internal one no and it should, so i must manually invert both
    switch (loc) {
    case Plasma::LeftEdge:
        if (QApplication::layoutDirection() == Qt::RightToLeft) {
            m_extLayout->setDirection(QBoxLayout::LeftToRight);
        } else {
            m_extLayout->setDirection(QBoxLayout::RightToLeft);
        }
        break;

    case Plasma::RightEdge:
        if (QApplication::layoutDirection() == Qt::RightToLeft) {
            m_extLayout->setDirection(QBoxLayout::RightToLeft);
        } else {
            m_extLayout->setDirection(QBoxLayout::LeftToRight);
        }
        break;

    case Plasma::TopEdge:
        if (QApplication::layoutDirection() == Qt::RightToLeft) {
            m_layout->setDirection(QBoxLayout::RightToLeft);
        } else {
            m_layout->setDirection(QBoxLayout::LeftToRight);
        }
        break;

    case Plasma::BottomEdge:
    default:
        if (QApplication::layoutDirection() == Qt::RightToLeft) {
            m_layout->setDirection(QBoxLayout::RightToLeft);
        } else {
            m_layout->setDirection(QBoxLayout::LeftToRight);
        }
        break;
    }

    QRect screenGeom = Kephal::ScreenUtils::screenGeometry(containment()->screen());

    switch (loc) {
    case Plasma::LeftEdge:
    case Plasma::RightEdge:
        m_sizeTool->setCursor(Qt::SizeHorCursor);
        m_sizeTool->setText(i18n("Width"));
        m_sizeTool->setIcon(m_iconSvg->pixmap("size-horizontal"));
        m_expandTool->setIcon(m_iconSvg->pixmap("size-vertical"));
        m_leftAlignTool->setText(i18n("Top"));
        m_rightAlignTool->setText(i18n("Bottom"));

        m_ruler->setAvailableLength(screenGeom.height());

        break;
    case Plasma::TopEdge:
    case Plasma::BottomEdge:
    default:
        m_sizeTool->setCursor(Qt::SizeVerCursor);
        m_sizeTool->setText(i18n("Height"));
        m_sizeTool->setIcon(m_iconSvg->pixmap("size-vertical"));
        m_expandTool->setIcon(m_iconSvg->pixmap("size-horizontal"));
        m_leftAlignTool->setText(i18n("Left"));
        m_rightAlignTool->setText(i18n("Right"));

        m_ruler->setAvailableLength(screenGeom.width());
    }

    m_ruler->setMaximumSize(m_ruler->sizeHint());
    syncRuler();
}

void PanelController::setOffset(int newOffset)
{
    if (newOffset != m_ruler->offset()) {
        m_ruler->setOffset(newOffset);
    }
}

int PanelController::offset() const
{
    return m_ruler->offset();
}

void PanelController::setAlignment(const Qt::Alignment &newAlignment)
{
    if (newAlignment != m_ruler->alignment()) {
        if (newAlignment == Qt::AlignLeft) {
            m_leftAlignTool->setChecked(true);
        } else if (newAlignment == Qt::AlignCenter) {
            m_centerAlignTool->setChecked(true);
        } else if (newAlignment == Qt::AlignRight) {
            m_rightAlignTool->setChecked(true);
        }

        m_ruler->setAlignment(newAlignment);
    }
}

Qt::Alignment PanelController::alignment() const
{
    return m_ruler->alignment();
}

void PanelController::setVisibilityMode(PanelView::VisibilityMode mode)
{
    switch (mode) {
    case PanelView::AutoHide:
        m_autoHideTool->setChecked(true);
        break;
    case PanelView::LetWindowsCover:
        m_underWindowsTool->setChecked(true);
        break;
    case PanelView::WindowsGoBelow:
        m_overWindowsTool->setChecked(true);
        break;
    case PanelView::NormalPanel:
    default:
        m_normalPanelTool->setChecked(true);
        break;
    }
}

PanelView::VisibilityMode PanelController::panelVisibilityMode() const
{
    if (m_underWindowsTool->isChecked()) {
        return PanelView::LetWindowsCover;
    } else if (m_overWindowsTool->isChecked()) {
        return PanelView::WindowsGoBelow;
    } else if (m_autoHideTool->isChecked()) {
        return PanelView::AutoHide;
    } else {
        return PanelView::NormalPanel;
    }
}

void PanelController::themeChanged()
{
    QColor color = Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
    QPalette p = m_alignLabel->palette();
    p.setColor(QPalette::Normal, QPalette::WindowText, color);
    p.setColor(QPalette::Inactive, QPalette::WindowText, color);
    m_alignLabel->setPalette(p);
    m_modeLabel->setPalette(p);

    m_sizeTool->setIcon(m_iconSvg->pixmap("move"));

    if (orientation() == Qt::Horizontal) {
        m_sizeTool->setIcon(m_iconSvg->pixmap("size-vertical"));
    } else {
        m_sizeTool->setIcon(m_iconSvg->pixmap("size-horizontal"));
    }
}

void PanelController::switchToWidgetExplorer()
{
    m_configWidget->hide();
    showWidgetExplorer();
}

bool PanelController::eventFilter(QObject *watched, QEvent *event)
{
    ControllerWindow::eventFilter(watched, event);

    if (watched == m_optionsDialog && event->type() == QEvent::WindowDeactivate && (!isWidgetExplorerVisible())) {
        if (!m_settingsTool->underMouse()) {
            m_optionsDialog->hide();
        }

        if (!isActiveWindow()) {
            close();
        }
        return true;
    } else if (watched == m_moveTool) {
        if (event->type() == QEvent::MouseButtonPress) {
            m_dragging = MoveButtonElement;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = NoElement;
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            mouseMoveFilter(mouseEvent);
        }
    } else if (watched == m_sizeTool) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            m_startDragPos = mouseEvent->pos();
            m_dragging = ResizeButtonElement;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            //resets properties saved during the drag
            m_startDragPos = QPoint(0, 0);
            m_dragging = NoElement;
            setCursor(Qt::ArrowCursor);
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            mouseMoveFilter(mouseEvent);
        }
    }

    return false;
}

void PanelController::mouseMoveFilter(QMouseEvent *event)
{
    if (m_dragging == NoElement || !containment()) {
        return;
    }

    QRect screenGeom = Kephal::ScreenUtils::screenGeometry(containment()->screen());

    if (m_dragging == MoveButtonElement) {
        //only move when the mouse cursor is out of the controller to avoid an endless reposition cycle
        if (geometry().contains(event->globalPos())) {
            return;
        }

        if (!screenGeom.contains(event->globalPos())) {
            //move panel to new screen if dragged there
            int targetScreen = Kephal::ScreenUtils::screenId(event->globalPos());
            //kDebug() << "Moving panel from screen" << containment()->screen() << "to screen" << targetScreen;
            containment()->setScreen(targetScreen);
            return;
        }

        //create a dead zone so you can go across the middle without having it hop to one side
        float dzFactor = 0.35;
        QPoint offset = QPoint(screenGeom.width()*dzFactor,screenGeom.height()*dzFactor);
        QRect deadzone = QRect(screenGeom.topLeft()+offset, screenGeom.bottomRight()-offset);
        if (deadzone.contains(event->globalPos())) {
            //kDebug() << "In the deadzone:" << deadzone;
            return;
        }

        const Plasma::Location oldLocation = containment()->location();
        Plasma::Location newLocation = oldLocation;
        float screenAspect = float(screenGeom.height())/screenGeom.width();

        /* Use diagonal lines so we get predictable behavior when moving the panel
         * y=topleft.y+(x-topleft.x)*aspectratio   topright < bottomleft
         * y=bottomleft.y-(x-topleft.x)*aspectratio   topleft < bottomright
         */
        if (event->globalY() < screenGeom.y()+(event->globalX()-screenGeom.x())*screenAspect) {
            if (event->globalY() < screenGeom.bottomLeft().y()-(event->globalX()-screenGeom.x())*screenAspect) {
                if (containment()->location() == Plasma::TopEdge) {
                    return;
                } else {
                    newLocation = Plasma::TopEdge;
                }
            } else if (containment()->location() == Plasma::RightEdge) {
                    return;
            } else {
                newLocation = Plasma::RightEdge;
            }
        } else {
            if (event->globalY() < screenGeom.bottomLeft().y()-(event->globalX()-screenGeom.x())*screenAspect) {
                if (containment()->location() == Plasma::LeftEdge) {
                    return;
                } else {
                    newLocation = Plasma::LeftEdge;
                }
            } else if(containment()->location() == Plasma::BottomEdge) {
                    return;
            } else {
                newLocation = Plasma::BottomEdge;
            }
        }


        //If the orientation changed swap width and height
        if (oldLocation != newLocation) {
            emit locationChanged(newLocation);
        }

        return;
    }

    //Resize handle moved
    switch (location()) {
    case Plasma::LeftEdge: {
        int newX = mapToGlobal(event->pos()).x() - m_startDragPos.x();
        if (newX - MINIMUM_HEIGHT > screenGeom.left() &&
            newX - screenGeom.left() <= screenGeom.width()/3) {
            move(newX, pos().y());
            resizeFrameHeight(geometry().left() - screenGeom.left());
        }
        break;
    }
    case Plasma::RightEdge: {
        int newX = mapToGlobal(event->pos()).x() - m_startDragPos.x();
        if (newX + width() + MINIMUM_HEIGHT < screenGeom.right() &&
            newX + width() - screenGeom.left() >= 2*(screenGeom.width()/3)) {
            move(newX, pos().y());
            resizeFrameHeight(screenGeom.right() - geometry().right());
        }
        break;
    }
    case Plasma::TopEdge: {
        int newY = mapToGlobal(event->pos()).y() - m_startDragPos.y();
        if ( newY - MINIMUM_HEIGHT > screenGeom.top() &&
             newY - screenGeom.top()<= screenGeom.height()/3) {
            move(pos().x(), newY);
            resizeFrameHeight(geometry().top() - screenGeom.top());
        }
        break;
    }
    case Plasma::BottomEdge:
    default: {
        int newY = mapToGlobal(event->pos()).y() - m_startDragPos.y();
        if ( newY + height() + MINIMUM_HEIGHT < screenGeom.bottom() &&
             newY + height() - screenGeom.top() >= 2*(screenGeom.height()/3)) {
            move(pos().x(), newY);
            resizeFrameHeight(screenGeom.bottom() - geometry().bottom());
        }
        break;
    }
    }
}

void PanelController::focusOutEvent(QFocusEvent * event)
{
    Q_UNUSED(event)
    if (!m_optionsDialog->isActiveWindow() && !isWidgetExplorerVisible()) {
        m_optionsDialog->hide();
        close();
    }
}

ToolButton *PanelController::addTool(QAction *action, QWidget *parent, Qt::ToolButtonStyle style)
{
    ToolButton *tool = new ToolButton(parent);
    tool->setToolButtonStyle(style);
    tool->setAction(action);
    m_actionWidgets.append(tool);

    return tool;
}

ToolButton *PanelController::addTool(const QString iconName, const QString iconText, QWidget *parent, Qt::ToolButtonStyle style, bool checkButton)
{
    //TODO take advantage of setDefaultAction using the containment's actions if possible
    ToolButton *tool = new ToolButton(parent);

    KIcon icon = KIcon(iconName);
    if (!icon.isNull() && !iconName.isNull()) {
        tool->setIcon(icon);
    }

    tool->setText(iconText);
    tool->setToolButtonStyle(style);

    if (style == Qt::ToolButtonIconOnly) {
        tool->setToolTip(iconText);
    }

    tool->setCheckable(checkButton);
    tool->setAutoExclusive(checkButton);

    return tool;
}

void PanelController::resizeFrameHeight(const int newHeight)
{
    if (!containment()) {
        return;
    }

    switch (location()) {
        case Plasma::LeftEdge:
        case Plasma::RightEdge:
            containment()->resize(QSize(newHeight, (int)containment()->size().height()));
            containment()->setMinimumSize(QSize(newHeight, (int)containment()->minimumSize().height()));
            containment()->setMaximumSize(QSize(newHeight, (int)containment()->maximumSize().height()));
            break;
        case Plasma::TopEdge:
        case Plasma::BottomEdge:
        default:
            containment()->resize(QSize((int)containment()->size().width(), newHeight));
            containment()->setMinimumSize(QSize((int)containment()->minimumSize().width(), newHeight));
            containment()->setMaximumSize(QSize((int)containment()->maximumSize().width(), newHeight));
            break;
    }
}

void PanelController::rulersMoved(int offset, int minLength, int maxLength)
{
    if (!containment()) {
        return;
    }

    QSize preferredSize(containment()->preferredSize().toSize());

    switch (location()) {
        case Plasma::LeftEdge:
        case Plasma::RightEdge:
            containment()->resize(QSize((int)containment()->size().width(), qBound(minLength, preferredSize.height(), maxLength)));
            containment()->setMinimumSize(QSize((int)containment()->minimumSize().width(), minLength));
            containment()->setMaximumSize(QSize((int)containment()->maximumSize().width(), maxLength));
            break;
        case Plasma::TopEdge:
        case Plasma::BottomEdge:
        default:
            containment()->resize(QSize(qBound(minLength, preferredSize.width(), maxLength), (int)containment()->size().height()));
            containment()->setMinimumSize(QSize(minLength, (int)containment()->minimumSize().height()));
            containment()->setMaximumSize(QSize(maxLength, (int)containment()->maximumSize().height()));
            break;
    }

    emit offsetChanged(offset);
}

void PanelController::alignToggled(bool toggle)
{
    if (!toggle) {
        return;
    }

    if (sender() == m_leftAlignTool) {
        emit alignmentChanged(Qt::AlignLeft);
        m_ruler->setAlignment(Qt::AlignLeft);
    } else if (sender() == m_centerAlignTool) {
        emit alignmentChanged(Qt::AlignCenter);
        m_ruler->setAlignment(Qt::AlignCenter);
    } else if (sender() == m_rightAlignTool) {
        emit alignmentChanged(Qt::AlignRight);
        m_ruler->setAlignment(Qt::AlignRight);
    }

    emit offsetChanged(0);
    m_ruler->setOffset(0);
}

void PanelController::panelVisibilityModeChanged(bool toggle)
{
    if (!toggle) {
        return;
    }

    if (sender() == m_normalPanelTool) {
        emit panelVisibilityModeChanged(PanelView::NormalPanel);
    } else if (sender() == m_autoHideTool) {
        emit panelVisibilityModeChanged(PanelView::AutoHide);
    } else if (sender() == m_underWindowsTool) {
        emit panelVisibilityModeChanged(PanelView::LetWindowsCover);
    } else if (sender() == m_overWindowsTool) {
        emit panelVisibilityModeChanged(PanelView::WindowsGoBelow);
    }
}

void PanelController::settingsPopup()
{
    if (m_optionsDialog->isVisible()) {
        m_optionsDialog->hide();
    } else {
        KWindowSystem::setState(m_optionsDialog->winId(), NET::SkipTaskbar | NET::SkipPager | NET::Sticky);
        QPoint pos = mapToGlobal(m_settingsTool->pos());
        m_optionsDialog->layout()->activate();
        m_optionsDialog->resize(m_optionsDialog->sizeHint());
        QSize s = m_optionsDialog->size();

        switch (location()) {
            case Plasma::BottomEdge:
                pos = QPoint(pos.x(), pos.y() - s.height());
                break;
            case Plasma::TopEdge:
                pos = QPoint(pos.x(), pos.y() + m_settingsTool->size().height());
                break;
            case Plasma::LeftEdge:
                pos = QPoint(pos.x() + m_settingsTool->size().width(), pos.y());
                break;
            case Plasma::RightEdge:
                pos = QPoint(pos.x() - s.width(), pos.y());
                break;
            default:
                if (pos.y() - s.height() > 0) {
                    pos = QPoint(pos.x(), pos.y() - s.height());
                } else {
                    pos = QPoint(pos.x(), pos.y() + m_settingsTool->size().height());
                }
        }

        QRect screenRect = Kephal::ScreenUtils::screenGeometry(containment()->screen());

        if (pos.rx() + s.width() > screenRect.right()) {
            pos.rx() -= ((pos.rx() + s.width()) - screenRect.right());
        }

        if (pos.ry() + s.height() > screenRect.bottom()) {
            pos.ry() -= ((pos.ry() + s.height()) - screenRect.bottom());
        }

        pos.rx() = qMax(0, pos.rx());
        m_optionsDialog->move(pos);
        m_optionsDialog->show();
    }
}

void PanelController::syncRuler()
{
    QRect screenGeom = Kephal::ScreenUtils::screenGeometry(containment()->screen());

    switch (location()) {
        case Plasma::LeftEdge:
        case Plasma::RightEdge:
            m_ruler->setAvailableLength(screenGeom.height());
            m_ruler->setMaxLength(qMin((int)containment()->maximumSize().height(), screenGeom.height()));
            m_ruler->setMinLength(containment()->minimumSize().height());
            break;
        case Plasma::TopEdge:
        case Plasma::BottomEdge:
        default:
            m_ruler->setAvailableLength(screenGeom.width());
            m_ruler->setMaxLength(qMin((int)containment()->maximumSize().width(), screenGeom.width()));
            m_ruler->setMinLength(containment()->minimumSize().width());
            break;
    }
}

void PanelController::maximizePanel()
{
    const int length = m_ruler->availableLength();
    const int screen = containment()->screen();
    const QRect screenGeom = PlasmaApp::self()->corona()->screenGeometry(screen);
    QRegion availGeom(screenGeom);
    foreach (PanelView *view, PlasmaApp::self()->panelViews()) {
        if (view->containment() != containment() &&
            view->screen() == screen && view->visibilityMode() == PanelView::NormalPanel) {
            availGeom = availGeom.subtracted(view->geometry());
        }
    }
    int offset = 0;
    const int w = containment()->size().width();
    const int h = containment()->size().height();

    switch (location()) {
        case Plasma::LeftEdge: {
            QRect r = availGeom.intersected(QRect(0, 0, w, length)).boundingRect();
            offset = r.top();
        }
        break;

        case Plasma::RightEdge: {
            QRect r = availGeom.intersected(QRect(screenGeom.right() - w, 0, w, length)).boundingRect();
            offset = r.top();
        }
        break;

        case Plasma::TopEdge: {
            QRect r = availGeom.intersected(QRect(0, 0, length, h)).boundingRect();
            offset = r.left();
        }
        break;

        case Plasma::BottomEdge:
        default: {
            QRect r = availGeom.intersected(QRect(0, screenGeom.bottom() - h, length, h)).boundingRect();
            offset = r.left();
        }
        break;
    }

    rulersMoved(offset, length, length);
    m_ruler->setMaxLength(length);
    m_ruler->setMinLength(length);
}

void PanelController::addSpace()
{
    Plasma::Applet *spacer = containment()->addApplet("panelspacer_internal");
    if (spacer) {
        QMetaObject::invokeMethod(spacer, "updateConfigurationMode", Q_ARG(bool, true));
    }
}

#include "panelcontroller.moc"
