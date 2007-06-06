/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "desktopgrid.h"

#include <math.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <klocale.h>
#include <netwm_def.h>
#include <qevent.h>

namespace KWin
{

KWIN_EFFECT( desktopgrid, DesktopGridEffect )

const int PROGRESS_TIME = 500; // ms

DesktopGridEffect::DesktopGridEffect()
    : progress( 0 )
    , activated( false )
    , keyboard_grab( false )
    , was_window_move( false )
    , window_move( NULL )
    , slide( false )
    {
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = static_cast< KAction* >( actionCollection->addAction( "ShowDesktopGrid" ));
    a->setText( i18n("Show Desktop Grid" ));
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F8 ));
    connect( a, SIGNAL( triggered( bool )), this, SLOT( toggle()));
    }

void DesktopGridEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( slide )
        {
        progress = qMin( 1.0, progress + time / double( PROGRESS_TIME ));
        // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted more than once,
        // so with normal screen painting second screen paint would erase parts of the first paint
        if( progress != 1 )
            *mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
        else
            {
            slide = false;
            progress = 0;
            }
        }
    else if( progress != 0 || activated )
        {
        if( activated )
            progress = qMin( 1.0, progress + time / double( PROGRESS_TIME ));
        else
            progress = qMax( 0.0, progress - time / double( PROGRESS_TIME ));
        // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted more than once,
        // so with normal screen painting second screen paint would erase parts of the first paint
        if( progress != 0 )
            *mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
        if( !activated && progress == 0 )
            finish();
        int d = posToDesktop( cursorPos());
        if( d != hover_desktop )
            {
            *region |= desktopRect( hover_desktop, true );
            hover_desktop = d;
            *region |= desktopRect( hover_desktop, true );
            }
        }
    effects->prePaintScreen( mask, region, time );
    }

void DesktopGridEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( slide )
        {
        if( w->isOnAllDesktops())
            {
            if( slide_painting_sticky )
                *mask |= PAINT_WINDOW_TRANSFORMED;
            else
                w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
            }
        else if( w->isOnDesktop( painting_desktop ))
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        else
            w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        }
    else if( progress != 0 )
        {
        if( w->isOnDesktop( painting_desktop ))
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        else
            w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        if( w == window_move )
            {
            *mask |= PAINT_WINDOW_TRANSFORMED;
            if( w->isOnAllDesktops() && painting_desktop != posToDesktop( window_move_pos - window_move_diff ))
                w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
            }
        }
    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void DesktopGridEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( progress == 0 )
        {
        effects->paintScreen( mask, region, data );
        return;
        }
    if( slide )
        {
        paintSlide( mask, region, data );
        return;
        }
    int desktop_with_move = -1;
    if( window_move != NULL )
        desktop_with_move = window_move->isOnAllDesktops()
            ? posToDesktop( window_move_pos - window_move_diff ) : window_move->desktop();
    for( int desktop = 1;
         desktop <= effects->numberOfDesktops();
         ++desktop )
        {
        if( desktop != desktop_with_move )
            paintScreenDesktop( desktop, mask, region, data );
        }
    // paint the desktop with the window being moved as the last one, i.e. on top of others
    if( desktop_with_move != -1 )
        paintScreenDesktop( desktop_with_move, mask, region, data );
    }

void DesktopGridEffect::paintScreenDesktop( int desktop, int mask, QRegion region, ScreenPaintData data )
    {
    QRect rect = desktopRect( desktop, true );
    if( region.contains( rect )) // this desktop needs painting
        {
        painting_desktop = desktop;
        ScreenPaintData d = data;
        QRect normal = desktopRect( effects->currentDesktop(), false );
        d.xTranslate += rect.x(); // - normal.x();
        d.yTranslate += rect.y(); // - normal.y();
        d.xScale *= rect.width() / float( normal.width());
        d.yScale *= rect.height() / float( normal.height());
        // TODO mask parts that are not visible?
        effects->paintScreen( mask, region, d );
        }
    }

void DesktopGridEffect::paintSlide( int mask, QRegion region, const ScreenPaintData& data )
    {
    /*
     Transformations are done by remembering starting position of the change and the progress
     of it, the destination is computed from the current desktop. Positions of desktops
     are done using their topleft corner.
    */
    QPoint destPos = desktopRect( effects->currentDesktop(), false ).topLeft();
    QPoint diffPos = destPos - slide_start_pos;
    int w = 0;
    int h = 0;
    if( effects->optionRollOverDesktops())
        {
        int x, y;
        Qt::Orientation orientation;
        effects->calcDesktopLayout( &x, &y, &orientation );
        w = x * displayWidth();
        h = y * displayHeight();
        // wrap around if shorter
        if( diffPos.x() > 0 && diffPos.x() > w / 2 )
            diffPos.setX( diffPos.x() - w );
        if( diffPos.x() < 0 && abs( diffPos.x()) > w / 2 )
            diffPos.setX( diffPos.x() + w );
        if( diffPos.y() > 0 && diffPos.y() > h / 2 )
            diffPos.setY( diffPos.y() - h );
        if( diffPos.y() < 0 && abs( diffPos.y()) > h / 2 )
            diffPos.setY( diffPos.y() + h );
        }
    QPoint currentPos = slide_start_pos + progress * diffPos;
    QSize displaySize( displayWidth(), displayHeight());
    QRegion currentRegion = QRect( currentPos, displaySize );
    if( effects->optionRollOverDesktops())
        {
        currentRegion |= ( currentRegion & QRect( -w, 0, w, h )).translated( w, 0 );
        currentRegion |= ( currentRegion & QRect( 0, -h, w, h )).translated( 0, h );
        currentRegion |= ( currentRegion & QRect( w, 0, w, h )).translated( -w, 0 );
        currentRegion |= ( currentRegion & QRect( 0, h, w, h )).translated( 0, -h );
        }
    bool do_sticky = true;
    for( int desktop = 1;
         desktop <= effects->numberOfDesktops();
         ++desktop )
        {
        QRect rect = desktopRect( desktop, false );
        if( currentRegion.contains( rect )) // part of the desktop needs painting
            {
            painting_desktop = desktop;
            slide_painting_sticky = do_sticky;
            slide_painting_diff = rect.topLeft() - currentPos;
            if( effects->optionRollOverDesktops())
                {
                if( slide_painting_diff.x() > displayWidth())
                    slide_painting_diff.setX( slide_painting_diff.x() - w );
                if( slide_painting_diff.x() < -displayWidth())
                    slide_painting_diff.setX( slide_painting_diff.x() + w );
                if( slide_painting_diff.y() > displayHeight())
                    slide_painting_diff.setY( slide_painting_diff.y() - h );
                if( slide_painting_diff.y() < -displayHeight())
                    slide_painting_diff.setY( slide_painting_diff.y() + h );
                }
            do_sticky = false; // paint on-all-desktop windows only once
            ScreenPaintData d = data;
            d.xTranslate += slide_painting_diff.x();
            d.yTranslate += slide_painting_diff.y();
            // TODO mask parts that are not visible?
            effects->paintScreen( mask, region, d );
            }
        }
    }

void DesktopGridEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( slide )
        { // don't move windows on all desktops (compensate screen transformation)
        if( w->isOnAllDesktops()) // TODO also fix 'Workspace::movingClient'
            {
            data.xTranslate -= slide_painting_diff.x();
            data.yTranslate -= slide_painting_diff.y();
            }
        }
    else if( progress != 0 )
        {
        if( w == window_move )
            {
            int x, y;
            Qt::Orientation orientation;
            effects->calcDesktopLayout( &x, &y, &orientation );
            QRect desktop = desktopRect( painting_desktop, false );
            data.xTranslate += window_move_pos.x() * x - ( desktop.x() + w->x());
            data.yTranslate += window_move_pos.y() * y - ( desktop.y() + w->y());
            }
        else if( painting_desktop != hover_desktop )
            data.brightness *= 0.7;
        }
    effects->paintWindow( w, mask, region, data );    
    }

void DesktopGridEffect::postPaintScreen()
    {
    if( slide )
        effects->addRepaintFull();
    if( activated ? progress != 1 : progress != 0 )
        effects->addRepaintFull(); // trigger next animation repaint
    effects->postPaintScreen();
    }

// Gives a position of the given desktop when all desktops are arranged in a grid
QRect DesktopGridEffect::desktopRect( int desktop, bool scaled ) const
    {
    int x, y;
    Qt::Orientation orientation;
    effects->calcDesktopLayout( &x, &y, &orientation );
    --desktop; // make it start with 0
    QRect rect;
    if( orientation == Qt::Vertical )
        rect = QRect(( desktop % x ) * displayWidth(), ( desktop / x ) * displayHeight(),
            displayWidth(), displayHeight());
    else
        rect = QRect(( desktop / y ) * displayWidth(), ( desktop % y ) * displayHeight(),
            displayWidth(), displayHeight());
    if( !scaled )
        return rect;
    QRect current = desktopRect( effects->currentDesktop(), false );
    rect = QRect( qRound( interpolate( rect.x() - current.x(), rect.x() / float( x ), progress )),
        qRound( interpolate( rect.y() - current.y(), rect.y() / float( y ), progress )),
        qRound( interpolate( rect.width(), displayWidth() / float( x ), progress )),
        qRound( interpolate( rect.height(), displayHeight() / float( y ), progress )));
    return rect;
    }

int DesktopGridEffect::posToDesktop( const QPoint& pos ) const
    {
    for( int desktop = 1; // TODO could be perhaps optimized
         desktop <= effects->numberOfDesktops();
         ++desktop )
        {
        if( desktopRect( desktop, true ).contains( pos ))
            return desktop;
        }
    return 0;
    }

QRect DesktopGridEffect::windowRect( EffectWindow* w ) const
    {
    int x, y;
    Qt::Orientation orientation;
    effects->calcDesktopLayout( &x, &y, &orientation );
    if( w == window_move ) // it's being moved, return moved position
        return QRect( window_move_pos, QSize( w->width() / x, w->height() / y ));
    QRect desktop = desktopRect( w->isOnCurrentDesktop()
        ? effects->currentDesktop() : w->desktop(), true );
    return QRect( desktop.x() + w->x() / x, desktop.y() + w->y() / y,
        w->width() / x, w->height() / y );
    }

EffectWindow* DesktopGridEffect::windowAt( const QPoint& pos, QRect* rect ) const
    {
    if( window_move != NULL && windowRect( window_move ).contains( pos ))
        {
        if( rect != NULL )
            *rect = windowRect( window_move );
        return window_move; // has special position and is on top
        }
    EffectWindowList windows = effects->stackingOrder();
    // qReverse()
    EffectWindowList::Iterator begin = windows.begin();
    EffectWindowList::Iterator end = windows.end();
    --end;
    while( begin < end )
        qSwap( *begin++, *end-- );
    int x, y;
    Qt::Orientation orientation;
    effects->calcDesktopLayout( &x, &y, &orientation );
    foreach( EffectWindow* w, windows )
        {
        // don't use windowRect(), take special care of on-all-desktop windows
        QRect desktop = desktopRect( w->isOnAllDesktops()
            ? posToDesktop( pos ) : w->desktop(), true );
        QRect r( desktop.x() + w->x() / x, desktop.y() + w->y() / y,
            w->width() / x, w->height() / y );
        if( r.contains( pos ))
            {
            if( rect != NULL )
                *rect = r;
            return w;
            }
        }
    return NULL;
    }

void DesktopGridEffect::desktopChanged( int old )
    {
    if( activated )
        setActive( false );
    else
        slideDesktopChanged( old );
    }

void DesktopGridEffect::slideDesktopChanged( int old )
    {
    if( slide ) // old slide still in progress
        {
        QPoint diffPos = desktopRect( old, false ).topLeft() - slide_start_pos;
        int w = 0;
        int h = 0;
        if( effects->optionRollOverDesktops())
            {
            int x, y;
            Qt::Orientation orientation;
            effects->calcDesktopLayout( &x, &y, &orientation );
            w = x * displayWidth();
            h = y * displayHeight();
            // wrap around if shorter
            if( diffPos.x() > 0 && diffPos.x() > w / 2 )
                diffPos.setX( diffPos.x() - w );
            if( diffPos.x() < 0 && abs( diffPos.x()) > w / 2 )
                diffPos.setX( diffPos.x() + w );
            if( diffPos.y() > 0 && diffPos.y() > h / 2 )
                diffPos.setY( diffPos.y() - h );
            if( diffPos.y() < 0 && abs( diffPos.y()) > h / 2 )
                diffPos.setY( diffPos.y() + h );
            }
        QPoint currentPos = slide_start_pos + progress * diffPos;
        QRegion currentRegion = QRect( currentPos, QSize( displayWidth(), displayHeight()));
        if( effects->optionRollOverDesktops())
            {
            currentRegion |= ( currentRegion & QRect( -w, 0, w, h )).translated( w, 0 );
            currentRegion |= ( currentRegion & QRect( 0, -h, w, h )).translated( 0, h );
            currentRegion |= ( currentRegion & QRect( w, 0, w, h )).translated( -w, 0 );
            currentRegion |= ( currentRegion & QRect( 0, h, w, h )).translated( 0, -h );
            }
        QRect rect = desktopRect( effects->currentDesktop(), false );
        if( currentRegion.contains( rect ))
            { // current position is in new current desktop (e.g. quickly changing back),
              // don't do full progress
            if( abs( currentPos.x() - rect.x()) > abs( currentPos.y() - rect.y()))
                progress = 1 - abs( currentPos.x() - rect.x()) / float( displayWidth());
            else
                progress = 1 - abs( currentPos.y() - rect.y()) / float( displayHeight());
            }
        else // current position is not on current desktop, do full progress
            progress = 0;
        diffPos = rect.topLeft() - currentPos;
        // Compute starting point for this new move (given current and end positions)
        slide_start_pos = rect.topLeft() - diffPos * 1 / ( 1 - progress );
        }
    else
        {
        progress = 0;
        slide_start_pos = desktopRect( old, false ).topLeft();
        slide = true;
        }
    effects->addRepaintFull();
    }

void DesktopGridEffect::toggle()
    {
    setActive( !activated );
    }
    
void DesktopGridEffect::setActive( bool active )
    {
    if( activated == active )
        return;
    activated = active;
    if( activated && progress == 0 )
        setup();
    effects->addRepaintFull();
    }

void DesktopGridEffect::setup()
    {
    keyboard_grab = effects->grabKeyboard( this );
    input = effects->createInputWindow( this, 0, 0, displayWidth(), displayHeight(),
        Qt::PointingHandCursor );
    hover_desktop = effects->currentDesktop();
    }

void DesktopGridEffect::finish()
    {
    if( keyboard_grab )
        effects->ungrabKeyboard();
    keyboard_grab = false;
    if( window_move != NULL )
        effects->setElevatedWindow( window_move, false );
    window_move = NULL;
    effects->destroyInputWindow( input );
    effects->addRepaintFull(); // to get rid of hover
    }

void DesktopGridEffect::windowInputMouseEvent( Window, QEvent* e )
    {
    if( e->type() != QEvent::MouseMove
        && e->type() != QEvent::MouseButtonPress
        && e->type() != QEvent::MouseButtonRelease )
        return;
    QMouseEvent* me = static_cast< QMouseEvent* >( e );
    if( e->type() == QEvent::MouseMove )
        {
        // highlight desktop under mouse
        int d = posToDesktop( me->pos());
        if( d != hover_desktop )
            {
            effects->addRepaint( desktopRect( hover_desktop, true ));
            hover_desktop = d;
            effects->addRepaint( desktopRect( hover_desktop, true ));
            }
        if( window_move != NULL ) // handle window moving
            {
            was_window_move = true;
            // windowRect() handles window_move specially
            effects->addRepaint( windowRect( window_move ));
            window_move_pos = me->pos() + window_move_diff;
            effects->addRepaint( windowRect( window_move ));
            }
        }
    if( e->type() == QEvent::MouseButtonPress )
        {
        if( me->buttons() == Qt::LeftButton )
            {
            QRect rect;
            EffectWindow* w = windowAt( me->pos(), &rect );
            if( w != NULL && w->isMovable())
                { // prepare it for moving
                window_move_pos = rect.topLeft();
                window_move_diff = window_move_pos - me->pos();
                window_move = w;
                effects->setElevatedWindow( window_move, true );
                }
            }
        else if( me->buttons() == Qt::MidButton && window_move == NULL )
            {
            EffectWindow* w = windowAt( me->pos());
            if( w != NULL && w->isMovable())
                {
                if( w->isOnAllDesktops())
                    effects->windowToDesktop( w, posToDesktop( me->pos()));
                else
                    effects->windowToDesktop( w, NET::OnAllDesktops );
                effects->addRepaintFull();
                }
            }
        }
    if( e->type() == QEvent::MouseButtonRelease && me->buttons() == 0 )
        {
        if( was_window_move )
            {
            if( window_move != NULL )
                {
                QRect rect = windowRect( window_move );
                int desktop = posToDesktop( rect.center());
                // to desktop's coordinates
                rect.translate( -desktopRect( desktop, true ).topLeft());
                int x, y;
                Qt::Orientation orientation;
                effects->calcDesktopLayout( &x, &y, &orientation );
                effects->moveWindow( window_move, QPoint( rect.x() * x, rect.y() * y ));
                effects->windowToDesktop( window_move, desktop );
                window_move = NULL;
                }
            }
        if( !was_window_move && me->button() == Qt::LeftButton )
            {
            effects->setCurrentDesktop( posToDesktop( me->pos()));
            setActive( false );
            }
        was_window_move = false;
        }
    }

void DesktopGridEffect::windowClosed( EffectWindow* w )
    {
    if( w == window_move )
        {
        effects->setElevatedWindow( window_move, false );
        window_move = NULL;
        }
    }

void DesktopGridEffect::grabbedKeyboardEvent( QKeyEvent* e )
    {
    // TODO
    }


} // namespace

#include "desktopgrid.moc"
