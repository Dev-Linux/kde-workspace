/***************************************************************************
 *   Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>          *
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
#include "abstracttaskitem.h"
#include "taskgroupitem.h"

// Standard
#include <limits.h>

// Qt
#include <QApplication>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTimeLine>
#include <QGraphicsLinearLayout>

// KDE
#include <KColorScheme>
#include <KColorUtils>
#include <KGlobalSettings>
#include <KDebug>
#include <KIconLoader>

// Plasma
#include "plasma/plasma.h"
#include "plasma/theme.h"
#include "plasma/panelsvg.h"

bool AbstractTaskItem::s_backgroundCreated = false;
Plasma::PanelSvg* AbstractTaskItem::s_taskItemBackground = 0;


AbstractTaskItem::AbstractTaskItem(QGraphicsItem *parent, QObject *parentObject)
    : QGraphicsWidget(parent),
      _flags(0),
      m_animId(-1),
      m_alpha(1),
      m_fadeIn(true),
      m_updateTimerId(-1)
{
    setAcceptsHoverEvents(true);
    setupBackgroundSvg(parentObject);
    //setAcceptDrops(true);

    //FIXME:sometimes the last task is a little bit less wide than the others, so using this connection
    //causes an update frenzy and the sucking up of the cpu
    /*if (s_taskItemBackground) {
        connect(s_taskItemBackground, SIGNAL(repaintNeeded()), this, SLOT(slotUpdate()));
    }*/

    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), this, SLOT(slotUpdate()));
}

AbstractTaskItem::~AbstractTaskItem()
{
}

void AbstractTaskItem::setupBackgroundSvg(QObject *parent)
{
    if (s_backgroundCreated) {
        return;
    }

    //TODO: we should probably reset this when the theme changes
    s_backgroundCreated = true;
    QString tasksThemePath = Plasma::Theme::defaultTheme()->imagePath("widgets/tasks");
    s_taskItemBackground = 0;

    if (!tasksThemePath.isEmpty()) {
        while (parent && parent->parent()) {
            parent = parent->parent();
        }

        s_taskItemBackground = new Plasma::PanelSvg(parent);
        s_taskItemBackground->setImagePath(tasksThemePath);
        s_taskItemBackground->setCacheAllRenderedPanels(true);
    }
}

void AbstractTaskItem::animationUpdate(qreal progress)
{
    if (progress == 1) {
        m_animId = -1;
        m_fadeIn = true;
    }

    m_alpha = m_fadeIn ? progress : 1 - progress;

    // explicit update
    update();
}

void AbstractTaskItem::finished()
{
    // do something here to get the task removed
}

void AbstractTaskItem::setText(const QString &text)
{
    if (_text == text) {
        return;
    }

    _text = text;

    //let some place for at least the icon and the first character
    QFontMetrics fm(KGlobalSettings::taskbarFont());
//     setMinimumSize(QSizeF(fm.height() + fm.charWidth(text,0) + IconTextSpacing + 2, fm.height()));

    TaskGroupItem *group = qobject_cast<TaskGroupItem*>(parent());
    if (group) {
        group->queueGeometryUpdate();
    } else {
        updateGeometry();
    }
}

void AbstractTaskItem::queueUpdate()
{
    if (m_updateTimerId != -1) {
        return;
    }

    if (m_lastUpdate.elapsed() < 200) {
        if (m_updateTimerId == -1) {
            m_updateTimerId = startTimer(200);
        }

        return;
    }

    update();
    m_lastUpdate.restart();
}

void AbstractTaskItem::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_updateTimerId) {
        killTimer(m_updateTimerId);
        update();
        m_updateTimerId = -1;
    }
}

QString AbstractTaskItem::text() const
{
    return _text;
}

void AbstractTaskItem::setTaskFlags(TaskFlags flags)
{
    if (_flags & TaskWantsAttention != flags & TaskWantsAttention) {
        if (flags & TaskWantsAttention) {
            // start attention getting
        } else {
            // stop attention getting
        }
    }
    _flags = flags;
}

AbstractTaskItem::TaskFlags AbstractTaskItem::taskFlags() const
{
    return _flags;
}

void AbstractTaskItem::slotUpdate()
{
    setupBackgroundSvg(parent());
    QGraphicsItem::update();
}

void AbstractTaskItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    const int FadeInDuration = 100;

    if (m_animId != -1) {
        Plasma::Animator::self()->stopCustomAnimation(m_animId);
    }

    m_fadeIn = true;
    m_animId = Plasma::Animator::self()->customAnimation(40 / (1000 / FadeInDuration), FadeInDuration,
                                                      Plasma::Animator::LinearCurve, this,
                                                      "animationUpdate");

    QGraphicsWidget::hoverEnterEvent(event);
}

void AbstractTaskItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    const int FadeOutDuration = 200;

    if (m_animId != -1) {
        Plasma::Animator::self()->stopCustomAnimation(m_animId);
    }

    m_fadeIn = false;
    m_animId = Plasma::Animator::self()->customAnimation(40 / (1000 / FadeOutDuration), FadeOutDuration,
                                                      Plasma::Animator::LinearCurve, this,
                                                      "animationUpdate");

    QGraphicsWidget::hoverLeaveEvent(event);
}

void AbstractTaskItem::setIcon(const QIcon &icon)
{
    _icon = icon; //icon.pixmap(MinTaskIconSize);

    TaskGroupItem *group = qobject_cast<TaskGroupItem*>(parent());
    if (group) {
        group->queueGeometryUpdate();
    } else {
        updateGeometry();
    }
}

void AbstractTaskItem::drawBackground(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    // FIXME  Check the usage of KColorScheme here with various color schemes

    const qreal hoverAlpha = 0.4;

    KColorScheme colorScheme(QPalette::Active, KColorScheme::View, Plasma::Theme::defaultTheme()->colorScheme());

    if (taskFlags() & TaskWantsAttention) {
        if (s_taskItemBackground) {
            //Draw task background from theme svg "attention" element
            s_taskItemBackground->setElementPrefix("attention");
            s_taskItemBackground->resizePanel(option->rect.size());
            s_taskItemBackground->paint(painter, option->rect);
        } else {
            //Draw task background without svg theming
            QColor background = colorScheme.background(KColorScheme::ActiveBackground).color();
            background.setAlphaF(hoverAlpha+0.2);
            painter->setBrush(QBrush(background));
            painter->drawPath(Plasma::roundedRectangle(option->rect, 6));
        }
    } else if (taskFlags() & TaskIsMinimized) {
            //Not painting anything for iconified tasks for now
            painter->setBrush(QBrush());
    } else {
        if (s_taskItemBackground) {
            //Draw task background from theme svg "normal" element
            s_taskItemBackground->setElementPrefix("normal");
            s_taskItemBackground->resizePanel(option->rect.size());
            s_taskItemBackground->paint(painter, option->rect);
        } else {
            //Draw task background without svg theming
            QColor background = colorScheme.shade(colorScheme.background(KColorScheme::AlternateBackground).color(),
                                              KColorScheme::DarkShade);
            background.setAlphaF(0.2);
            painter->setBrush(QBrush(background));
            painter->setPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor));
        }
    }

    if (option->state & QStyle::State_MouseOver
         || m_animId != -1
         || taskFlags() & TaskHasFocus)
    {

        if (s_taskItemBackground) {
            if (taskFlags() & TaskHasFocus) {
                //Draw task background from theme svg "focus" element
                s_taskItemBackground->setElementPrefix("focus");
                s_taskItemBackground->resizePanel(option->rect.size());
                s_taskItemBackground->paint(painter, option->rect);
            }

            if (option->state & QStyle::State_MouseOver && m_alpha > 0) {
                //Draw task background from theme svg "hover" element
                painter->save();
                painter->setOpacity(m_alpha);

                s_taskItemBackground->setElementPrefix("hover");
                s_taskItemBackground->resizePanel(option->rect.size());
                s_taskItemBackground->paint(painter, option->rect);

                painter->restore();
            }

        } else {
            //Draw task background without svg theming
            QLinearGradient background(boundingRect().topLeft(),
                                   boundingRect().bottomLeft());

            QColor startColor;
            QColor endColor;

            if (taskFlags() & TaskHasFocus) {
                startColor = colorScheme.background(KColorScheme::NormalBackground).color();
            } else {
                startColor = colorScheme.background(KColorScheme::AlternateBackground).color();
            }

            endColor = colorScheme.shade(startColor,KColorScheme::DarkShade);

            const qreal pressedAlpha = 0.2;

            qreal alpha = 0;

            if (option->state & QStyle::State_Sunken) {
                alpha = pressedAlpha;
            } else {
                alpha = hoverAlpha;
            }

            if (!(taskFlags() & TaskHasFocus)) {
                alpha *= m_alpha;
            }

            startColor.setAlphaF(alpha);
            endColor.setAlphaF(qMin(1.0,startColor.alphaF()+0.2));

            background.setColorAt(0, startColor);
            background.setColorAt(1, endColor);

            painter->setBrush(background);
            painter->setPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor));
        }
    }

    if (!s_taskItemBackground) {
        painter->drawPath(Plasma::roundedRectangle(option->rect, 6));
    }
}

QSize AbstractTaskItem::layoutText(QTextLayout &layout, const QString &text,
                                   const QSize &constraints) const
{
    QFontMetrics metrics(layout.font());
    int leading     = metrics.leading();
    int height      = 0;
    int maxWidth    = constraints.width();
    int widthUsed   = 0;
    int lineSpacing = metrics.lineSpacing();
    QTextLine line;

    layout.setText(text);

    layout.beginLayout();
    while ((line = layout.createLine()).isValid())
    {
        height += leading;

        // Make the last line that will fit infinitely long.
        // drawTextLayout() will handle this by fading the line out
        // if it won't fit in the contraints.
        if (height + 2 * lineSpacing > constraints.height()) {
            line.setPosition(QPoint(0, height));
            break;
        }

        line.setLineWidth(maxWidth);
        line.setPosition(QPoint(0, height));

        height += int(line.height());
        widthUsed = int(qMax(qreal(widthUsed), line.naturalTextWidth()));
    }
    layout.endLayout();

    return QSize(widthUsed, height);
}

void AbstractTaskItem::drawTextLayout(QPainter *painter, const QTextLayout &layout, const QRect &rect) const
{
    if (rect.width() < 1 || rect.height() < 1) {
        return;
    }

    QPixmap pixmap(rect.size());
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setPen(painter->pen());

    // Create the alpha gradient for the fade out effect
    QLinearGradient alphaGradient(0, 0, 1, 0);
    alphaGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    if (layout.textOption().textDirection() == Qt::LeftToRight)
    {
        alphaGradient.setColorAt(0, QColor(0, 0, 0, 255));
        alphaGradient.setColorAt(1, QColor(0, 0, 0, 0));
    } else
    {
        alphaGradient.setColorAt(0, QColor(0, 0, 0, 0));
        alphaGradient.setColorAt(1, QColor(0, 0, 0, 255));
    }

    QFontMetrics fm(layout.font());
    int textHeight = layout.lineCount() * fm.lineSpacing();
    QPointF position = textHeight < rect.height() ?
            QPointF(0, (rect.height() - textHeight) / 2) : QPointF(0, 0);
    QList<QRect> fadeRects;
    int fadeWidth = 30;

    // Draw each line in the layout
    for (int i = 0; i < layout.lineCount(); i++)
    {
        QTextLine line = layout.lineAt(i);
        line.draw(&p, position);

        // Add a fade out rect to the list if the line is too long
        if (line.naturalTextWidth() > rect.width())
        {
            int x = int(qMin(line.naturalTextWidth(), (qreal)pixmap.width())) - fadeWidth;
            int y = int(line.position().y() + position.y());
            QRect r = QStyle::visualRect(layout.textOption().textDirection(), pixmap.rect(),
                                         QRect(x, y, fadeWidth, int(line.height())));
            fadeRects.append(r);
        }
    }

    // Reduce the alpha in each fade out rect using the alpha gradient
    if (!fadeRects.isEmpty())
    {
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        foreach (const QRect &rect, fadeRects)
            p.fillRect(rect, alphaGradient);
    }

    p.end();

    painter->drawPixmap(rect.topLeft(), pixmap);
}

QTextOption AbstractTaskItem::textOption() const
{
    Qt::LayoutDirection direction = QApplication::layoutDirection();
    Qt::Alignment alignment = QStyle::visualAlignment(direction, Qt::AlignLeft | Qt::AlignVCenter);

    QTextOption option;
    option.setTextDirection(direction);
    option.setAlignment(alignment);

    return option;
}

QRectF AbstractTaskItem::iconRect() const
{
    QSizeF bounds = boundingRect().size();
    //leave enough space for the text. usefull in vertical panel
    bounds.setWidth(qMax(bounds.width() / 3, qMin(minimumSize().height(), bounds.width())));

    //add the margins in the svg if the icon wouldn't become too small
    if (s_taskItemBackground) {
        //using always the "normal" element to not make the icon size jump
        s_taskItemBackground->setElementPrefix("normal");

        const int height = bounds.height() - s_taskItemBackground->marginSize(Plasma::TopMargin) - s_taskItemBackground->marginSize(Plasma::BottomMargin);
        const int width = bounds.width() - s_taskItemBackground->marginSize(Plasma::LeftMargin) - s_taskItemBackground->marginSize(Plasma::RightMargin);

        const int minIconSize = IconSize(KIconLoader::Small);

        if (height > minIconSize && width > minIconSize) {
            bounds = QSize(width, height);
        }
    }

    QSize iconSize = _icon.actualSize(bounds.toSize());

    return QStyle::alignedRect(QApplication::layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter,
                               iconSize, boundingRect().toRect());
}

QRectF AbstractTaskItem::textRect() const
{
    QSize size(boundingRect().size().toSize());
    size.rwidth() -= int(iconRect().width()) + qMin(0, IconTextSpacing - 2);

    return QStyle::alignedRect(QApplication::layoutDirection(), Qt::AlignRight | Qt::AlignVCenter,
                                     size, boundingRect().toRect());
}

void AbstractTaskItem::drawTask(QPainter *painter,
                                const QStyleOptionGraphicsItem *option,
                                QWidget *)
{
    Q_UNUSED(option)

    _icon.paint( painter , iconRect().toRect() );

#if 0
    QFont font = painter->font();
    if (taskFlags() & TaskHasFocus)
    {
        font.setBold(true);
        painter->setFont(font);
    }
#endif

    painter->setPen(QPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor), 1.0));

    QRect rect = textRect().toRect();
    rect.adjust(2, 2, -2, -2); // Create a text margin

    QTextLayout layout;
    layout.setFont(KGlobalSettings::taskbarFont());
    layout.setTextOption(textOption());

    layoutText(layout, _text, rect.size());
    drawTextLayout(painter, layout, rect);
}

void AbstractTaskItem::paint(QPainter *painter,
                             const QStyleOptionGraphicsItem *option,
                             QWidget *widget)
{
    //painter->setOpacity(opacity());
    painter->setRenderHint(QPainter::Antialiasing);

    // draw background
    drawBackground(painter, option, widget);

    // draw icon and text
    drawTask(painter, option, widget);
}

void AbstractTaskItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *)
{
    activate();
}

void AbstractTaskItem::mousePressEvent(QGraphicsSceneMouseEvent *)
{
    update();
}

void AbstractTaskItem::close()
{
    finished();
}

#include "abstracttaskitem.moc"
