/*  
    Copyright 2007 Robert Knight <robertknight@gmail.com>
    Copyright 2007 by Alexis Ménard <darktears31@gmail.com>
    
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "listview.h"

// Qt
#include <QtCore/QHash>
#include <QtCore/QPersistentModelIndex>

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QScrollBar>

#include <KDebug>

#include "itemdelegate.h"

using namespace Notifier;

class ListView::Private
{
public:
    Private(ListView *parent)
        : q(parent)
        , contentsHeight(0)
    {
    }

    void doLayout()
    {
        // clear existing layout information
        itemRects.clear();
        visualOrder.clear();

        if (!q->model()) {
            return;
        }

        int verticalOffset = ItemDelegate::TOP_OFFSET;
        int horizontalOffset = 0;
        int row = 0;
        int visualColumn = 0;

        QModelIndex branch = currentRootIndex;

        while (true) {
            if (itemChildOffsets[branch]+row >= q->model()->rowCount(branch) ||
                branch != currentRootIndex && row > MAX_CHILD_ROWS) {

                if (branch.isValid()) {
                    row = branch.row()+1;
                    branch = branch.parent();
                    continue;
                } else {
                    break;
                }
            }

            QModelIndex child = q->model()->index(row+itemChildOffsets[branch],0,branch);

            if (q->model()->hasChildren(child)) {
                QSize childSize = calculateHeaderSize(child);
                itemRects.insert(child,QRect(QPoint(ItemDelegate::HEADER_LEFT_MARGIN,verticalOffset),childSize));

                verticalOffset += childSize.height();
                horizontalOffset = 0; 
                branch = child;
                row = 0;
                visualColumn = 0;
            } else {
                QSize childSize = calculateItemSize(child);
                itemRects.insert(child,QRect(QPoint(horizontalOffset,verticalOffset),
                                             childSize));

                if (childSize.isValid()) {
                    visualOrder << child;
                }

                horizontalOffset += contentWidth() / MAX_COLUMNS;

                visualColumn++;
                row++;

                bool wasLastRow = row+itemChildOffsets[branch] >= q->model()->rowCount(branch);
                bool nextItemIsBranch = false;
                if (!wasLastRow) { 
                     QModelIndex nextIndex =q->model()->index(row+itemChildOffsets[branch],0,branch);
                     nextItemIsBranch = q->model()->hasChildren(nextIndex);
                }

                if (visualColumn >= MAX_COLUMNS || wasLastRow || nextItemIsBranch) {
                    verticalOffset += childSize.height();
                    horizontalOffset = 0; 
                    visualColumn = 0;
                }
            }
        }
        contentsHeight = verticalOffset;

        updateScrollBarRange();
    }

    void drawHeader(QPainter *painter,
                    const QModelIndex& index,
                    const QStyleOptionViewItem& option)
    {
        painter->save();
        QFont font = option.font;
        font.setBold(true);
        font.setPointSize(font.pointSize()+1);
        painter->setFont(font);
        painter->setPen(QPen(option.palette.dark(),0));
        QString text = index.data(Qt::DisplayRole).value<QString>();
        int dy = (int)(index.row() > 0 ? ItemDelegate::HEADER_HEIGHT / 4.0 : 0);
        painter->drawText(option.rect.adjusted(0, dy, 0, 0),
                          Qt::AlignVCenter|Qt::AlignLeft, text);
        painter->restore();
    }

    void updateScrollBarRange()
    {
        int pageSize = q->height();
        q->verticalScrollBar()->setRange(0,contentsHeight-pageSize);
        q->verticalScrollBar()->setPageStep(pageSize);
        q->verticalScrollBar()->setSingleStep(ItemDelegate::ITEM_HEIGHT);
    }

    int contentWidth() const
    {
        return q->width();
    }

    QSize calculateItemSize(const QModelIndex& index) const 
    {
        return  QSize(contentWidth() / MAX_COLUMNS, q->sizeHintForIndex(index).height());
    }

    QSize calculateHeaderSize(const QModelIndex& index) const
    {
	if (index.row() == 0) {
            return QSize(q->width() - ItemDelegate::HEADER_LEFT_MARGIN, ItemDelegate::FIRST_HEADER_HEIGHT);
        } else {
            return QSize(q->width() - ItemDelegate::HEADER_LEFT_MARGIN, ItemDelegate::HEADER_HEIGHT);
        }
    }

    QPoint mapFromViewport(const QPoint& point) const
    {
        return point + QPoint(0,q->verticalOffset());
    }

    QPoint mapToViewport(const QPoint& point) const
    {
        return point - QPoint(0,q->verticalOffset());
    }

    ListView * const q;
    QPersistentModelIndex currentRootIndex;
    QPersistentModelIndex hoveredIndex;

    QHash<QModelIndex,int> itemChildOffsets;
    QHash<QModelIndex,QRect> itemRects;
    QList<QModelIndex> visualOrder;

    int contentsHeight;
    
    static const int MAX_COLUMNS = 1;
    static const int MAX_CHILD_ROWS = 1000;
};

ListView::ListView(QWidget *parent)
    : QAbstractItemView(parent)
    , d(new Private(this))
{
    setIconSize(QSize(ItemDelegate::ITEM_HEIGHT,ItemDelegate::ITEM_HEIGHT));
    setMouseTracking(true);
}

ListView::~ListView()
{
    delete d;
}

QModelIndex ListView::indexAt(const QPoint& point) const
{
    // simple linear search through the item rects, this will
    // be inefficient when the viewport is large
    QHashIterator<QModelIndex,QRect> iter(d->itemRects);
    while (iter.hasNext()) {
        iter.next();
        if (iter.value().contains(d->mapFromViewport(point))) {
            return iter.key();
        }
    }
    return QModelIndex();
}

void ListView::setModel(QAbstractItemModel *model)
{
    QAbstractItemView::setModel(model);

    if (model) {
        connect(model,SIGNAL(rowsRemoved(QModelIndex,int,int)),this,SLOT(updateLayout()));
        connect(model,SIGNAL(rowsInserted(QModelIndex,int,int)),this,SLOT(updateLayout()));
        connect(model,SIGNAL(modelReset()),this,SLOT(updateLayout()));
    }

    d->currentRootIndex = QModelIndex();
    d->itemChildOffsets.clear();
    updateLayout();
}

void ListView::updateLayout()
{
    d->doLayout();

    if (!d->visualOrder.contains(currentIndex())) {
        // select the first valid index
        setCurrentIndex(moveCursor(MoveDown,0)); 
    }

    if (viewport()->isVisible()) {
        viewport()->update();
    }
}

void ListView::scrollTo(const QModelIndex& index, ScrollHint hint)
{
    QRect itemRect = d->itemRects[index];
    QRect viewedRect = QRect(d->mapFromViewport(QPoint(0,0)),
                             size());
    int topDifference = viewedRect.top()-itemRect.top();
    int bottomDifference = viewedRect.bottom()-itemRect.bottom();
    QScrollBar *scrollBar = verticalScrollBar();

    if (!itemRect.isValid())
        return;

    switch (hint) {
        case EnsureVisible:
            {
               if (!viewedRect.contains(itemRect)) {

                   if (topDifference < 0) {
                        // scroll view down
                        scrollBar->setValue(scrollBar->value() - bottomDifference); 
                   } else {
                        // scroll view up
                        scrollBar->setValue(scrollBar->value() - topDifference);
                   }
               } 
            }
            break;
        case PositionAtTop:
            {
                //d->viewportOffset = itemRect.top();
            }
        default:
            Q_ASSERT(false); // Not implemented
    }
}

QRect ListView::visualRect(const QModelIndex& index) const
{
    QRect itemRect = d->itemRects[index];
    if (!itemRect.isValid()) {
        return itemRect;
    }

    itemRect.moveTopLeft(d->mapToViewport(itemRect.topLeft()));
    return itemRect;
}

int ListView::horizontalOffset() const
{
    return 0;
}

bool ListView::isIndexHidden(const QModelIndex&) const
{
    return false;
}

QModelIndex ListView::moveCursor(CursorAction cursorAction,Qt::KeyboardModifiers )
{
    QModelIndex index = currentIndex();

    int visualIndex = d->visualOrder.indexOf(index);

    switch (cursorAction) {
        case MoveUp:
                visualIndex = qMax(0,visualIndex-1); 
            break;
        case MoveDown:
                visualIndex = qMin(d->visualOrder.count()-1,visualIndex+1);
            break;
        case MoveLeft:
        case MoveRight:
                // do nothing
            break;
        default:

            break;
    }

    // when changing the current item with the keyboard, clear the mouse-over item
    d->hoveredIndex = QModelIndex();

    return d->visualOrder.value(visualIndex,QModelIndex());
}

void ListView::setSelection(const QRect& rect,QItemSelectionModel::SelectionFlags flags)
{
    QItemSelection selection;
    selection.select(indexAt(rect.topLeft()),indexAt(rect.bottomRight()));
    selectionModel()->select(selection,flags);
}

int ListView::verticalOffset() const
{
    return verticalScrollBar()->value(); 
}

QRegion ListView::visualRegionForSelection(const QItemSelection& selection) const{
    QRegion region;
    foreach(const QModelIndex& index,selection.indexes()) {
        region |= visualRect(index);
    }
    return region;
}

void ListView::paintEvent(QPaintEvent *event)
{
    if (!model()) {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);

    QHashIterator<QModelIndex,QRect> indexIter(d->itemRects);
    while (indexIter.hasNext()) {
        indexIter.next();
        const QRect itemRect = visualRect(indexIter.key());
        const QModelIndex index = indexIter.key();

        if (event->region().contains(itemRect)) {
            QStyleOptionViewItem option = viewOptions();
            option.rect = itemRect;

            if (selectionModel()->isSelected(index)) {
                option.state |= QStyle::State_Selected;
            }
            if (index == d->hoveredIndex) {
                option.state |= QStyle::State_MouseOver;
            }
            if (index == currentIndex()) {
                option.state |= QStyle::State_HasFocus;
            }

            if (model()->hasChildren(index)) {
                d->drawHeader(&painter,index,option);
            } else {
                if (option.rect.left() == 0) {
                    option.rect.setLeft(option.rect.left() + ItemDelegate::ITEM_LEFT_MARGIN);
                    option.rect.setRight(option.rect.right() - ItemDelegate::ITEM_RIGHT_MARGIN);
                }
                itemDelegate(index)->paint(&painter,option,index);
            }
        }
    }
}

void ListView::resizeEvent(QResizeEvent *)
{
    updateLayout();
}

void ListView::mouseMoveEvent(QMouseEvent *event)
{
    const QModelIndex itemUnderMouse = indexAt(event->pos());
    if (itemUnderMouse != d->hoveredIndex && itemUnderMouse.isValid() &&
        state() == NoState) {
        update(itemUnderMouse);
        update(d->hoveredIndex);

        d->hoveredIndex = itemUnderMouse;
        setCurrentIndex(d->hoveredIndex);
    }
    QAbstractItemView::mouseMoveEvent(event);
}
#include "listview.moc"
