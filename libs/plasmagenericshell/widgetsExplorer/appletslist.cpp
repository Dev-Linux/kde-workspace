/*
 *   Copyright (C) 2009 by Ana Cecília Martins <anaceciliamb@gmail.com>
 *   Copyright (C) 2009 by Ivan Cukic <ivan.cukic+kde@gmail.com>
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

#include "appletslist.h"

#include <cmath>

#include <QHash>

#include <KIconLoader>
#include <KIcon>
#include <KPushButton>

#include <Plasma/Containment>
#include <Plasma/Corona>
#include <Plasma/ItemBackground>
#include <Plasma/Theme>
#include <Plasma/Animator>
#include <Plasma/AbstractAnimation>

#include "widgetexplorer.h"

const int ICON_SIZE = 70;
const int FILTER_APPLIANCE_DELAY = 400;
const int SEARCH_DELAY = 300;
const int TOOLTIP_APPEAR_DELAY = 1000;
const int TOOLTIP_APPEAR_WHEN_VISIBLE_DELAY = 300;
const int TOOLTIP_DISAPPEAR_DELAY = 300;
const int SCROLL_STEP_DURATION = 300;

using namespace KCategorizedItemsViewModels;

AppletsListWidget::AppletsListWidget(Qt::Orientation orientation, QGraphicsItem *parent)
    : QGraphicsWidget(parent),
      m_arrowsSvg(new Plasma::Svg(this)),
      m_appletIconBgSvg(new Plasma::FrameSvg(this)),
      m_selectionIndicator(new Plasma::ItemBackground(this)),
      m_hoverIndicator(new Plasma::ItemBackground(this)),
      m_iconSize(16)
{
    arrowClickStep = 0;
    wheelStep = 0;
    m_firstItemIndex = 0;
    m_selectedItem = 0;
    m_orientation = orientation;

    // init svg objects
    m_arrowsSvg->setImagePath("widgets/arrows");
    m_arrowsSvg->setContainsMultipleImages(true);
    m_arrowsSvg->resize(KIconLoader::SizeSmall, KIconLoader::SizeSmall);
    m_appletIconBgSvg->setImagePath("widgets/translucentbackground");

    m_slide = Plasma::Animator::create(Plasma::Animator::SlideAnimation);

    toolTipMoveTimeLine.setFrameRange(0, 100);
    toolTipMoveTimeLine.setCurveShape(QTimeLine::EaseInOutCurve);
    toolTipMoveTimeLine.setDuration(500);
    connect(&toolTipMoveTimeLine, SIGNAL(frameChanged(int)),
            this, SLOT(toolTipMoveTimeLineFrameChanged(int)));

    //init tooltip
    m_toolTip = new AppletToolTipWidget();
    m_toolTip->setVisible(false);
    connect(m_toolTip, SIGNAL(enter()), this, SLOT(onToolTipEnter()));
    connect(m_toolTip, SIGNAL(leave()), this, SLOT(onToolTipLeave()));

    init();
}

AppletsListWidget::~AppletsListWidget()
{
    delete m_toolTip;

    //FIXME: if the follow foreach looks silly, that's because it is.
    //       but Qt 4.6 currently has a devistating bug that crashes
    //       when we don't do precisely this
    foreach (QGraphicsWidget *item, m_allAppletsHash) {
        item->setParentItem(0);
        item->deleteLater();
    }

    delete m_slide;
}

void AppletsListWidget::init()
{
    //init arrows
    m_upLeftArrow = new Plasma::ToolButton(this);
    m_upLeftArrow->setPreferredSize(IconSize(KIconLoader::Panel), IconSize(KIconLoader::Panel));
    m_upLeftArrow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_downRightArrow = new Plasma::ToolButton(this);
    m_downRightArrow->setPreferredSize(IconSize(KIconLoader::Panel), IconSize(KIconLoader::Panel));
    m_downRightArrow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    if (m_orientation == Qt::Horizontal) {
        m_upLeftArrow->setIcon(KIcon(m_arrowsSvg->pixmap("left-arrow")));
        m_downRightArrow->setIcon(KIcon(m_arrowsSvg->pixmap("right-arrow")));
        m_upLeftArrow->setMaximumSize(IconSize(KIconLoader::Panel), -1);
        m_downRightArrow->setMaximumSize(IconSize(KIconLoader::Panel), -1);
    } else {
        m_upLeftArrow->setIcon(KIcon(m_arrowsSvg->pixmap("up-arrow")));
        m_downRightArrow->setIcon(KIcon(m_arrowsSvg->pixmap("down-arrow")));
        m_upLeftArrow->setMaximumSize(-1, IconSize(KIconLoader::Panel));
        m_downRightArrow->setMaximumSize(-1, IconSize(KIconLoader::Panel));
    }

    connect(m_downRightArrow, SIGNAL(pressed()), this, SLOT(onRightArrowPress()));
    connect(m_upLeftArrow, SIGNAL(pressed()), this, SLOT(onLeftArrowPress()));

    //init window that shows the applets of the list - it clips the appletsListWidget
    m_appletsListWindowWidget = new QGraphicsWidget(this);
    m_appletsListWindowWidget->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
    m_appletsListWindowWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    //init applets list
    m_appletsListWidget = new QGraphicsWidget(m_appletsListWindowWidget);
    m_appletsListWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_appletListLinearLayout = new QGraphicsLinearLayout(m_orientation, m_appletsListWidget);

    m_slide->setWidgetToAnimate(m_appletsListWidget);

    //make its events pass through its parent
    m_appletsListWidget->installEventFilter(this);
    m_appletsListWindowWidget->installEventFilter(this);

    //layouts
    m_arrowsLayout = new QGraphicsLinearLayout(m_orientation);

    m_arrowsLayout->addItem(m_upLeftArrow);
    m_arrowsLayout->addItem(m_appletsListWindowWidget);
    m_arrowsLayout->addItem(m_downRightArrow);

    m_arrowsLayout->setAlignment(m_downRightArrow, Qt::AlignVCenter | Qt::AlignHCenter);
    m_arrowsLayout->setAlignment(m_upLeftArrow, Qt::AlignVCenter | Qt::AlignHCenter);
    m_arrowsLayout->setAlignment(m_appletsListWindowWidget, Qt::AlignVCenter | Qt::AlignHCenter);

    setLayout(m_arrowsLayout);
}


//parent intercepts children events
bool AppletsListWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::GraphicsSceneResize) {
        QGraphicsWidget *widget = dynamic_cast<QGraphicsWidget *>(obj);

        if (widget == m_appletsListWidget) {
            //the resize occured with the list widget
            if (m_orientation == Qt::Horizontal) {
                m_appletsListWindowWidget->setMinimumSize(0, m_appletsListWidget->minimumHeight());
            } else {
                m_appletsListWindowWidget->setMinimumSize(m_appletsListWidget->minimumWidth(), 0);
            }

            manageArrows();
            return false;
        } else if (widget == m_appletsListWindowWidget) {
            // the resize occured with the window widget
            int maxVisibleIconsOnList = maximumAproxVisibleIconsOnList();
            arrowClickStep = ceil((float)maxVisibleIconsOnList/4);
            wheelStep = ceil((float)maxVisibleIconsOnList/2);
            return false;
        }
    }

    return QObject::eventFilter(obj, event);
}

void AppletsListWidget::setItemModel(PlasmaAppletItemModel *model)
{
    m_modelFilterItems = new DefaultItemFilterProxyModel(this);

    m_modelItems = model;
    m_modelFilterItems->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_modelFilterItems->setDynamicSortFilter(true);
    m_modelFilterItems->setSourceModel(m_modelItems);
    m_modelFilterItems->sort(0);

    populateAllAppletsHash();

    connect(m_modelFilterItems, SIGNAL(searchTermChanged(QString)), this, SLOT(updateList()));
    connect(m_modelFilterItems, SIGNAL(filterChanged()), this, SLOT(updateList()));
    connect(m_modelItems, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)),
            this, SLOT(rowsAboutToBeRemoved(const QModelIndex&, int, int)));

    updateList();
}

void AppletsListWidget::setFilterModel(QStandardItemModel *model)
{
    m_modelFilters = model;
}

void AppletsListWidget::setOrientation(Qt::Orientation orientation)
{
    m_orientation = orientation;
    setContentsPropertiesAccordingToOrientation();
    updateList();
}

void AppletsListWidget::setContentsPropertiesAccordingToOrientation()
{
    m_appletListLinearLayout->invalidate();
    m_appletListLinearLayout->setOrientation(m_orientation);
    m_arrowsLayout->setOrientation(m_orientation);

    if(m_orientation == Qt::Horizontal) {
        m_upLeftArrow->nativeWidget()->setIcon(KIcon(QIcon(m_arrowsSvg->pixmap("left-arrow"))));
        m_downRightArrow->nativeWidget()->setIcon(KIcon(QIcon(m_arrowsSvg->pixmap("right-arrow"))));
        m_upLeftArrow->setMaximumSize(IconSize(KIconLoader::Panel), -1);
        m_downRightArrow->setMaximumSize(IconSize(KIconLoader::Panel), -1);
    } else {
        m_upLeftArrow->nativeWidget()->setIcon(KIcon(QIcon(m_arrowsSvg->pixmap("up-arrow"))));
        m_downRightArrow->nativeWidget()->setIcon(KIcon(QIcon(m_arrowsSvg->pixmap("down-arrow"))));
        m_upLeftArrow->setMaximumSize(-1, IconSize(KIconLoader::Panel));
        m_downRightArrow->setMaximumSize(-1, IconSize(KIconLoader::Panel));
    }

    m_appletListLinearLayout->activate();
}

void AppletsListWidget::filterChanged(int index)
{
    if (m_modelFilterItems) {
        QStandardItem *item = m_modelFilters->item(index);

        if (item) {
            m_dataFilterAboutToApply = item->data();
            //wait a little before filtering the list
            m_filterApplianceTimer.start(FILTER_APPLIANCE_DELAY, this);
        }
    }
}

void AppletsListWidget::searchTermChanged(const QString &text)
{
    m_searchString = text;
    m_searchDelayTimer.start(SEARCH_DELAY, this);
}

void AppletsListWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_searchDelayTimer.timerId()) {
        m_modelFilterItems->setSearch(m_searchString);
        m_searchDelayTimer.stop();
    } else if (event->timerId() == m_toolTipAppearTimer.timerId()) {
        setToolTipPosition();
        m_toolTip->updateContent();
        m_toolTip->setVisible(true);
        m_toolTipAppearTimer.stop();
    } else if (event->timerId() == m_toolTipAppearWhenAlreadyVisibleTimer.timerId()) {
        m_toolTip->updateContent();
        setToolTipPosition();
        m_toolTipAppearWhenAlreadyVisibleTimer.stop();
    } else if (event->timerId() == m_toolTipDisappearTimer.timerId()) {
        m_toolTip->setVisible(false);
        m_toolTipDisappearTimer.stop();
    } else if (event->timerId() == m_filterApplianceTimer.timerId()) {
        m_modelFilterItems->setFilter(qVariantValue<KCategorizedItemsViewModels::Filter>
                                      (m_dataFilterAboutToApply));
        m_filterApplianceTimer.stop();
    }

    QGraphicsWidget::timerEvent(event);
}

QVariant AppletsListWidget::itemChange(GraphicsItemChange change, const QVariant & value)
{
    if (change == QGraphicsItem::ItemSceneHasChanged) {
        m_toolTip->setScene(scene());
    }

    return QGraphicsWidget::itemChange(change, value);
}

void AppletsListWidget::appletIconHoverEnter(AppletIconWidget *applet)
{
    if (!m_toolTip->isVisible()) {
        m_toolTip->setAppletIconWidget(applet);
        m_toolTipAppearTimer.start(TOOLTIP_APPEAR_DELAY, this);
    } else {
        if(m_toolTip->appletIconWidget()->appletItem() &&
            !(m_toolTip->appletIconWidget()->appletItem()->pluginName() ==
            applet->appletItem()->pluginName())) {
            m_toolTip->setAppletIconWidget(applet);

            //small delay, so if one's hovering very fast over the icons,
            //the tooltip doesn't appear frantically
            m_toolTipAppearWhenAlreadyVisibleTimer.start(TOOLTIP_APPEAR_WHEN_VISIBLE_DELAY, this);
        }
        m_toolTipDisappearTimer.stop();
    }

    m_hoverIndicator->setTargetItem(applet);
    if (!m_hoverIndicator->isVisible()) {
        m_hoverIndicator->setGeometry(applet->geometry());
        m_hoverIndicator->show();
    }
}

void AppletsListWidget::appletIconHoverLeave(AppletIconWidget *applet)
{
    Q_UNUSED(applet)

    if (m_toolTip->isVisible()) {
        m_toolTipDisappearTimer.start(TOOLTIP_DISAPPEAR_DELAY, this);
    } else {
        m_toolTipAppearTimer.stop();
    }
}

void AppletsListWidget::onToolTipEnter()
{
    m_toolTipDisappearTimer.stop();
}

void AppletsListWidget::onToolTipLeave()
{
    m_toolTipDisappearTimer.start(TOOLTIP_DISAPPEAR_DELAY, this);
}

void AppletsListWidget::setToolTipPosition()
{
    QPointF appletPosition = m_toolTip->appletIconWidget()->mapToItem(this, 0, 0);
    QRectF appletRect = m_toolTip->appletIconWidget()->
                        mapRectToItem(this, m_toolTip->appletIconWidget()->boundingRect());

    toolTipMoveFrom = m_toolTip->pos();

    Plasma::Corona *corona = static_cast<Plasma::WidgetExplorer*>(parentItem())->corona();
    if (corona) {
        toolTipMoveTo = corona->popupPosition(m_toolTip->appletIconWidget(), m_toolTip->geometry().size());
    } else {
        toolTipMoveTo = QPoint(appletPosition.x(), appletPosition.y());
    }

    if (m_toolTip->isVisible()) {
        animateToolTipMove();
    } else {
        m_toolTip->move(toolTipMoveTo);
    }
}

void AppletsListWidget::insertAppletIcon(AppletIconWidget *appletIconWidget)
{
    if (appletIconWidget != 0) {
        appletIconWidget->setVisible(true);
        m_appletListLinearLayout->addItem(appletIconWidget);
        m_appletListLinearLayout->setAlignment(appletIconWidget, Qt::AlignHCenter);
    }
}

int AppletsListWidget::maximumAproxVisibleIconsOnList()
{
    qreal windowSize;
    qreal listTotalSize;
    qreal iconAverageSize;
    qreal maxVisibleIconsOnList;

    if (m_orientation == Qt::Horizontal) {
        windowSize = m_appletsListWindowWidget->geometry().width();
        listTotalSize = m_appletListLinearLayout->preferredSize().width();
    } else {
        windowSize = m_appletsListWindowWidget->geometry().height();
        listTotalSize = m_appletListLinearLayout->preferredSize().height();
    }

    iconAverageSize = listTotalSize/
                      (m_currentAppearingAppletsOnList.count()) +
                       m_appletListLinearLayout->spacing();
//    approximatelly
    maxVisibleIconsOnList = floor(windowSize/iconAverageSize);

    return maxVisibleIconsOnList;
}

AppletIconWidget *AppletsListWidget::createAppletIcon(PlasmaAppletItem *appletItem)
{
    AppletIconWidget *applet = new AppletIconWidget(m_appletsListWidget, appletItem, m_appletIconBgSvg);
    applet->setMinimumSize(100, 0);
    qreal l, t, r, b;
    m_hoverIndicator->getContentsMargins(&l, &t, &r, &b);
    applet->setContentsMargins(l, t, r, b);

    if (m_iconSize != AppletIconWidget::DEFAULT_ICON_SIZE) {
        applet->setIconSize(m_iconSize);
    }

    connect(applet, SIGNAL(hoverEnter(AppletIconWidget*)), this, SLOT(appletIconHoverEnter(AppletIconWidget*)));
    connect(applet, SIGNAL(hoverLeave(AppletIconWidget*)), this, SLOT(appletIconHoverLeave(AppletIconWidget*)));
    connect(applet, SIGNAL(selected(AppletIconWidget*)), this, SLOT(itemSelected(AppletIconWidget*)));
    connect(applet, SIGNAL(doubleClicked(AppletIconWidget*)), this, SLOT(appletIconDoubleClicked(AppletIconWidget*)));

    return applet;
}

void AppletsListWidget::itemSelected(AppletIconWidget *applet)
{
    if (m_selectedItem) {
        m_selectedItem->setSelected(false);
    }

    applet->setSelected(true);
    m_selectedItem = applet;
    m_selectionIndicator->setTargetItem(m_selectedItem);
}

void AppletsListWidget::appletIconDoubleClicked(AppletIconWidget *applet)
{
    emit(appletDoubleClicked(applet->appletItem()));
}

void AppletsListWidget::eraseList()
{
    QList<QGraphicsItem *> applets = m_appletsListWidget->childItems();
    foreach (QGraphicsItem *applet, applets) {
        applet->setVisible(false);
    }
}

void AppletsListWidget::updateList()
{
    m_appletsListWidget->setLayout(NULL);
    m_appletListLinearLayout = new QGraphicsLinearLayout(m_orientation);
    m_appletListLinearLayout->setSpacing(0);

    m_currentAppearingAppletsOnList.clear();
    eraseList();

    for (int i = 0; i < m_modelFilterItems->rowCount(); i++) {
        PlasmaAppletItem *appletItem = static_cast<PlasmaAppletItem*>(getItemByProxyIndex(m_modelFilterItems->index(i, 0)));

        if (appletItem && m_allAppletsHash.contains(appletItem->id())) {
            AppletIconWidget *appletIconWidget = m_allAppletsHash.value(appletItem->id());
            insertAppletIcon(appletIconWidget);
            m_currentAppearingAppletsOnList.append(appletIconWidget);
        }
    }

    m_appletsListWidget->setLayout(m_appletListLinearLayout);
    m_appletsListWidget->adjustSize();

    updateGeometry();
    m_hoverIndicator->hide();
    resetScroll();
}

void AppletsListWidget::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    ScrollPolicy side;
    bool canScroll;

    side = event->delta() < 0 ? AppletsListWidget::DownRight : AppletsListWidget::UpLeft;

    if (side == AppletsListWidget::DownRight) {
        canScroll = m_downRightArrow->isEnabled();
    } else {
        canScroll = m_upLeftArrow->isEnabled();
    }

    if (canScroll) {
        scroll(side, AppletsListWidget::Wheel);
    }
}

void AppletsListWidget::onRightArrowPress()
{
    scroll(AppletsListWidget::DownRight, AppletsListWidget::Wheel);
}

void AppletsListWidget::onLeftArrowPress()
{
    scroll(AppletsListWidget::UpLeft, AppletsListWidget::Wheel);
}

void AppletsListWidget::scroll(ScrollPolicy side, ScrollPolicy how)
{
    int step = (how == AppletsListWidget::Wheel) ? wheelStep : arrowClickStep;

    m_toolTip->setVisible(false);

    switch (side) {
        case AppletsListWidget::DownRight:
            scrollDownRight(step);
            break;
        case AppletsListWidget::UpLeft:
            scrollUpLeft(step);
            break;
        default:
            break;
    }
}

void AppletsListWidget::scrollDownRight(int step)
{
    int nextFirstItemIndex = m_firstItemIndex + step;
    if (nextFirstItemIndex > m_currentAppearingAppletsOnList.count() - 1) {
        nextFirstItemIndex = m_currentAppearingAppletsOnList.count() - 1;
    }
    if (nextFirstItemIndex < 0) {
        return;
    }
    
    qreal startPosition = itemPosition(nextFirstItemIndex);
    qreal endPosition = startPosition + (visibleEndPosition() - visibleStartPosition());
    
    // would we show empty space at the end?
    if (endPosition > listSize()) {
        // find a better first item
        qreal searchPosition = startPosition - (endPosition - listSize());
        int i;
        for (i = 0; i < m_currentAppearingAppletsOnList.count(); i++) {
            if (itemPosition(i) >= searchPosition) {
                nextFirstItemIndex = i;
                startPosition = itemPosition(nextFirstItemIndex);
                endPosition = startPosition + (visibleEndPosition() - visibleStartPosition());
                break;
            }
        }
    }
    
    qreal move = startPosition - visibleStartPosition();
    
    m_firstItemIndex = nextFirstItemIndex;
    
    if (m_orientation == Qt::Horizontal) {
        m_slide->setDirection(Plasma::MoveLeft);
    } else {
        m_slide->setDirection(Plasma::MoveUp);
    }
    m_slide->setDistance(move);
    m_slide->start();
    
    manageArrows();
}

void AppletsListWidget::scrollUpLeft(int step)
{
    int nextFirstItemIndex = m_firstItemIndex - step;
    if (nextFirstItemIndex < 0) {
        nextFirstItemIndex = 0;
    }
    if (nextFirstItemIndex > m_currentAppearingAppletsOnList.count() - 1) {
        return;
    }
    
    qreal startPosition = itemPosition(nextFirstItemIndex);
    qreal move = startPosition - visibleStartPosition();
    
    m_firstItemIndex = nextFirstItemIndex;
    
    if (m_orientation == Qt::Horizontal) {
        m_slide->setDirection(Plasma::MoveLeft);
    } else {
        m_slide->setDirection(Plasma::MoveUp);
    }
    m_slide->setDistance(move);
    m_slide->start();
    
    manageArrows();
}

void AppletsListWidget::animateToolTipMove( )
{
    if ( toolTipMoveTimeLine.state() != QTimeLine::Running && toolTipMoveFrom != toolTipMoveTo) {
         toolTipMoveTimeLine.start();
    }
}

void AppletsListWidget::toolTipMoveTimeLineFrameChanged(int frame)
{
    QPoint newPos;

    newPos = QPoint(
            (frame/(qreal)100) * (toolTipMoveTo.x() - toolTipMoveFrom.x()) + toolTipMoveFrom.x(),
            (frame/(qreal)100) * (toolTipMoveTo.y() - toolTipMoveFrom.y()) + toolTipMoveFrom.y());

    m_toolTip->move(newPos);
}

void AppletsListWidget::resetScroll()
{
    m_appletsListWidget->setPos(0,0);
    m_firstItemIndex = 0;
    manageArrows();
}

void AppletsListWidget::manageArrows()
{
    qreal list_size = listSize();
    qreal window_size = windowSize();
    
    if (list_size <= window_size || m_currentAppearingAppletsOnList.count() == 0) {
        m_upLeftArrow->setEnabled(false);
        m_downRightArrow->setEnabled(false);
        m_upLeftArrow->setVisible(false);
        m_downRightArrow->setVisible(false);
    } else {
        qreal endPosition = itemPosition(m_firstItemIndex) + window_size;
        m_upLeftArrow->setVisible(true);
        m_downRightArrow->setVisible(true);
        m_upLeftArrow->setEnabled(m_firstItemIndex > 0);
        m_downRightArrow->setEnabled(endPosition < list_size);
    }
}

QRectF AppletsListWidget::visibleListRect()
{
    QRectF visibleRect = m_appletsListWindowWidget->
                         mapRectToItem(m_appletsListWidget, 0, 0,
                                          m_appletsListWindowWidget->geometry().width(),
                                          m_appletsListWindowWidget->geometry().height());

    return visibleRect;
}

qreal AppletsListWidget::visibleStartPosition()
{
    if (m_orientation == Qt::Horizontal) {
        return m_appletsListWindowWidget->mapToItem(m_appletsListWidget, m_appletsListWindowWidget->boundingRect().left(), 0).x();
    } else {
        return m_appletsListWindowWidget->mapToItem(m_appletsListWidget, 0, m_appletsListWindowWidget->boundingRect().top()).y();
    }
}

qreal AppletsListWidget::visibleEndPosition()
{
    if (m_orientation == Qt::Horizontal) {
        return m_appletsListWindowWidget->mapToItem(m_appletsListWidget, m_appletsListWindowWidget->boundingRect().right(), 0).x();
    } else {
        return m_appletsListWindowWidget->mapToItem(m_appletsListWidget, 0, m_appletsListWindowWidget->boundingRect().bottom()).y();
    }
}

qreal AppletsListWidget::listSize()
{
    if (m_orientation == Qt::Horizontal) {
        return m_appletsListWidget->boundingRect().size().width();
    } else {
        return m_appletsListWidget->boundingRect().size().height();
    }
}

qreal AppletsListWidget::windowSize()
{
    return (visibleEndPosition() - visibleStartPosition());
}

qreal AppletsListWidget::itemPosition(int i)
{
    AppletIconWidget *applet = m_currentAppearingAppletsOnList.at(i);
    
    if (m_orientation == Qt::Horizontal) {
        return applet->pos().x();
    } else {
        return applet->pos().y();
    }
}

void AppletsListWidget::populateAllAppletsHash()
{
    qDeleteAll(m_allAppletsHash);
    m_allAppletsHash.clear();

    const int indexesCount = m_modelFilterItems->rowCount();
    for (int i = 0; i < indexesCount ; i++) {
        PlasmaAppletItem *appletItem = static_cast<PlasmaAppletItem*>(getItemByProxyIndex(m_modelFilterItems->index(i, 0)));
        m_allAppletsHash.insert(appletItem->id(), createAppletIcon(appletItem));
    }
}

AbstractItem *AppletsListWidget::getItemByProxyIndex(const QModelIndex &index) const
{
    return (AbstractItem *)m_modelItems->itemFromIndex(m_modelFilterItems->mapToSource(index));
}

QList <AbstractItem *> AppletsListWidget::selectedItems() const
{
//    return m_appletList->selectedItems();
    return QList<AbstractItem *>();
}

void AppletsListWidget::setIconSize(int size)
{
    if (m_iconSize == size || size < 16) {
        return;
    }

    m_iconSize = size;

    foreach (AppletIconWidget *applet, m_allAppletsHash) {
        applet->setIconSize(size);
    }

    adjustSize();
}

int AppletsListWidget::iconSize() const
{
    return m_iconSize;
}

void AppletsListWidget::rowsAboutToBeRemoved(const QModelIndex& parent, int row, int column)
{
    Q_UNUSED(parent)
    Q_UNUSED(column)
    PlasmaAppletItem *item = dynamic_cast<PlasmaAppletItem*>(m_modelItems->item(row));
    if (item) {
        m_allAppletsHash.remove(item->id());
        updateList();
        m_toolTip->hide();
    }
}
