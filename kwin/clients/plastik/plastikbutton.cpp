/* Plastik KWin window decoration
  Copyright (C) 2003 Sandro Giessl <ceebx@users.sourceforge.net>

  based on the window decoration "Web":
  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
 */

// #include <kwin/options.h>

#include <qbitmap.h>
#include <qcursor.h>
#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <kpixmap.h>
#include <kpixmapeffect.h>
#include <qtooltip.h>
#include <qtimer.h>

#include "xpm/close.xpm"
#include "xpm/minimize.xpm"
#include "xpm/maximize.xpm"
#include "xpm/restore.xpm"
#include "xpm/help.xpm"
#include "xpm/sticky.xpm"
#include "xpm/unsticky.xpm"
#include "xpm/empty.xpm"

#include "plastikbutton.h"
#include "plastikbutton.moc"
#include "plastikclient.h"
#include "misc.h"
#include "shadow.h"

namespace KWinPlastik
{

static const uint TIMERINTERVAL = 50; // msec
static const uint ANIMATIONSTEPS = 4;

PlastikButton::PlastikButton(PlastikClient *parent, const char *name,
                             const QString& tip, ButtonType type, int size, int btns)
    : QButton(parent->widget(), name),
    m_client(parent),
    m_lastMouse(0),
    m_realizeButtons(btns),
    m_size(size),
    m_type(type),
    m_aDecoLight(QImage() ), m_iDecoLight(QImage() ),
    m_aDecoDark(QImage() ), m_iDecoDark(QImage() ),
    hover(false),
    isOnAllDesktops(false),
    isMaximized(false)
{
    QToolTip::add( this, tip );
    setCursor(ArrowCursor);

    setBackgroundMode(NoBackground);

    if(m_size < 10) { m_size = 10; }

    setFixedSize(m_size, m_size);

    setDeco();

    animTmr = new QTimer(this);
    connect(animTmr, SIGNAL(timeout() ), this, SLOT(animate() ) );
    animProgress = 0;
}

PlastikButton::~PlastikButton()
{
}

QSize PlastikButton::sizeHint() const
{
    return QSize(m_size, m_size);
}

void PlastikButton::setSize(int s)
{
    m_size = s;
    if(m_size < 10) { m_size = 10; }
    setFixedSize(m_size, m_size);
    setDeco();
}

void PlastikButton::setDeco()
{
    QColor aDecoFgDark = alphaBlendColors(PlastikHandler::getColor(TitleGradientTo, true),
            Qt::black, 50);
    QColor aDecoFgLight = alphaBlendColors(PlastikHandler::getColor(TitleGradientTo, true),
            Qt::white, 50);
    QColor iDecoFgDark = alphaBlendColors(PlastikHandler::getColor(TitleGradientTo, false),
            Qt::black, 50);
    QColor iDecoFgLight = alphaBlendColors(PlastikHandler::getColor(TitleGradientTo, false),
            Qt::white, 50);

    int reduceW = 0, reduceH = 0;
    if(width()>12) {
        reduceW = static_cast<int>(2*(width()/3.5) );
    }
    else
        reduceW = 4;
    if(height()>12)
        reduceH = static_cast<int>(2*(height()/3.5) );
    else
        reduceH = 4;

    QImage img;
    switch (m_type) {
        case CloseButton:
            img = QImage(close_xpm);
            break;
        case HelpButton:
            img = QImage(help_xpm);
            break;
        case MinButton:
            img = QImage(minimize_xpm);
            break;
        case MaxButton:
            if (isMaximized) {
                img = QImage(restore_xpm);
            } else {
                img = QImage(maximize_xpm);
            }
            break;
        case OnAllDesktopsButton:
            if (isOnAllDesktops) {
                img = QImage(unsticky_xpm);
            } else {
                img = QImage(sticky_xpm);
            }
            break;
        default:
            img = QImage(empty_xpm);
            break;
    }

    m_aDecoDark = recolorImage(&img, aDecoFgDark).smoothScale(width()-reduceW, height()-reduceH);
    m_iDecoDark = recolorImage(&img, iDecoFgDark).smoothScale(width()-reduceW, height()-reduceH);
    m_aDecoLight = recolorImage(&img, aDecoFgLight).smoothScale(width()-reduceW, height()-reduceH);
    m_iDecoLight = recolorImage(&img, iDecoFgLight).smoothScale(width()-reduceW, height()-reduceH);

    this->update();
}

void PlastikButton::setTipText(const QString &tip) {
    QToolTip::remove(this );
    QToolTip::add(this, tip );
}

void PlastikButton::animate()
{
    animTmr->stop();

    if(hover) {
        if(animProgress < ANIMATIONSTEPS) {
            if (PlastikHandler::animateButtons() ) {
                animProgress++;
            } else {
                animProgress = ANIMATIONSTEPS;
            }
            animTmr->start(TIMERINTERVAL, true); // single-shot
        }
    } else {
        if(animProgress > 0) {
            if (PlastikHandler::animateButtons() ) {
                animProgress--;
            } else {
                animProgress = 0;
            }
            animTmr->start(TIMERINTERVAL, true); // single-shot
        }
    }

    repaint(false);
}

void PlastikButton::enterEvent(QEvent *e)
{
    QButton::enterEvent(e);

    hover = true;
    animate();
//     repaint(false);
}

void PlastikButton::leaveEvent(QEvent *e)
{
    QButton::leaveEvent(e);

    hover = false;
    animate();
//     repaint(false);
}

void PlastikButton::mousePressEvent(QMouseEvent* e)
{
    m_lastMouse = e->button();
    // pass on event after changing button to LeftButton
    QMouseEvent me(e->type(), e->pos(), e->globalPos(),
                   (e->button()&m_realizeButtons)?LeftButton:NoButton, e->state());

    QButton::mousePressEvent(&me);
}

void PlastikButton::mouseReleaseEvent(QMouseEvent* e)
{
    m_lastMouse = e->button();
    // pass on event after changing button to LeftButton
    QMouseEvent me(e->type(), e->pos(), e->globalPos(),
                    (e->button()&m_realizeButtons)?LeftButton:NoButton, e->state());

    QButton::mouseReleaseEvent(&me);
}

void PlastikButton::drawButton(QPainter *painter)
{
    if (!PlastikHandler::initialized())
        return;

    QRect r(0,0,width(),height());

    bool active = m_client->isActive();
    QPixmap backgroundTile = m_client->getTitleBarTile(active);
    KPixmap tempKPixmap;

    QColor highlightColor;
    if(m_type == CloseButton) {
        highlightColor = QColor(255,64,0);
    } else {
        highlightColor = Qt::white;
    }

    QColor contourTop = alphaBlendColors(PlastikHandler::getColor(TitleGradientFrom, active),
            Qt::black, 220);
    QColor contourBottom = alphaBlendColors(PlastikHandler::getColor(TitleGradientTo, active),
            Qt::black, 220);
    QColor sourfaceTop = alphaBlendColors(PlastikHandler::getColor(TitleGradientFrom, active),
            Qt::white, 220);
    QColor sourfaceBottom = alphaBlendColors(PlastikHandler::getColor(TitleGradientTo, active),
            Qt::white, 220);

    int highlightAlpha = static_cast<int>(255-((60/static_cast<double>(ANIMATIONSTEPS))*
                                          static_cast<double>(animProgress) ) );
    contourTop = alphaBlendColors(contourTop, highlightColor, highlightAlpha );
    contourBottom = alphaBlendColors(contourBottom, highlightColor, highlightAlpha);
    sourfaceTop = alphaBlendColors(sourfaceTop, highlightColor, highlightAlpha);
    sourfaceBottom = alphaBlendColors(sourfaceBottom, highlightColor, highlightAlpha);

    if (isDown() ) {
        contourTop = alphaBlendColors(contourTop, Qt::black, 200);
        contourBottom = alphaBlendColors(contourBottom, Qt::black, 200);
        sourfaceTop = alphaBlendColors(sourfaceTop, Qt::black, 200);
        sourfaceBottom = alphaBlendColors(sourfaceBottom, Qt::black, 200);
    }

    QPixmap buffer;
    buffer.resize(width(), height());
    QPainter bP(&buffer);

    // fill with the titlebar background
    bP.drawTiledPixmap(0, 0, width(), width(), backgroundTile);

    if (! (m_type == MenuButton && !hover) ) {
        // contour
        bP.setPen(contourTop);
        bP.drawLine(r.x()+2, r.y(), r.right()-2, r.y() );
        bP.drawPoint(r.x()+1, r.y()+1);
        bP.drawPoint(r.right()-1, r.y()+1);
        bP.setPen(contourBottom);
        bP.drawLine(r.x()+2, r.bottom(), r.right()-2, r.bottom() );
        bP.drawPoint(r.x()+1, r.bottom()-1);
        bP.drawPoint(r.right()-1, r.bottom()-1);
        // sides of the contour
        tempKPixmap.resize(1, r.height()-2*2);
        KPixmapEffect::gradient(tempKPixmap,
                                contourTop,
                                contourBottom,
                                KPixmapEffect::VerticalGradient);
        bP.drawPixmap(r.x(), r.y()+2, tempKPixmap);
        bP.drawPixmap(r.right(), r.y()+2, tempKPixmap);
        // sort of anti-alias for the contour
        bP.setPen(alphaBlendColors(PlastikHandler::getColor(TitleGradientFrom, active),
                contourTop, 150) );
        bP.drawPoint(r.x()+1, r.y());
        bP.drawPoint(r.right()-1, r.y());
        bP.drawPoint(r.x(), r.y()+1);
        bP.drawPoint(r.right(), r.y()+1);
        bP.setPen(alphaBlendColors(PlastikHandler::getColor(TitleGradientTo, active),
                contourBottom, 150) );
        bP.drawPoint(r.x()+1, r.bottom());
        bP.drawPoint(r.right()-1, r.bottom());
        bP.drawPoint(r.x(), r.bottom()-1);
        bP.drawPoint(r.right(), r.bottom()-1);
        // sourface
        // fill top and bottom
        bP.setPen(sourfaceTop);
        bP.drawLine(r.x()+2, r.y()+1, r.right()-2, r.y()+1 );
        bP.setPen(sourfaceBottom);
        bP.drawLine(r.x()+2, r.bottom()-1, r.right()-2, r.bottom()-1 );
        // fill the rest! :)
        tempKPixmap.resize(1, r.height()-2*2);
        KPixmapEffect::gradient(tempKPixmap,
                                sourfaceTop,
                                sourfaceBottom,
                                KPixmapEffect::VerticalGradient);
        bP.drawTiledPixmap(r.x()+1, r.y()+2, r.width()-2, r.height()-4, tempKPixmap);
    }

    if (m_type == MenuButton)
    {
        QPixmap menuIcon(m_client->icon().pixmap( QIconSet::Small, QIconSet::Normal));
        if (width() < menuIcon.width() || height() < menuIcon.height() ) {
            menuIcon.convertFromImage( menuIcon.convertToImage().smoothScale(width(), height()));
        }
        bP.drawPixmap((width()-menuIcon.width())/2, (height()-menuIcon.height())/2, menuIcon);
    }
    else
    {
        int dX,dY;
        QImage *deco = 0;
        if (isDown() ) {
            deco = active?&m_aDecoLight:&m_iDecoLight;
        } else {
            deco = active?&m_aDecoDark:&m_iDecoDark;
        }
        dX = r.x()+(r.width()-deco->width())/2;
        dY = r.y()+(r.height()-deco->height())/2;
        if (isDown() ) {
            dY++;
        }
        bP.drawImage(dX, dY, *deco);
    }

    bP.end();
    painter->drawPixmap(0, 0, buffer);
}

} // KWinPlastik
