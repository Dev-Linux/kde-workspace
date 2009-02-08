/***************************************************************************
 *   Copyright 2007 by Enrico Ros <enrico.ros@gmail.com>                   *
 *   Copyright 2007 by Riccardo Iaconelli <ruphy@kde.org>                  *
 *   Copyright 2008 by Davide Bettio <davide.bettio@kdemail.net>           *
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


#include "resultitem.h"

#include <math.h>

#include <QtCore/QTimeLine>
#include <QtCore/QDebug>
#include <QtCore/QtGlobal>
#include <QtCore/QTimer>
#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QStyleOptionGraphicsItem>
#include <QtGui/QGraphicsItemAnimation>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsView>

#include <KDebug>
#include <KIcon>

#include <Plasma/PaintUtils>
#include <Plasma/Plasma>
#include <Plasma/RunnerManager>
#include <Plasma/PaintUtils>
#include <Plasma/FrameSvg>

#define TEXT_AREA_HEIGHT ResultItem::MARGIN + ResultItem::TEXT_MARGIN*2 + ResultItem::Private::s_fontHeight
//#define NO_GROW_ANIM

void shadowBlur(QImage &image, int radius, const QColor &color);

class ResultItem::Private
{
public:
    Private(ResultItem *item, Plasma::FrameSvg *f)
        : q(item),
          match(0),
          frame(f),
          tempTransp(1.0),
          highlight(false),
          index(-1),
          rowStride(6),
          highlightTimerId(0),
          animation(0),
          needsMoving(false),
          dying(false)
    {
        if (s_fontHeight < 1) {
            //FIXME: reset when the application font changes
            QFontMetrics fm(q->font());
            s_fontHeight = fm.height();
            //kDebug() << "font height is: " << s_fontHeight;
        }
    }

    ~Private()
    {
        delete animation;
    }

    static ResultItemSignaller * signaller()
    {
        if (!s_signaller) {
            s_signaller = new ResultItemSignaller;
        }

        return s_signaller;
    }

    QPointF pos();
    void appear();
    void move();
    void init();
    //void animationComplete();

    static ResultItemSignaller *s_signaller;
    static int s_removingCount;
    static int s_fontHeight;

    ResultItem * q;
    Plasma::QueryMatch match;
    Plasma::FrameSvg *frame;
    // static description
    QIcon       icon;
    // dyn params
    QBrush bgBrush;
    QPixmap fadeout;
    qreal       tempTransp;
    int highlight;
    int index;
    int rowStride;
    int highlightTimerId;
    QGraphicsItemAnimation *animation;
    bool isFavorite : 1;
    bool needsMoving : 1;
    bool dying : 1;
};

int ResultItem::Private::s_removingCount = 0;
ResultItemSignaller* ResultItem::Private::s_signaller = 0;
int ResultItem::Private::s_fontHeight = 0;

QPointF ResultItem::Private::pos()
{
    const int x = (index % rowStride) * ResultItem::BOUNDING_WIDTH + 4;
    const int y = (index / rowStride) * (ResultItem::BOUNDING_HEIGHT + s_fontHeight) + 4;
    //kDebug() << x << y << "for" << index;
    return QPointF(x, y);
}

void ResultItem::Private::appear()
{
    if (animation) {
        q->animationComplete();
    }

    //TODO: maybe have them scatter in versus expand/spin in place into view?
    QPointF p(pos());
    qreal halfway = ResultItem::BOUNDING_WIDTH * 0.5;
    qreal mostway = ResultItem::BOUNDING_WIDTH * 0.1;

    q->setPos(p);
    q->scale(0.0, 0.0);
    q->setPos(p + QPointF(halfway, halfway));
    q->becomeVisible();
    animation = new QGraphicsItemAnimation();
    animation->setItem(q);
    animation->setScaleAt(0.5, 0.1, 1.0);
    animation->setScaleAt(1.0, 1.0, 1.0);
    animation->setPosAt(0.5, p + QPointF(mostway, 0));
    animation->setPosAt(1.0, p);
    QTimeLine * timer = new QTimeLine(100, animation);
    animation->setTimeLine(timer);

    timer->start();
   // QTimer::singleShot(50, q, SLOT(becomeVisible()));
    connect(timer, SIGNAL(finished()), q, SLOT(animationComplete()));
}

void ResultItem::Private::move()
{
    //qDebug() << "moving to" << index << rowStride;
    if (animation) {
        q->animationComplete();
    }

    QGraphicsLayoutItem *parent = q->parentLayoutItem();
    QRect contentsRect = parent ? parent->contentsRect().toRect() : q->scene()->sceneRect().toRect();

    QGraphicsItemAnimation * animation = new QGraphicsItemAnimation(q);
    animation->setItem(q);
    QTimeLine *timer = new QTimeLine(150, animation);
    timer->setCurveShape(QTimeLine::EaseOutCurve);
    animation->setTimeLine(timer);

    animation->setPosAt(1.0, pos());
    QObject::connect(timer, SIGNAL(finished()), q, SLOT(animationComplete()));
    timer->start();
}

//void ResultItem::Private::animationComplete()
void ResultItem::animationComplete()
{
    if(!d->dying) {
        delete d->animation;
        d->animation = 0;
        resetTransform();
    }
}

ResultItem::ResultItem(const Plasma::QueryMatch &match, QGraphicsWidget *parent, Plasma::FrameSvg *frame)
    : QGraphicsWidget(parent),
      d(new Private(this, frame))
{
    setMatch(match);
    d->init();
    connect(Private::signaller(), SIGNAL(animate()), this, SLOT(animate()));
    setZValue(0);
}

void ResultItem::Private::init()
{
    //QTimer * timer = new QTimer(q);
    //connect(timer, SIGNAL(timeout()), q, SLOT(slotTestTransp()));
    //timer->start(50);

    q->setFlag(QGraphicsItem::ItemIsFocusable);
    q->setFlag(QGraphicsItem::ItemIsSelectable);
    q->setAcceptHoverEvents(true);
    q->setFocusPolicy(Qt::TabFocus);
    q->resize(ITEM_SIZE, ITEM_SIZE + TEXT_AREA_HEIGHT);
}

ResultItem::~ResultItem()
{
    --Private::s_removingCount;

    if (Private::s_removingCount < 1) {
        Private::s_removingCount = 0;
        Private::signaller()->startAnimations();
    }
    delete d;
}

void ResultItem::setMatch(const Plasma::QueryMatch &match)
{
    d->match = match;
    d->icon = KIcon(match.icon());

    int hue = 0;
    switch (match.type()) {
        case Plasma::QueryMatch::CompletionMatch:
            hue = 10; // reddish
            break;
        case Plasma::QueryMatch::InformationalMatch:
        case Plasma::QueryMatch::HelperMatch:
            hue = 110; // green
            break;
        case Plasma::QueryMatch::ExactMatch:
            hue = 60; // gold
            break;
        case Plasma::QueryMatch::PossibleMatch:
        default:
            hue = 40; // browny
            break;
    }

    QColor mix =  QColor::fromHsv(hue, 160, 150);
    mix.setAlpha(180);
/*    QColor grey(61, 61, 61, 200);
    QRectF rect = boundingRect();
    QLinearGradient gr(QPointF(0, 0), geometry().bottomRight());
    gr.setColorAt(0.0, mix);
    gr.setColorAt(0.8, grey);
    gr.setColorAt(1.0, mix);
    d->bgBrush = gr;*/
    d->bgBrush = mix;
    update();
}

QString ResultItem::id() const
{
    return d->match.id();
}

bool ResultItem::compare(const ResultItem *one, const ResultItem *other)
{
    return other->d->match < one->d->match;
}

bool ResultItem::operator<(const ResultItem &other) const
{
    return d->match < other.d->match;
}

QString ResultItem::name() const
{
    return d->match.text();
}

QString ResultItem::description() const
{
    return d->match.subtext();
}

QString ResultItem::data() const
{
    return d->match.data().toString();
}

QIcon ResultItem::icon() const
{
    return d->icon;
}

Plasma::QueryMatch::Type ResultItem::group() const
{
    return d->match.type();
}

qreal ResultItem::priority() const
{
    // TODO, need to fator in more things than just this
    return d->match.relevance();
}

bool ResultItem::isFavorite() const
{
    return d->isFavorite;
}

void ResultItem::setIndex(int index)
{
    if (d->index == index) {
        return;
    }

    bool first = d->index == -1;
    d->index = index;
    d->needsMoving = false;

    if (index < 0) {
        index = -1;
        return;
    }

    //kDebug() << index << first << hasFocus();
    if (first) {
        d->appear();
    } else if (d->s_removingCount) {
        d->needsMoving = true;
    } else {
        d->move();
    }
}

void ResultItem::animate()
{
    //kDebug() << "can animate now" << d->needsMoving;
    if (d->needsMoving) {
        d->needsMoving = false;
        d->move();
    }
}

void ResultItem::becomeVisible()
{
    show();
}

int ResultItem::index() const
{
    return d->index;
}

void ResultItem::setRowStride(int stride)
{
    if (d->rowStride == stride) {
        return;
    }

    d->rowStride = stride;
    if (d->index != -1) {
        d->move();
    }
}

int ResultItem::rowStride() const
{
    return d->rowStride;
}

void ResultItem::remove()
{
    delete d->animation;
    d->animation = 0;

    QPointF p(d->pos());
    d->needsMoving = false;
    d->animation = new QGraphicsItemAnimation();
    d->animation->setItem(this);
    d->animation->setScaleAt(0.0, 1.0, 1.0);
    d->animation->setScaleAt(0.5, 0.1, 1.0);
    d->animation->setScaleAt(1.0, 0.0, 0.0);
    d->animation->setPosAt(0.0, p + QPointF(0.0, 0.0));
    d->animation->setPosAt(0.5, p + QPointF(32.0*0.9, 0.0));
    d->animation->setPosAt(1.0, p + QPointF(32.0, 32.0));
    QTimeLine * timer = new QTimeLine(150, d->animation);
    d->animation->setTimeLine(timer);
    ++Private::s_removingCount;

    connect(timer, SIGNAL(finished()), this, SLOT(deleteLater()));
    d->dying = true;
    timer->start();
}

void ResultItem::run(Plasma::RunnerManager *manager)
{
    manager->run(d->match);
}

void ResultItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    bool oldClipping = painter->hasClipping();
    painter->setClipping(false);

    QRectF rect = boundingRect().adjusted(0, 0, 0, -(TEXT_AREA_HEIGHT));
    QRect iRect = rect.toRect().adjusted(PADDING, PADDING, -PADDING, -PADDING);
    QSize iconSize = iRect.size().boundedTo(QSize(64, 64));
    int iconPadding = qMax((int(rect.width()) - iconSize.width()) / 2, 0);
    //kDebug() << iconPadding << PADDING;
    painter->setRenderHint(QPainter::Antialiasing);
    painter->save();

    // Draw background
    if (hasFocus()) {
        d->frame->setElementPrefix("hover");
    } else {
        d->frame->setElementPrefix("normal");
    }
    d->frame->resizeFrame(rect.size());
    d->frame->paintFrame(painter, rect.topLeft());

    painter->restore();
    painter->save();

        /*
        The following implements blinking Blinking
    if (hasFocus() || d->tempTransp >= 1.5) {
        painter->setOpacity(1.0);
    } else {
        qreal transp = d->tempTransp - int(d->tempTransp);
        //qDebug() << "transparency of" << transp << "from" << d->tempTransp;
        if (transp > 0.5) {
            painter->setOpacity(2.0 - (2.0 * transp));
        } else {
            painter->setOpacity(2.0 * transp);
        }

        if (!d->highlightTimerId) {
            d->highlightTimerId = startTimer(40);
        }
    }
        */

    if (!hasFocus() && d->tempTransp < 0.9) {
        painter->setOpacity(d->tempTransp);

        if (!d->highlightTimerId) {
            d->highlightTimerId = startTimer(40);
        }
    }

    bool drawMixed = false;

    if (hasFocus() || isSelected()) {
#ifdef NO_GROW_ANIM
        if (d->highlight > 2) {
#else
        // here's what the next line means:
        // we check to see if the scene has focus, but that's overridden by the mouse hovering an
        // item ... or unless we are over 2 ticks into the higlight anim. complex but it works
        if (((scene() && !scene()->views().isEmpty() && !scene()->views()[0]->hasFocus()) && !(option->state & QStyle::State_MouseOver)) || d->highlight > 2) {
#endif
            painter->drawPixmap(iconPadding, iconPadding, d->icon.pixmap(iconSize, QIcon::Active));
        } else {
            drawMixed = true;

#ifdef NO_GROW_ANIM
            ++d->highlight;
#else
            if (!d->animation) {
                ++d->highlight;
                if (d->highlight == 1) {
                    setGeometry(sceneBoundingRect().adjusted(-1, -1, 1, 1));
                } else if (d->highlight == 3) {
                    setGeometry(sceneBoundingRect().adjusted(-2, -2, 2, 2));
                }
            }
#endif

            if (!d->highlightTimerId) {
                d->highlightTimerId = startTimer(40);
            }
        }
    } else if (d->highlight > 0) {
        drawMixed = true;

#ifdef NO_GROW_ANIM
        --d->highlight;
#else
        if (!d->animation) {
            --d->highlight;
            if (d->highlight == 0) {
                setGeometry(sceneBoundingRect().adjusted(1, 1, -1, -1));
            } else if (d->highlight == 2) {
                setGeometry(sceneBoundingRect().adjusted(2, 2, -2, -2));
            }
        }
#endif

        if (!d->highlightTimerId) {
            d->highlightTimerId = startTimer(40);
        }
    } else {
        painter->drawPixmap(iconPadding, iconPadding, d->icon.pixmap(iconSize, QIcon::Disabled));
    }

    if (drawMixed) {
        qreal factor = .2;

        if (d->highlight == 1) {
            factor = .4;
        } else if (d->highlight == 2) {
            factor = .6;
        } else if (d->highlight > 2) {
            factor = .8;
        }

        qreal activeOpacity = painter->opacity() * factor;

        painter->setOpacity(painter->opacity() * (1 - factor));
        painter->drawPixmap(iconPadding, iconPadding, d->icon.pixmap(iconSize, QIcon::Disabled));
        painter->setOpacity(activeOpacity);
        painter->drawPixmap(iconPadding, iconPadding, d->icon.pixmap(iconSize, QIcon::Active));
    }
    painter->restore();

    // Draw hover/selection rects
    /*
    if (isSelected()) {
        //TODO: fancier, please
        painter->save();
        painter->translate(0.5, 0.5);
        painter->setBrush(Qt::transparent);
        painter->setPen(QPen(Qt::white, 2));
        QColor color = palette().color(QPalette::Highlight);
        if (mouseOver) {
            color = color.lighter();
        }
        //QPen pen(palette().color(QPalette::Highlight), 2);
        QPen pen(color, 2);
        painter->strokePath(Plasma::PaintUtils::roundedRectangle(rect, 6), pen);
        painter->restore();
    } else if (mouseOver) {
        painter->save();
        painter->translate(0.5, 0.5);
        QPen pen(palette().color(QPalette::Highlight).lighter(), 1);
        painter->strokePath(Plasma::PaintUtils::roundedRectangle(rect, 6), pen);
        painter->restore();
    }
    */

    QRect textRect(iRect.bottomLeft() + QPoint(0, MARGIN + TEXT_MARGIN), iRect.bottomRight() + QPoint(0, TEXT_AREA_HEIGHT));

    // Draw the text on a pixmap
    const QColor textColor = Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
    const int width = option->fontMetrics.width(name());
    QPixmap pixmap(textRect.size());
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setPen(textColor);
    p.drawText(pixmap.rect(), width > pixmap.width() ? Qt::AlignLeft : Qt::AlignCenter, name());
 
    // Fade the pixmap out at the end
    if (width > pixmap.width()) {
        if (d->fadeout.isNull() || d->fadeout.height() != pixmap.height()) {
            QLinearGradient g(0, 0, 20, 0);
            g.setColorAt(0, layoutDirection() == Qt::LeftToRight ? Qt::white : Qt::transparent);
            g.setColorAt(1, layoutDirection() == Qt::LeftToRight ? Qt::transparent : Qt::white);
            d->fadeout = QPixmap(20, textRect.height());
            d->fadeout.fill(Qt::transparent);
            QPainter p(&d->fadeout);
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.fillRect(d->fadeout.rect(), g);
        }
        const QRect r = QStyle::alignedRect(layoutDirection(), Qt::AlignRight, d->fadeout.size(), pixmap.rect());
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.drawPixmap(r.topLeft(), d->fadeout);
    }
    p.end();

    // Draw a drop shadow if we have a bright text color
    if (qGray(textColor.rgb()) > 192) {
        const int blur = 2;
        const QPoint offset(1, 1);

        QImage shadow(pixmap.size() + QSize(blur * 2, blur * 2), QImage::Format_ARGB32_Premultiplied);
        p.begin(&shadow);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(shadow.rect(), Qt::transparent);
        p.drawPixmap(blur, blur, pixmap);
        p.end();

        Plasma::PaintUtils::shadowBlur(shadow, blur, Qt::black);

        // Draw the shadow
        painter->drawImage(textRect.topLeft() - QPoint(blur, blur) + offset, shadow);
    }

    // Draw the text
    painter->drawPixmap(textRect.topLeft(), pixmap);
    painter->setClipping(oldClipping);
}

void ResultItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
    QGraphicsItem::hoverLeaveEvent(e);
    //update();
    //emit hoverLeave(this);
    setFocus(Qt::MouseFocusReason);
}

void ResultItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    QGraphicsItem::hoverEnterEvent(e);
//    update();
//    emit hoverEnter(this);
    //setFocusItem(this);
    setFocus(Qt::MouseFocusReason);
}

void ResultItem::timerEvent(QTimerEvent *e)
{
    Q_UNUSED(e)

    d->tempTransp += 0.1;
    killTimer(d->highlightTimerId);
    d->highlightTimerId = 0;

    update();
}

void ResultItem::slotTestTransp()
{
    d->tempTransp += 0.02;
    if (d->tempTransp >= 1.0)
        d->tempTransp -= 1.0;
    update();
}

void ResultItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *)
{
    emit activated(this);
}

void ResultItem::focusInEvent(QFocusEvent * event)
{
    QGraphicsWidget::focusInEvent(event);
    setZValue(1);
    //setGeometry(sceneBoundingRect().adjusted(-4, -4, 4, 4));
    if (!d->highlightTimerId) {
        d->highlightTimerId = startTimer(40);
    }
    emit hoverEnter(this);
}

void ResultItem::focusOutEvent(QFocusEvent * event)
{
    QGraphicsWidget::focusOutEvent(event);
    setZValue(0);
    //setGeometry(sceneBoundingRect().adjusted(4, 4, -4, -4));
    if (!d->highlightTimerId) {
        d->highlightTimerId = startTimer(40);
    }
    emit hoverLeave(this);
}

void ResultItem::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        emit activated(this);
    } else {
        QGraphicsWidget::keyPressEvent(event);
    }
}

QVariant ResultItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    return QGraphicsWidget::itemChange(change, value);
}

#include "resultitem.moc"

