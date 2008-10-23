#include "layoutwidget.h"

//Taskmanager
#include <taskmanager/taskmanager.h>
#include <taskmanager/abstractgroupableitem.h>
#include <taskmanager/groupmanager.h>

// Qt
#include <QGraphicsSceneWheelEvent>
#include <QList>
#include <QGraphicsScene>
#include <QGraphicsGridLayout>
#include <QPainter>

#include <math.h>

#include "windowtaskitem.h"
#include "taskgroupitem.h"


//GroupItem Constructor
LayoutWidget::LayoutWidget(TaskGroupItem *parent, Tasks *applet)
    : QObject(parent),
      m_hasSpacer(false),
      m_spacer(0),
      m_groupItem(parent),
      m_rowSize(6), //TODO calculate a reasonable default value
      m_maxRows(1),
      m_applet(applet),
      m_layout(0)
{
    init();
    //kDebug();
    foreach(AbstractTaskItem *item, m_groupItem->memberList()) {
        addTaskItem(item);
    }
}

LayoutWidget::~LayoutWidget()
{
    //kDebug();
}

void LayoutWidget::init()
{
    createLayout();
    setOrientation(Plasma::Horizontal);
}


void LayoutWidget::constraintsChanged(Plasma::Constraints constraints)
{
    Q_ASSERT(m_applet);
    kDebug();
    if (constraints & Plasma::LocationConstraint) {
        setOrientation(m_applet->formFactor());
    }

    if (constraints & Plasma::SizeConstraint) {
        layoutItems();
    }
}

void LayoutWidget::addTaskItem(AbstractTaskItem * item)
{
//    kDebug();
    if (!item) {
        kDebug() << "invalid item";
        return;
    }
    if (m_itemPositions.contains(item)) {
	kDebug() << "already in this layout";
	return;
    }
    if (item->abstractItem()) {
        if (item->abstractItem()->isGroupItem()) {
            connect(static_cast<TaskGroupItem*>(item), SIGNAL(changed()), this, SLOT(update())); //update on expanding group
        }
    }
    if (m_groupItem->scene() && !item->scene()) {
        //kDebug() << "layout widget got scene"<<m_groupItem->scene()<< "add item to scene" <<item->scene();
        m_groupItem->scene()->addItem(item);
        //kDebug() << "itemScene" << item->scene();
    }

   if (!insert(m_groupItem->memberList().indexOf(item), item)) {
        kDebug() << "error on  insert";
        return;
    }
    item->show();
    //kDebug() << "end";
}

void LayoutWidget::removeTaskItem(AbstractTaskItem * item)
{
    if (!remove(item)) {
        return;
    }

    //kDebug();
  /*  if (item->abstractItem()->isGroupItem()) { 
        TaskGroupItem *groupItem = static_cast<TaskGroupItem*>(item);
        if (groupItem->isSplit()) {
            groupItem->unsplitGroup(); //TODO reasonable?
        }
    }*/

    if (m_groupItem->scene()) {
        //kDebug() << "got scene";
        m_groupItem->scene()->removeItem(item);
    } else {
        kDebug() << "No Scene available";
    }
    //kDebug() << "done";
}

void LayoutWidget::setOrientation(Plasma::FormFactor orientation)
{
    if (orientation == Plasma::Vertical) {
        m_orientation = Qt::Vertical;
    } else {
        m_orientation = Qt::Horizontal;
    }
}

bool LayoutWidget::insert(int index, AbstractTaskItem* item)
{
   // kDebug();
    if (!item ) {
        kDebug() << "error";
        return false;
    }

    if ((index <= numberOfItems()) && (index >= 0)) {
        m_itemPositions.insert(index, item);
    } else {
        m_itemPositions.insert(numberOfItems(), item);
    }

    layoutItems();
    return true;
}

bool LayoutWidget::remove(AbstractTaskItem* item)
{
    if (!item) {
        kDebug() << "null Item";
    }
    m_itemPositions.removeAll(item);
    layoutItems();
    return true;
}

void LayoutWidget::update()
{
    kDebug();
    layoutItems();
}


/** size including expanded groups*/
int LayoutWidget::size()
{
    int groupSize = 0;
    TaskGroupItem *group;
    foreach (AbstractTaskItem *item, m_groupItem->memberList()) {
        if (!item->abstractItem()) {
            kDebug() << "error";
            continue;
        }
        if (item->abstractItem()->isGroupItem()) {
           group = static_cast<TaskGroupItem*>(item);
            if (!group->collapsed()) { 
                LayoutWidget *layout = dynamic_cast<LayoutWidget*>(group->layoutWidget());
                if (!layout) {
                    kDebug() << "Error";
                    continue;
                }
                groupSize += layout->size();// increase number of items since expanded groups occupy several spaces
                continue;
            }
        }
        groupSize++;
    }
    kDebug() << "group size" << groupSize;
    return groupSize;
}

/** width including expanded groups*/
int LayoutWidget::rowWidth(int groupSize)
{
    int columns = m_rowSize;
    if (columns < 1) {
        //kDebug() << "divider columns is 0!!!";
        return 1;
    }

    if (m_itemPositions.count() == 0) {
        return 1;
    }

    QSizeF itemSize = m_itemPositions[0]->basicPreferredSize();

    //kDebug() << geometry().height() << int(geometry().height() / 22) << m_maxRows;
    int maxRows;
    // in this case rows are columns, columns are rows...
    if (m_applet->formFactor() == Plasma::Vertical) {
        maxRows = qMin(qMax(1, int(m_groupItem->geometry().width() / itemSize.width())), m_maxRows);
    } else {
        maxRows = qMin(qMax(1, int(m_groupItem->geometry().height() / itemSize.height())), m_maxRows);
    }
    //kDebug() << m_layout->geometry() << m_groupItem->geometry();
    
    while (ceil(static_cast<float>(groupSize)/static_cast<float>(columns)) > maxRows) {
        columns++;  // more rows needed than allowed so we add some columns instead
    }
    //kDebug() << "groupWidth" << columns << maxRows << m_maxRows;
    return qMax(1, qMin(columns, groupSize));
}


void LayoutWidget::createLayout()
{
    if (m_layout) {
	//m_groupItem->setLayout(0); //this also delets the layout
	m_layout = 0;
    }
    m_layout = new QGraphicsGridLayout(m_groupItem);
    m_layout->setContentsMargins(0,0,0,0);
    m_layout->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding));
    m_layout->setMaximumSize(INT_MAX,INT_MAX);
    setOrientation(Plasma::Horizontal);    
}

void LayoutWidget::layoutItems()
{ 
    kDebug();
    
    int totalSize = size(); //get the size including all expanded groups
    // make sure columns is not 0, as that will crash divisions.
    int columns = qMax(1, rowWidth(totalSize)); //now adjust columns if necessary 
    //kDebug() << "totalSize/columns" << totalSize << columns;
    int rows = ceil(static_cast<float>(totalSize)/static_cast<float>(columns)); //and calculate the rows (rowWidth already took the maximum rows setting into account
    kDebug() << "Laying out with" << columns << rows;
    //kDebug() << "geometry" << m_groupItem->geometry();
    int rowHeight = qMax(1, int(m_groupItem->geometry().height() / rows));
    //kDebug() << "rowHeight" << rowHeight;
    int columnWidth = qMax(1, int(m_groupItem->geometry().size().width() / columns));
    //kDebug() << "column width set to " << columnWidth;
    
    createLayout(); //its a shame that we have to create a new layout every time but the QGraphicsGridLayout is just to buggy yet

    //go through all items of this layoutwidget and populate the layout with items
    int numberOfItems = 0;
    foreach (AbstractTaskItem *item, m_itemPositions) {
        int row;
        int col;
        if (m_applet->formFactor() == Plasma::Vertical) {
            row = numberOfItems % columns; 
            col = numberOfItems / columns;
        } else {
            row = numberOfItems / columns; 
            col = numberOfItems % columns;
        }

        m_layout->setColumnPreferredWidth(col, columnWidth);//Somehow this line is absolutely crucial
        m_layout->setRowPreferredHeight(row, rowHeight);//Somehow this line is absolutely crucial

        if (item->abstractItem() && item->abstractItem()->isGroupItem()) {
            TaskGroupItem *group = static_cast<TaskGroupItem*>(item);
            if (!group->collapsed()) { 
                LayoutWidget *layout = group->layoutWidget();
                if (!layout) {
                    kDebug() << "group has no valid layout";
                    continue;
                }
                int groupRowWidth = layout->rowWidth(layout->size());

                if ((columns-col) < groupRowWidth) {//we need to split the group
                    int splitIndex = columns - col;//number of items in group that are on this row
                    TaskGroupItem *splitChild = group->splitGroup(splitIndex);
                    m_layout->addItem(item, row, col, 1, splitIndex); //Add the normal item 
                    kDebug() << "add normal item: split index = column span " << splitIndex;
                    if (splitChild) {
                        m_layout->addItem(splitChild, row + 1, 0, 1, groupRowWidth - splitIndex);//also add the second part of the group if there is one
                    }
                    kDebug() << "add split item: column span " << groupRowWidth - splitIndex;
                } else  {
                    group->unsplitGroup();
                    m_layout->addItem(item, row, col, 1, groupRowWidth); //Add the normal item 
                    kDebug() << "add unsplit expanded item over columns " << groupRowWidth;
                }
                numberOfItems += groupRowWidth - 1;
            } else {
                group->unsplitGroup();
                m_layout->addItem(item, row, col, 1, 1); 
            }

        } else {
            m_layout->addItem(item, row, col, 1, 1); 
        }

        kDebug() << "addItem at: " << row  <<  col;
        numberOfItems++;
    }

    //invalidate();
    updatePreferredSize();
    m_groupItem->setLayout(m_layout);
}


void LayoutWidget::updatePreferredSize()
{
    kDebug() << "column count: " << m_layout->columnCount();

    if (m_layout->count() > 0) {
        AbstractTaskItem *item = dynamic_cast<AbstractTaskItem *>(m_layout->itemAt(0));
        Q_ASSERT(item);
        if (orientation() == Qt::Vertical) {
            m_layout->setPreferredSize(item->basicPreferredSize().width()*m_layout->rowCount(), item->basicPreferredSize().height()*m_layout->columnCount());
        } else {
            m_layout->setPreferredSize(item->basicPreferredSize().width()*m_layout->columnCount(), item->basicPreferredSize().height()*m_layout->rowCount());
            //kDebug() << "horizontal size set";
        }
    //Empty taskbar, arbitrary small value
    } else {
        kDebug() << "Empty layout!!!!!!!!!!!!!!!!!!";
        if (orientation() == Qt::Vertical) {
            m_layout->setPreferredSize(/*m_layout->preferredSize().width()*/10, 10); //since we recreate the layout we don't have the previous values
        } else {
            m_layout->setPreferredSize(10, /*m_layout->preferredSize().height()*/10);
        }
    }
    kDebug() << "preferred size: " << m_layout->preferredSize();
    emit sizeHintChanged(Qt::PreferredSize);
}

Qt::Orientation LayoutWidget::orientation()
{
    return m_orientation;
}

void LayoutWidget::setMaximumRows(int rows)
{
    m_maxRows = rows;
}

int LayoutWidget::insertionIndexAt(const QPointF &pos)//FIXME implement vertical
{
    int insertIndex = -1;
    int row = 0;
    int col = 0;

    //if pos is (-1,-1) insert at the end of the panel
    if (pos.toPoint() == QPoint(-1, -1)) {
        kDebug() << "Error";
        return -1;
    } else {
        QRectF siblingGeometry;
        for (int i = 0; i < m_layout->rowCount(); i++) {
            siblingGeometry = m_layout->itemAt(i, 0)->geometry();//set geometry of single item
            if (pos.y() <= siblingGeometry.bottom()) {
                row = i;
                break;
            }
        }

        for (int i = 0; i < itemsInRow(row); i++) {
            siblingGeometry = m_layout->itemAt(0, i)->geometry();//set geometry of single item
            qreal horizMiddle = (siblingGeometry.left() + siblingGeometry.right()) / 2.0;
            //kDebug() << "pos middle " << pos.x() << horizMiddle;
            if (pos.x() < horizMiddle) {
                col = i;
                break;
            } else if (pos.x() <= siblingGeometry.right()) {
                col = i + 1; //take next item if there is one
                break;
            }
        }

        //kDebug() << row << col;
    }

    insertIndex = row *m_layout->columnCount() + col;

    if (insertIndex > numberOfItems()) {
        insertIndex--;
        //kDebug() << "correction";
    }

    //kDebug() << "insert Index" << insertIndex;
    return insertIndex;
}

int LayoutWidget::numberOfItems()
{
    //kDebug() << m_layout->count();
    if (m_hasSpacer) {
        //kDebug() << "spacer";
        return (m_layout->count() - 1); //last item is a Spacer
    }
    return (m_layout->count());
}

int LayoutWidget::numberOfRows()
{
    if (orientation() == Qt::Vertical) {
        return m_layout->columnCount();
    } else {
        return m_layout->rowCount();
    }
}

int LayoutWidget::itemsInRow(int row)
{
    //kDebug() << m_layout->count();
    if (row * m_rowSize < numberOfItems()) { //we are in a full row
        return m_layout->columnCount();
    } else {
        int val;
        if (orientation() == Qt::Vertical) {
            val = numberOfItems() % m_layout->rowCount();
            if (!val) {
                val = m_layout->rowCount();
            }
        } else {
            val = numberOfItems() % m_layout->columnCount();
            if (!val) {
                val = m_layout->columnCount();
            }
        }
        return val;
    }
}


#include "layoutwidget.moc"

