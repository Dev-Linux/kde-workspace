/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 This file contains things relevant to geometry, i.e. workspace size,
 window positions and window sizes.

*/

#include "client.h"
#include "workspace.h"

#include <kapplication.h>
#include <kglobal.h>
#include <qpainter.h>
#include <kwin.h>

#include "placement.h"
#include "notifications.h"
#include "geometrytip.h"

extern Time qt_x_time;

namespace KWinInternal
{

//********************************************
// Workspace
//********************************************

/*!
  Resizes the workspace after an XRANDR screen size change
 */
void Workspace::desktopResized()
    {
    updateClientArea();
    if (options->electricBorders() == Options::ElectricAlways)
        { // update electric borders
        destroyBorderWindows();
        createBorderWindows();
        }
    }

/*!
  Updates the current client areas according to the current clients.

  If the area changes or force is true, the new areas are propagated to the world.

  The client area is the area that is available for clients (that
  which is not taken by windows like panels, the top-of-screen menu
  etc).

  \sa clientArea()
 */

void Workspace::updateClientArea( bool force )
    {
    QRect* new_areas = new QRect[ numberOfDesktops() + 1 ];
    QRect all = QApplication::desktop()->geometry();
    for( int i = 1;
         i <= numberOfDesktops();
         ++i )
         new_areas[ i ] = all;
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it)
        {
        QRect r = (*it)->adjustedClientArea( all );
        if( r == all )
            continue;
        if( (*it)->isOnAllDesktops())
            for( int i = 1;
                 i <= numberOfDesktops();
                 ++i )
                new_areas[ i ] = new_areas[ i ].intersect( r );
        else
            new_areas[ (*it)->desktop() ] = new_areas[ (*it)->desktop() ].intersect( r );
        }
    if( topmenu_space != NULL )
        {
        QRect topmenu_area = all;
        topmenu_area.setTop( topMenuHeight());
        for( int i = 1;
             i <= numberOfDesktops();
             ++i )
            new_areas[ i ] = new_areas[ i ].intersect( topmenu_area );
        }

    bool changed = force;
    for( int i = 1;
         !changed && i <= numberOfDesktops();
         ++i )
        if( workarea[ i ] != new_areas[ i ] )
            changed = true;
    if ( changed )
        {
        delete[] workarea;
        workarea = new_areas;
        new_areas = NULL;
        NETRect r;
        for( int i = 1; i <= numberOfDesktops(); i++)
            {
            r.pos.x = workarea[ i ].x();
            r.pos.y = workarea[ i ].y();
            r.size.width = workarea[ i ].width();
            r.size.height = workarea[ i ].height();
            rootInfo->setWorkArea( i, r );
            }

        updateTopMenuGeometry();
        for( ClientList::ConstIterator it = clients.begin();
             it != clients.end();
             ++it)
            (*it)->checkWorkspacePosition();
        }
    delete[] new_areas;
    }

void Workspace::updateClientArea()
    {
    updateClientArea( false );
    }


/*!
  returns the area available for clients. This is the desktop
  geometry minus windows on the dock.  Placement algorithms should
  refer to this rather than geometry().

  \sa geometry()
 */
QRect Workspace::clientArea( clientAreaOption opt, const QPoint& p, int desktop ) const
    {
    if( desktop == NETWinInfo::OnAllDesktops || desktop == 0 )
        desktop = currentDesktop();
    QRect rect = QApplication::desktop()->geometry();
    QDesktopWidget *desktopwidget = KApplication::desktop();

    switch (opt)
        {
        case MaximizeArea:
        case MaximizeFullArea:
            if (options->xineramaMaximizeEnabled)
                rect = desktopwidget->screenGeometry(desktopwidget->screenNumber(p));
            break;
        case PlacementArea:
            if (options->xineramaPlacementEnabled)
                rect = desktopwidget->screenGeometry(desktopwidget->screenNumber(p));
            break;
        case MovementArea:
            if (options->xineramaMovementEnabled)
                rect = desktopwidget->screenGeometry(desktopwidget->screenNumber(p));
            break;
        case WorkArea:
        case FullArea:
            break; // nothing
        case ScreenArea:
            rect = desktopwidget->screenGeometry(desktopwidget->screenNumber(p));
            break;
        }

    if( workarea[ desktop ].isNull() || opt == FullArea || opt == MaximizeFullArea
        || opt == ScreenArea || opt == MovementArea )
        return rect;

    return workarea[ desktop ].intersect(rect);
    }

QRect Workspace::clientArea( clientAreaOption opt, const Client* c ) const
    {
    return clientArea( opt, c->geometry().center(), c->desktop());
    }

/*!
  Client \a c is moved around to position \a pos. This gives the
  workspace the opportunity to interveniate and to implement
  snap-to-windows functionality.
 */
QPoint Workspace::adjustClientPosition( Client* c, QPoint pos )
    {
   //CT 16mar98, 27May98 - magics: BorderSnapZone, WindowSnapZone
   //CT adapted for kwin on 25Nov1999
   //aleXXX 02Nov2000 added second snapping mode
    if (options->windowSnapZone || options->borderSnapZone )
        {
        bool sOWO=options->snapOnlyWhenOverlapping;
        QRect maxRect = clientArea(MovementArea, pos+c->rect().center(), c->desktop());
        int xmin = maxRect.left();
        int xmax = maxRect.right()+1;               //desk size
        int ymin = maxRect.top();
        int ymax = maxRect.bottom()+1;

        int cx(pos.x());
        int cy(pos.y());
        int cw(c->width());
        int ch(c->height());
        int rx(cx+cw);
        int ry(cy+ch);                 //these don't change

        int nx(cx), ny(cy);                         //buffers
        int deltaX(xmax);
        int deltaY(ymax);   //minimum distance to other clients

        int lx, ly, lrx, lry; //coords and size for the comparison client, l

      // border snap
        int snap = options->borderSnapZone; //snap trigger
        if (snap)
            {
            if ((sOWO?(cx<xmin):true) && (QABS(xmin-cx)<snap))
                {
                deltaX = xmin-cx;
                nx = xmin;
                }
            if ((sOWO?(rx>xmax):true) && (QABS(rx-xmax)<snap) && (QABS(xmax-rx) < deltaX))
                {
                deltaX = rx-xmax;
                nx = xmax - cw;
                }

            if ((sOWO?(cy<ymin):true) && (QABS(ymin-cy)<snap))
                {
                deltaY = ymin-cy;
                ny = ymin;
                }
            if ((sOWO?(ry>ymax):true) && (QABS(ry-ymax)<snap) && (QABS(ymax-ry) < deltaY))
                {
                deltaY =ry-ymax;
                ny = ymax - ch;
                }
            }

      // windows snap
        snap = options->windowSnapZone;
        if (snap)
            {
            QValueList<Client *>::ConstIterator l;
            for (l = clients.begin();l != clients.end();++l )
                {
                if ((*l)->isOnDesktop(currentDesktop()) &&
                   !(*l)->isMinimized()
                    && (*l) != c )
                    {
                    lx = (*l)->x();
                    ly = (*l)->y();
                    lrx = lx + (*l)->width();
                    lry = ly + (*l)->height();

                    if ( (( cy <= lry ) && ( cy  >= ly  ))  ||
                         (( ry >= ly  ) && ( ry  <= lry ))  ||
                         (( cy <= ly  ) && ( ry >= lry  )) )
                        {
                        if ((sOWO?(cx<lrx):true) && (QABS(lrx-cx)<snap) && ( QABS(lrx -cx) < deltaX) )
                            {
                            deltaX = QABS( lrx - cx );
                            nx = lrx;
                            }
                        if ((sOWO?(rx>lx):true) && (QABS(rx-lx)<snap) && ( QABS( rx - lx )<deltaX) )
                            {
                            deltaX = QABS(rx - lx);
                            nx = lx - cw;
                            }
                        }

                    if ( (( cx <= lrx ) && ( cx  >= lx  ))  ||
                         (( rx >= lx  ) && ( rx  <= lrx ))  ||
                         (( cx <= lx  ) && ( rx >= lrx  )) )
                        {
                        if ((sOWO?(cy<lry):true) && (QABS(lry-cy)<snap) && (QABS( lry -cy ) < deltaY))
                            {
                            deltaY = QABS( lry - cy );
                            ny = lry;
                            }
                  //if ( (QABS( ry-ly ) < snap) && (QABS( ry - ly ) < deltaY ))
                        if ((sOWO?(ry>ly):true) && (QABS(ry-ly)<snap) && (QABS( ry - ly ) < deltaY ))
                            {
                            deltaY = QABS( ry - ly );
                            ny = ly - ch;
                            }
                        }
                    }
                }
            }
        pos = QPoint(nx, ny);
        }
    return pos;
    }

/*!
  Marks the client as being moved around by the user.
 */
void Workspace::setClientIsMoving( Client *c )
    {
    Q_ASSERT(!c || !movingClient); // Catch attempts to move a second
    // window while still moving the first one.
    movingClient = c;
    if (movingClient)
        setFocusChangeEnabled( false );
    else
        setFocusChangeEnabled( true );
    }

/*!
  Cascades all clients on the current desktop
 */
void Workspace::cascadeDesktop()
    {
    Q_ASSERT( block_stacking_updates == 0 );
    ClientList::ConstIterator it(stackingOrder().begin());
    bool re_init_cascade_at_first_client = true;
    for (; it != stackingOrder().end(); ++it)
        {
        if((!(*it)->isOnDesktop(currentDesktop())) ||
           ((*it)->isMinimized())                  ||
           ((*it)->isOnAllDesktops())              ||
           (!(*it)->isMovable()) )
            continue;
        initPositioning->placeCascaded(*it, re_init_cascade_at_first_client);
        //CT is an if faster than an attribution??
        if (re_init_cascade_at_first_client)
          re_init_cascade_at_first_client = false;
        }
    }

/*!
  Unclutters the current desktop by smart-placing all clients
  again.
 */
void Workspace::unclutterDesktop()
    {
    ClientList::Iterator it(clients.fromLast());
    for (; it != clients.end(); --it)
        {
        if((!(*it)->isOnDesktop(currentDesktop())) ||
           ((*it)->isMinimized())                  ||
           ((*it)->isOnAllDesktops())              ||
           (!(*it)->isMovable()) )
            continue;
        initPositioning->placeSmart(*it);
        }
    }


void Workspace::updateTopMenuGeometry( Client* c )
    {
    if( !managingTopMenus())
        return;
    if( c != NULL )
        {
        XEvent ev;
        ev.xclient.display = qt_xdisplay();
        ev.xclient.type = ClientMessage;
        ev.xclient.window = c->window();
        static Atom msg_type_atom = XInternAtom( qt_xdisplay(), "_KDE_TOPMENU_MINSIZE", False );
        ev.xclient.message_type = msg_type_atom;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = qt_x_time;
        ev.xclient.data.l[1] = topmenu_space->width();
        ev.xclient.data.l[2] = topmenu_space->height();
        ev.xclient.data.l[3] = 0;
        ev.xclient.data.l[4] = 0;
        XSendEvent( qt_xdisplay(), c->window(), False, NoEventMask, &ev );
        KWin::setStrut( c->window(), 0, 0, topmenu_height, 0 ); // so that kicker etc. know
        c->checkWorkspacePosition();
        return;
        }
    // c == NULL - update all, including topmenu_space
    QRect area;
    area = clientArea( MaximizeFullArea, QPoint( 0, 0 ), 1 ); // HACK desktop ?
    area.setHeight( topMenuHeight());
    topmenu_space->setGeometry( area );
    for( ClientList::ConstIterator it = topmenus.begin();
         it != topmenus.end();
         ++it )
        updateTopMenuGeometry( *it );
    }

//********************************************
// Client
//********************************************


/*!
  Returns \a area with the client's strut taken into account.

  Used from Workspace in updateClientArea.
 */
// TODO move to Workspace?
QRect Client::adjustedClientArea( const QRect& area ) const
    {
    QRect r = area;
    // topmenu area is reserved in updateClientArea()
    if( isTopMenu())
        return r;
    NETStrut strut = info->strut();
    if ( strut.left > 0 )
        r.setLeft( r.left() + (int) strut.left );
    if ( strut.top > 0 )
        r.setTop( r.top() + (int) strut.top );
    if ( strut.right > 0  )
        r.setRight( r.right() - (int) strut.right );
    if ( strut.bottom > 0  )
        r.setBottom( r.bottom() - (int) strut.bottom );
    return r;
    }



// updates differences to workarea edges for all directions
void Client::updateWorkareaDiffs()
    {
    QRect area = workspace()->clientArea( WorkArea, this );
    QRect geom = geometry();
    workarea_diff_x = computeWorkareaDiff( geom.left(), geom.right(), area.left(), area.right());
    workarea_diff_y = computeWorkareaDiff( geom.top(), geom.bottom(), area.top(), area.bottom());
    }

// If the client was inside workarea in the x direction, and if it was close to the left/right
// edge, return the distance from the left/right edge (negative for left, positive for right)
// INT_MIN means 'not inside workarea', INT_MAX means 'not near edge'.
// In order to recognize 'at the left workarea edge' from 'at the right workarea edge'
// (i.e. negative vs positive zero), the distances are one larger in absolute value than they
// really are (i.e. 5 pixels from the left edge is -6, not -5). A bit hacky, but I'm lazy
// to rewrite it just to make it nicer. If this will ever get touched again, perhaps then.
// the y direction is done the same, just the values will be rotated: top->left, bottom->right
int Client::computeWorkareaDiff( int left, int right, int a_left, int a_right )
    {
    int left_diff = left - a_left;
    int right_diff = a_right - right;
    if( left_diff < 0 || right_diff < 0 )
        return INT_MIN;
    else // fully inside workarea in this direction direction
        {
        // max distance from edge where it's still considered to be close and is kept at that distance
        int max_diff = ( a_right - a_left ) / 10;
        if( left_diff < right_diff )
            return left_diff < max_diff ? -left_diff - 1 : INT_MAX;
        else if( left_diff > right_diff )
            return right_diff < max_diff ? right_diff + 1 : INT_MAX;
        return INT_MAX; // not close to workarea edge
        }
    }

void Client::checkWorkspacePosition()
    {
    if( maximizeMode() != MaximizeRestore )
	// TODO update geom_restore?
        changeMaximize( false, false, true ); // adjust size

    if( isFullScreen())
        {
        QRect area = workspace()->clientArea( MaximizeFullArea, this );
        if( geometry() != area )
            setGeometry( area );
        return;
        }
    if( isDock())
        return;
    if( isOverride())
        return; // I wish I knew what to do here :(
    if( isTopMenu())
        {
        if( workspace()->managingTopMenus())
            {
            QRect area;
            ClientList mainclients = mainClients();
            if( mainclients.count() == 1 )
                area = workspace()->clientArea( MaximizeFullArea, mainclients.first());
            else
                area = workspace()->clientArea( MaximizeFullArea, QPoint( 0, 0 ), desktop());
            area.setHeight( workspace()->topMenuHeight());
//            kdDebug() << "TOPMENU size adjust: " << area << ":" << this << endl;
            setGeometry( area );
            }
        return;
        }

    if( !isShade()) // TODO
        {
        int old_diff_x = workarea_diff_x;
        int old_diff_y = workarea_diff_y;
        updateWorkareaDiffs();

        // this can be true only if this window was mapped before KWin
        // was started - in such case, don't adjust position to workarea,
        // because the window already had its position, and if a window
        // with a strut altering the workarea would be managed in initialization
        // after this one, this window would be moved
        if( workspace()->initializing())
            return;

        QRect area = workspace()->clientArea( WorkArea, this );
        QRect new_geom = geometry();
        QRect tmp_rect_x( new_geom.left(), 0, new_geom.width(), 0 );
        QRect tmp_area_x( area.left(), 0, area.width(), 0 );
        checkDirection( workarea_diff_x, old_diff_x, tmp_rect_x, tmp_area_x );
        // the x<->y swapping
        QRect tmp_rect_y( new_geom.top(), 0, new_geom.height(), 0 );
        QRect tmp_area_y( area.top(), 0, area.height(), 0 );
        checkDirection( workarea_diff_y, old_diff_y, tmp_rect_y, tmp_area_y );
        new_geom = QRect( tmp_rect_x.left(), tmp_rect_y.left(), tmp_rect_x.width(), tmp_rect_y.width());
        QRect final_geom( new_geom.topLeft(), adjustedSize( new_geom.size()));
        if( final_geom != new_geom ) // size increments, or size restrictions
            { // adjusted size differing matters only for right and bottom edge
            if( old_diff_x != INT_MAX && old_diff_x > 0 )
                final_geom.moveRight( area.right() - ( old_diff_x - 1 ));
            if( old_diff_y != INT_MAX && old_diff_y > 0 )
                final_geom.moveBottom( area.bottom() - ( old_diff_y - 1 ));
            }
        if( final_geom != geometry() )
            setGeometry( final_geom );
        //    updateWorkareaDiffs(); done already by setGeometry()
        }
    }

// Try to be smart about keeping the clients visible.
// If the client was fully inside the workspace before, try to keep
// it still inside the workarea, possibly moving it or making it smaller if possible,
// and try to keep the distance from the nearest workarea edge.
// On the other hand, it it was partially moved outside of the workspace in some direction,
// don't do anything with that direction if it's still at least partially visible. If it's
// not visible anymore at all, make sure it's visible at least partially
// again (not fully, as that could(?) be potentionally annoying) by
// moving it slightly inside the workarea (those '+ 5').
// Again, this is done for the x direction, y direction will be done by x<->y swapping
void Client::checkDirection( int new_diff, int old_diff, QRect& rect, const QRect& area )
    {
    if( old_diff != INT_MIN ) // was inside workarea
        {
        if( old_diff == INT_MAX ) // was in workarea, but far from edge
            {
            if( new_diff == INT_MIN )  // is not anymore fully in workarea
                {
                rect.setLeft( area.left());
                rect.setRight( area.right());
                }
            return;
            }
        if( isResizable())
            {
            if( rect.width() > area.width())
                rect.setWidth( area.width());
            if( rect.width() >= area.width() / 2 )
                {
                if( old_diff < 0 )
                    rect.setLeft( area.left() + ( -old_diff - 1 ) );
                else // old_diff > 0
                    rect.setRight( area.right() - ( old_diff - 1 ));
                }
            }
        if( isMovable())
            {
            if( old_diff < 0 ) // was in left third, keep distance from left edge
                rect.moveLeft( area.left() + ( -old_diff - 1 ));
            else // old_diff > 0 // was in right third, keep distance from right edge
                rect.moveRight( area.right() - ( old_diff - 1 ));
            }
        // this isResizable() block is copied from above, the difference is
        // the condition with 'area.width() / 2' - for windows that are not wide,
        // moving is preffered to resizing
        if( isResizable())
            {
            if( old_diff < 0 )
                rect.setLeft( area.left() + ( -old_diff - 1 ) );
            else // old_diff > 0
                rect.setRight( area.right() - ( old_diff - 1 ));
            }
        }
    if( rect.right() < area.left() + 5 || rect.left() > area.right() - 5 )
        { // not visible (almost) at all - try to make it at least partially visible
        if( isMovable())
            {
            if( rect.left() < area.left() + 5 )
                rect.moveRight( area.left() + 5 );
            if( rect.right() > area.right() - 5 )
                rect.moveLeft( area.right() - 5 );
            }
        }
    }

/*!
  Adjust the frame size \a frame according to he window's size hints.
 */
QSize Client::adjustedSize( const QSize& frame, Sizemode mode ) const
    {
    // first, get the window size for the given frame size s

    QSize wsize( frame.width() - ( border_left + border_right ),
             frame.height() - ( border_top + border_bottom ));

    return sizeForClientSize( wsize, mode );
    }

/*!
  Calculate the appropriate frame size for the given client size \a
  wsize.

  \a wsize is adapted according to the window's size hints (minimum,
  maximum and incremental size changes).

 */
QSize Client::sizeForClientSize( const QSize& wsize, Sizemode mode ) const
    {
    int w = wsize.width();
    int h = wsize.height();
    if (w<1) w = 1;
    if (h<1) h = 1;

    // basesize, minsize, maxsize, paspect and resizeinc have all values defined,
    // even if they're not set in flags - see getWmNormalHints()
    QSize min_size( xSizeHint.min_width, xSizeHint.min_height );
    QSize max_size( xSizeHint.max_width, xSizeHint.max_height );
    if( decoration != NULL )
        {
        QSize decominsize = decoration->minimumSize();
        QSize border_size( border_left + border_right, border_top + border_bottom );
        if( border_size.width() > decominsize.width()) // just in case
            decominsize.setWidth( border_size.width());
        if( border_size.height() > decominsize.height())
            decominsize.setHeight( border_size.height());
        if( decominsize.width() > min_size.width())
                min_size.setWidth( decominsize.width());
        if( decominsize.height() > min_size.height())
                min_size.setHeight( decominsize.height());
        }
    w = QMIN( max_size.width(), w );
    h = QMIN( max_size.height(), h );
    w = QMAX( min_size.width(), w );
    h = QMAX( min_size.height(), h );

    int width_inc = xSizeHint.width_inc;
    int height_inc = xSizeHint.height_inc;
    int basew_inc = xSizeHint.min_width; // see getWmNormalHints()
    int baseh_inc = xSizeHint.min_height;
    w = int(( w - basew_inc ) / width_inc ) * width_inc + basew_inc;
    h = int(( h - baseh_inc ) / height_inc ) * height_inc + baseh_inc;
// code for aspect ratios based on code from FVWM
    /*
     * The math looks like this:
     *
     * minAspectX    dwidth     maxAspectX
     * ---------- <= ------- <= ----------
     * minAspectY    dheight    maxAspectY
     *
     * If that is multiplied out, then the width and height are
     * invalid in the following situations:
     *
     * minAspectX * dheight > minAspectY * dwidth
     * maxAspectX * dheight < maxAspectY * dwidth
     *
     */
    if( xSizeHint.flags & PAspect )
        {
        double min_aspect_w = xSizeHint.min_aspect.x; // use doubles, because the values can be MAX_INT
        double min_aspect_h = xSizeHint.min_aspect.y; // and multiplying would go wrong otherwise
        double max_aspect_w = xSizeHint.max_aspect.x;
        double max_aspect_h = xSizeHint.max_aspect.y;
        w -= xSizeHint.base_width;
        h -= xSizeHint.base_height;
        int max_width = max_size.width() - xSizeHint.base_width;
        int min_width = min_size.width() - xSizeHint.base_width;
        int max_height = max_size.height() - xSizeHint.base_height;
        int min_height = min_size.height() - xSizeHint.base_height;
#define ASPECT_CHECK_GROW_W \
        if( min_aspect_w * h > min_aspect_h * w ) \
            { \
            int delta = int( min_aspect_w * h / min_aspect_h - w ) / width_inc * width_inc; \
            if( w + delta <= max_width ) \
                w += delta; \
            }
#define ASPECT_CHECK_SHRINK_H_GROW_W \
        if( min_aspect_w * h > min_aspect_h * w ) \
            { \
            int delta = int( h - w * min_aspect_h / min_aspect_w ) / height_inc * height_inc; \
            if( h - delta >= min_height ) \
                h -= delta; \
            else \
                { \
                int delta = int( min_aspect_w * h / min_aspect_h - w ) / width_inc * width_inc; \
                if( w + delta <= max_width ) \
                    w += delta; \
                } \
            }
#define ASPECT_CHECK_GROW_H \
        if( max_aspect_w * h < max_aspect_h * w ) \
            { \
            int delta = int( w * max_aspect_h / max_aspect_w - h ) / height_inc * height_inc; \
            if( h + delta <= max_height ) \
                h += delta; \
            }
#define ASPECT_CHECK_SHRINK_W_GROW_H \
        if( max_aspect_w * h < max_aspect_h * w ) \
            { \
            int delta = int( w - max_aspect_w * h / max_aspect_h ) / width_inc * width_inc; \
            if( w - delta >= min_width ) \
                w -= delta; \
            else \
                { \
                int delta = int( w * max_aspect_h / max_aspect_w - h ) / height_inc * height_inc; \
                if( h + delta <= max_height ) \
                    h += delta; \
                } \
            }
        switch( mode )
            {
            case SizemodeAny:
                {
                ASPECT_CHECK_SHRINK_H_GROW_W
                ASPECT_CHECK_SHRINK_W_GROW_H
                ASPECT_CHECK_GROW_H
                ASPECT_CHECK_GROW_W
                break;
                }
            case SizemodeFixedW:
                {
                // the checks are order so that attempts to modify height are first
                ASPECT_CHECK_GROW_H
                ASPECT_CHECK_SHRINK_H_GROW_W
                ASPECT_CHECK_SHRINK_W_GROW_H
                ASPECT_CHECK_GROW_W
                break;
                }
            case SizemodeFixedH:
                {
                ASPECT_CHECK_GROW_W
                ASPECT_CHECK_SHRINK_W_GROW_H
                ASPECT_CHECK_SHRINK_H_GROW_W
                ASPECT_CHECK_GROW_H
                break;
                }
            case SizemodeMax:
                {
                // first checks that try to shrink
                ASPECT_CHECK_SHRINK_H_GROW_W
                ASPECT_CHECK_SHRINK_W_GROW_H
                ASPECT_CHECK_GROW_W
                ASPECT_CHECK_GROW_H
                break;
                }
            case SizemodeShaded:
                break;
            }
#undef ASPECT_CHECK_SHRINK_H_GROW_W
#undef ASPECT_CHECK_SHRINK_W_GROW_H
#undef ASPECT_CHECK_GROW_W
#undef ASPECT_CHECK_GROW_H
        w += xSizeHint.base_width;
        h += xSizeHint.base_height;
        }

    if ( mode == SizemodeShaded && wsize.height() == 0 )
        h = 0;
    return QSize( w + border_left + border_right, h + border_top + border_bottom );
    }

/*!
  Gets the client's normal WM hints and reconfigures itself respectively.
 */
void Client::getWmNormalHints()
    {
    long msize;
    if (XGetWMNormalHints(qt_xdisplay(), window(), &xSizeHint, &msize) == 0 )
        xSizeHint.flags = 0;
    // set defined values for the fields, even if they're not in flags

    // basesize is just like minsize, except for minsize is not used for aspect ratios
    // keep basesize only for aspect ratios, for size increments, keep the base
    // value in minsize - see ICCCM 4.1.2.3
    if( xSizeHint.flags & PBaseSize )
        {
        if( ! ( xSizeHint.flags & PMinSize )) // PBaseSize and PMinSize are equivalent
            {
            xSizeHint.flags |= PMinSize;
            xSizeHint.min_width = xSizeHint.base_width;
                xSizeHint.min_height = xSizeHint.base_height;
            }
        }
    else
        xSizeHint.base_width = xSizeHint.base_height = 0;
    if( ! ( xSizeHint.flags & PMinSize ))
        xSizeHint.min_width = xSizeHint.min_height = 0;
    if( ! ( xSizeHint.flags & PMaxSize ))
        xSizeHint.max_width = xSizeHint.max_height = INT_MAX;
    if( xSizeHint.flags & PResizeInc )
        {
        xSizeHint.width_inc = kMax( xSizeHint.width_inc, 1 );
        xSizeHint.height_inc = kMax( xSizeHint.height_inc, 1 );
        }
    else
        {
        xSizeHint.width_inc = 1;
        xSizeHint.height_inc = 1;
        }
    if( xSizeHint.flags & PAspect )
        { // no dividing by zero
        xSizeHint.min_aspect.y = kMax( xSizeHint.min_aspect.y, 1 );
        xSizeHint.max_aspect.y = kMax( xSizeHint.max_aspect.y, 1 );
        }
    else
        {
        xSizeHint.min_aspect.x = 1;
        xSizeHint.min_aspect.y = INT_MAX;
        xSizeHint.max_aspect.x = INT_MAX;
        xSizeHint.max_aspect.y = 1;
        }
    if( ! ( xSizeHint.flags & PWinGravity ))
        xSizeHint.win_gravity = NorthWestGravity;
    if( isManaged())
        { // update to match restrictions
        QSize new_size = adjustedSize( size());
        if( new_size != size() && !isShade()) // SHADE
            resizeWithChecks( new_size );
        }
    updateAllowedActions(); // affects isResizeable()
    }

/*!
  Auxiliary function to inform the client about the current window
  configuration.

 */
void Client::sendSyntheticConfigureNotify()
    {
    XConfigureEvent c;
    c.type = ConfigureNotify;
    c.send_event = True;
    c.event = window();
    c.window = window();
    c.x = x() + clientPos().x();
    c.y = y() + clientPos().y();
    c.width = clientSize().width();
    c.height = clientSize().height();
    c.border_width = 0;
    c.above = None;
    c.override_redirect = 0;
    XSendEvent( qt_xdisplay(), c.event, TRUE, StructureNotifyMask, (XEvent*)&c );
    }

const QPoint Client::calculateGravitation( bool invert, int gravity ) const
    {
    int dx, dy;
    dx = dy = 0;

    if( gravity == 0 ) // default (nonsense) value for the argument
        gravity = xSizeHint.win_gravity;

// dx, dy specify how the client window moves to make space for the frame
    switch (gravity)
        {
        case NorthWestGravity: // move down right
        default:
            dx = border_left;
            dy = border_top;
            break;
        case NorthGravity: // move right
            dx = 0;
            dy = border_top;
            break;
        case NorthEastGravity: // move down left
            dx = -border_right;
            dy = border_top;
            break;
        case WestGravity: // move right
            dx = border_left;
            dy = 0;
            break;
        case CenterGravity:
            break; // will be handled specially
        case StaticGravity: // don't move
            dx = 0;
            dy = 0;
            break;
        case EastGravity: // move left
            dx = -border_right;
            dy = 0;
            break;
        case SouthWestGravity: // move up right
            dx = border_left ;
            dy = -border_bottom;
            break;
        case SouthGravity: // move up
            dx = 0;
            dy = -border_bottom;
            break;
        case SouthEastGravity: // move up left
            dx = -border_right;
            dy = -border_bottom;
            break;
        }
    if( gravity != CenterGravity )
        { // translate from client movement to frame movement
        dx -= border_left;
        dy -= border_top;
        }
    else
        { // center of the frame will be at the same position client center without frame would be
        dx = - ( border_left + border_right ) / 2;
        dy = - ( border_top + border_bottom ) / 2;
        }
    if( !invert )
        return QPoint( x() + dx, y() + dy );
    else
        return QPoint( x() - dx, y() - dy );
    }

void Client::configureRequest( int value_mask, int rx, int ry, int rw, int rh, int gravity )
    {
    if( gravity == 0 ) // default (nonsense) value for the argument
        gravity = xSizeHint.win_gravity;
    if( value_mask & ( CWX | CWY ))
        {
        QPoint new_pos = calculateGravitation( true, gravity ); // undo gravitation
        if ( value_mask & CWX )
            new_pos.setX( rx );
        if ( value_mask & CWY )
            new_pos.setY( ry );

        // clever workaround for applications like xv that want to set
        // the location to the current location but miscalculate the
        // frame size due to kwin being a double-reparenting window
        // manager
        if ( new_pos.x() == x() + clientPos().x() &&
             new_pos.y() == y() + clientPos().y() )
            {
            new_pos.setX( x());
            new_pos.setY( y());
            }

        int nw = clientSize().width();
        int nh = clientSize().height();
        if ( value_mask & CWWidth )
            nw = rw;
        if ( value_mask & CWHeight )
            nh = rh;
        QSize ns = sizeForClientSize( QSize( nw, nh ) );

        // TODO what to do with maximized windows?
        if ( maximizeMode() != MaximizeFull
            || ns != size())
            {
            resetMaximize();
            ++block_geometry;
            move( new_pos );
            plainResize( ns ); // TODO must(?) resize before gravitating?
            --block_geometry;
            setGeometry( QRect( calculateGravitation( false, gravity ), size()), ForceGeometrySet );
            }
        }

    if ( value_mask & (CWWidth | CWHeight )
        && ! ( value_mask & ( CWX | CWY )) )  // pure resize
        {
        if ( isShade()) // SELI SHADE
            setShade( ShadeNone );

        int nw = clientSize().width();
        int nh = clientSize().height();
        if ( value_mask & CWWidth )
            nw = rw;
        if ( value_mask & CWHeight )
            nh = rh;
        QSize ns = sizeForClientSize( QSize( nw, nh ) );

        if( ns != size())  // don't restore if some app sets its own size again
            {
            resetMaximize();
            int save_gravity = xSizeHint.win_gravity;
            xSizeHint.win_gravity = gravity;
            resizeWithChecks( ns );
            xSizeHint.win_gravity = save_gravity;
            }
        }
    // No need to send synthetic configure notify event here, either it's sent together
    // with geometry change, or there's no need to send it.
    // Handling of the real ConfigureRequest event forces sending it, as there it's necessary.
    }

void Client::resizeWithChecks( int w, int h, ForceGeometry_t force )
    {
    int newx = x();
    int newy = y();
    QRect area = workspace()->clientArea( WorkArea, this );
    // don't allow growing larger than workarea
    if( w > area.width())
        w = area.width();
    if( h > area.height())
        h = area.height();
    QSize tmp = adjustedSize( QSize( w, h )); // checks size constraints, including min/max size
    w = tmp.width();
    h = tmp.height();
    switch( xSizeHint.win_gravity )
        {
        case NorthWestGravity: // top left corner doesn't move
        default:
            break;
        case NorthGravity: // middle of top border doesn't move
            newx = ( newx + width() / 2 ) - ( w / 2 );
            break;
        case NorthEastGravity: // top right corner doesn't move
            newx = newx + width() - w;
            break;
        case WestGravity: // middle of left border doesn't move
            newy = ( newy + height() / 2 ) - ( h / 2 );
            break;
        case CenterGravity: // middle point doesn't move
            newx = ( newx + width() / 2 ) - ( w / 2 );
            newy = ( newy + height() / 2 ) - ( h / 2 );
            break;
        case StaticGravity: // top left corner of _client_ window doesn't move
            // since decoration doesn't change, equal to NorthWestGravity
            break;
        case EastGravity: // // middle of right border doesn't move
            newx = newx + width() - w;
            newy = ( newy + height() / 2 ) - ( h / 2 );
            break;
        case SouthWestGravity: // bottom left corner doesn't move
            newy = newy + height() - h;
            break;
        case SouthGravity: // middle of bottom border doesn't move
            newx = ( newx + width() / 2 ) - ( w / 2 );
            newy = newy + height() - h;
            break;
        case SouthEastGravity: // bottom right corner doesn't move
            newx = newx + width() - w;
            newy = newy + height() - h;
            break;
        }
    // if it would be moved outside of workarea, keep it inside,
    // see also Client::computeWorkareaDiff()
    if( workarea_diff_x != INT_MIN && w <= area.width()) // was inside and can still fit
        {
        if( newx < area.left())
            newx = area.left();
        if( newx + w > area.right() + 1 )
            newx = area.right() + 1 - w;
        assert( newx >= area.left() && newx + w <= area.right() + 1 ); // width was checked above
        }
    if( workarea_diff_y != INT_MIN && h <= area.height()) // was inside and can still fit
        {
        if( newy < area.top())
            newy = area.top();
        if( newy + h > area.bottom() + 1 )
            newy = area.bottom() + 1 - h;
        assert( newy >= area.top() && newy + h <= area.bottom() + 1 ); // height was checked above
        }
    setGeometry( newx, newy, w, h, force );
    }

// _NET_MOVERESIZE_WINDOW
void Client::NETMoveResizeWindow( int flags, int x, int y, int width, int height )
    {
    int gravity = flags & 0xff;
    int value_mask = 0;
    if( flags & ( 1 << 8 ))
        value_mask |= CWX;
    if( flags & ( 1 << 9 ))
        value_mask |= CWY;
    if( flags & ( 1 << 10 ))
        value_mask |= CWWidth;
    if( flags & ( 1 << 11 ))
        value_mask |= CWHeight;
    configureRequest( value_mask, x, y, width, height, gravity );
    }

/*!
  Returns whether the window is resizable or has a fixed size.
 */
bool Client::isResizable() const
    {
    if ( !isMovable() || !motif_may_resize || isSplash())
        return FALSE;

    if ( ( xSizeHint.flags & PMaxSize) == 0 || (xSizeHint.flags & PMinSize ) == 0 )
        return TRUE;
    return ( xSizeHint.min_width < xSizeHint.max_width  ) ||
          ( xSizeHint.min_height < xSizeHint.max_height  );
    }

/*
  Returns whether the window is maximizable or not
 */
bool Client::isMaximizable() const
    {
    if ( maximizeMode() != MaximizeRestore )
        return TRUE;
    if( !isResizable() || isToolbar()) // SELI isToolbar() ?
        return false;
    if( xSizeHint.max_height < 32767 || xSizeHint.max_width < 32767 ) // sizes are 16bit with X
        return false;
    return true;
    }


/*!
  Reimplemented to inform the client about the new window position.
 */
void Client::setGeometry( int x, int y, int w, int h, ForceGeometry_t force )
    {
    if( force == NormalGeometrySet && frame_geometry == QRect( x, y, w, h ))
        return;
    frame_geometry = QRect( x, y, w, h );
    if( !isShade())
        client_size = QSize( w - border_left - border_right, h - border_top - border_bottom );
    else
        {
        // check that the frame is not resized to full size when it should be shaded
        if( !shade_geometry_change && h != border_top + border_bottom )
            {
            kdDebug() << "h:" << h << ":t:" << border_top << ":b:" << border_bottom << endl;
            assert( false );
            }
        client_size = QSize( w - border_left - border_right, client_size.height());
        }
    updateWorkareaDiffs();
    if( block_geometry == 0 )
        {
        XMoveResizeWindow( qt_xdisplay(), frameId(), x, y, w, h );
        resizeDecoration( QSize( w, h ));
        if( !isShade())
            {
            QSize cs = clientSize();
            XMoveResizeWindow( qt_xdisplay(), wrapperId(), clientPos().x(), clientPos().y(),
                cs.width(), cs.height());
    	    // FRAME tady poradi tak, at neni flicker
            XMoveResizeWindow( qt_xdisplay(), window(), 0, 0, cs.width(), cs.height());
            }
        if( shape())
            updateShape();
        // SELI TODO won't this be too expensive?
        updateWorkareaDiffs();
        sendSyntheticConfigureNotify(); // TODO optimize this?
        }
    }

void Client::plainResize( int w, int h, ForceGeometry_t force )
    { // TODO make this deffered with isResize() ? old kwin did
    if( force == NormalGeometrySet && frame_geometry.size() == QSize( w, h ))
        return;
    frame_geometry.setSize( QSize( w, h ));
    if( !isShade())
        client_size = QSize( w - border_left - border_right, h - border_top - border_bottom );
    else
        {
        // check that the frame is not resized to full size when it should be shaded
        if( !shade_geometry_change && h != border_top + border_bottom )
            {
            kdDebug() << "h:" << h << ":t:" << border_top << ":b:" << border_bottom << endl;
            assert( false );
            }
        client_size = QSize( w - border_left - border_right, client_size.height());
        }
    updateWorkareaDiffs();
    if( block_geometry == 0 )
        {
        // FRAME tady poradi tak, at neni flicker
        XResizeWindow( qt_xdisplay(), frameId(), w, h );
        resizeDecoration( QSize( w, h ));
        if( !isShade())
            {
            QSize cs = clientSize();
            XMoveResizeWindow( qt_xdisplay(), wrapperId(), clientPos().x(), clientPos().y(),
                cs.width(), cs.height());
            XMoveResizeWindow( qt_xdisplay(), window(), 0, 0, cs.width(), cs.height());
            }
        if( shape())
            updateShape();
        updateWorkareaDiffs();
        sendSyntheticConfigureNotify();
        }
    }

/*!
  Reimplemented to inform the client about the new window position.
 */
void Client::move( int x, int y, ForceGeometry_t force )
    {
    if( force == NormalGeometrySet && frame_geometry.topLeft() == QPoint( x, y ))
        return;
    frame_geometry.moveTopLeft( QPoint( x, y ));
    updateWorkareaDiffs();
    if( block_geometry == 0 )
        {
        XMoveWindow( qt_xdisplay(), frameId(), x, y );
        sendSyntheticConfigureNotify();
        }
    }


void Client::maximize( MaximizeMode m )
    {
    setMaximize( m & MaximizeVertical, m & MaximizeHorizontal );
    }

/*!
  Sets the maximization according to \a vertically and \a horizontally
 */
void Client::setMaximize( bool vertically, bool horizontally )
    {   // changeMaximize() flips the state, so change from set->flip
    changeMaximize(
        max_mode & MaximizeVertical ? !vertically : vertically,
        max_mode & MaximizeHorizontal ? !horizontally : horizontally,
        false );
    }

void Client::changeMaximize( bool vertical, bool horizontal, bool adjust )
    {
    if( !isMaximizable())
        return;

    ++block_geometry; // TODO GeometryBlocker class?

    if( isShade()) // SELI SHADE
        setShade( ShadeNone );

    MaximizeMode old_mode = max_mode;
    // 'adjust == true' means to update the size only, e.g. after changing workspace size
    if( !adjust )
        {
        if( vertical )
            max_mode = MaximizeMode( max_mode ^ MaximizeVertical );
        if( horizontal )
            max_mode = MaximizeMode( max_mode ^ MaximizeHorizontal );
        }

    // maximing one way and unmaximizing the other way shouldn't happen
    Q_ASSERT( !( vertical && horizontal )
        || (( max_mode & MaximizeVertical != 0 ) == ( max_mode & MaximizeHorizontal != 0 )));

    // save sizes for restoring, if maximalizing
    bool maximalizing = false;
    if( vertical && !(old_mode & MaximizeVertical ))
        {
        geom_restore.setTop( y());
        geom_restore.setHeight( height());
        maximalizing = true;
        }
    if( horizontal && !( old_mode & MaximizeHorizontal ))
        {
        geom_restore.setLeft( x());
        geom_restore.setWidth( width());
        maximalizing = true;
        }

    if( !adjust )
        {
        if( maximalizing )
                Notify::raise( Notify::Maximize );
        else
            Notify::raise( Notify::UnMaximize );
        }

    if( decoration != NULL ) // decorations may turn off some borders when maximized
        decoration->borders( border_left, border_right, border_top, border_bottom );

    QRect clientArea = workspace()->clientArea( MaximizeArea, this );

    switch (max_mode)
        {

        case MaximizeVertical:
            {
            if( old_mode & MaximizeHorizontal ) // actually restoring from MaximizeFull
                {
                if( geom_restore.width() == 0 )
                    { // needs placement
                    plainResize( adjustedSize(QSize(width(), clientArea.height()), SizemodeFixedH ));
                    workspace()->placeSmart( this );
                    }
                else
                    setGeometry( QRect(QPoint( geom_restore.x(), clientArea.top()),
                              adjustedSize(QSize( geom_restore.width(), clientArea.height()), SizemodeFixedH )));
                }
            else
                setGeometry( QRect(QPoint(x(), clientArea.top()),
                              adjustedSize(QSize(width(), clientArea.height()), SizemodeFixedH )));
            info->setState( NET::MaxVert, NET::Max );
            break;
            }

        case MaximizeHorizontal:
            {
            if( old_mode & MaximizeVertical ) // actually restoring from MaximizeFull
                {
                if( geom_restore.height() == 0 )
                    { // needs placement
                    plainResize( adjustedSize(QSize(clientArea.width(), height()), SizemodeFixedW ));
                    workspace()->placeSmart( this );
                    }
                else
                    setGeometry( QRect( QPoint(clientArea.left(), geom_restore.y()),
                              adjustedSize(QSize(clientArea.width(), geom_restore.height()), SizemodeFixedW )));
                }
            else
                setGeometry( QRect( QPoint(clientArea.left(), y()),
                              adjustedSize(QSize(clientArea.width(), height()), SizemodeFixedW )));
            info->setState( NET::MaxHoriz, NET::Max );
            break;
            }

        case MaximizeRestore:
            {
            QRect restore = geom_restore;
            if( !geom_restore.isValid())
                {
                QSize s = QSize( clientArea.width()*2/3, clientArea.height()*2/3 );
                if( geom_restore.width() > 0 )
                    s.setWidth( geom_restore.width());
                if( geom_restore.height() > 0 )
                    s.setHeight( geom_restore.height());
                plainResize( adjustedSize( s ));
                workspace()->placeSmart( this );
                restore = geometry();
                if( geom_restore.width() > 0 )
                    restore.moveLeft( geom_restore.x());
                if( geom_restore.height() > 0 )
                    restore.moveTop( geom_restore.y());
                }
	// when only partially maximized, geom_restore may not have the other dimension remembered
            if(( old_mode & MaximizeHorizontal ) == 0 && restore.width() <= 0 )
                {
                restore.setLeft( x());
                restore.setWidth( width());
                }
            if(( old_mode & MaximizeVertical ) == 0 && restore.height() <= 0 )
                {
                restore.setTop( y());
                restore.setHeight( height());
                }
            setGeometry( restore );
            info->setState( 0, NET::Max );
            break;
            }

        case MaximizeFull:
            {
            QSize adjSize = adjustedSize(clientArea.size(), SizemodeMax );
            QRect r = QRect(clientArea.topLeft(), adjSize);
            setGeometry( r );
            info->setState( NET::Max, NET::Max );
            break;
            }
        default:
            break;
        }

    --block_geometry;
    setGeometry( geometry(), ForceGeometrySet );

    updateAllowedActions();
    if( decoration != NULL )
        decoration->maximizeChange();
    }

void Client::resetMaximize()
    {
    if( max_mode == MaximizeRestore )
        return;
    max_mode = MaximizeRestore;
    Notify::raise( Notify::UnMaximize );
    info->setState( 0, NET::Max );
    updateAllowedActions();
    if( decoration != NULL )
        decoration->borders( border_left, border_right, border_top, border_bottom );
    setGeometry( geometry(), ForceGeometrySet );
    if( decoration != NULL )
        decoration->maximizeChange();
    }

bool Client::isFullScreenable() const
    {
    return isFullScreen() // necessary, because for fullscreen windows isMaximizable() returns false
        || (( isNormalWindow() || isOverride()) && isMaximizable());
    }

bool Client::userCanSetFullScreen() const
    {
    return isFullScreenable() && isNormalWindow() && fullscreen_mode != FullScreenHack;
    }

void Client::setFullScreen( bool set, bool user )
    {
    if( !isFullScreen() && !set )
        return;
    if( fullscreen_mode == FullScreenHack )
        return;
    if( user && !userCanSetFullScreen())
        return;
    setShade( ShadeNone );
    bool was_fs = isFullScreen();
    if( !was_fs )
        geom_fs_restore = geometry();
    fullscreen_mode = set ? FullScreenNormal : FullScreenNone;
    if( was_fs == isFullScreen())
        return;
    StackingUpdatesBlocker blocker( workspace());
    workspace()->updateClientLayer( this ); // active fullscreens get different layer
    info->setState( isFullScreen() ? NET::FullScreen : 0, NET::FullScreen );
    updateDecoration( false, false );
    if( isFullScreen())
        setGeometry( workspace()->clientArea( MaximizeFullArea, this ));
    else
        {
        if( maximizeMode() != MaximizeRestore )
            changeMaximize( false, false, true ); // adjust size
        else if( !geom_fs_restore.isNull())
            setGeometry( geom_fs_restore );
        // TODO isShaded() ?
        else
            { // does this ever happen?
            setGeometry( workspace()->clientArea( MaximizeArea, this ));
            }
        }
    }


static QRect*       visible_bound  = 0;
static GeometryTip* geometryTip    = 0;

void Client::drawbound( const QRect& geom )
    {
    assert( visible_bound == NULL );
    visible_bound = new QRect( geom );
    doDrawbound( *visible_bound, false );
    }

void Client::clearbound()
    {
    if( visible_bound == NULL )
        return;
    doDrawbound( *visible_bound, true );
    delete visible_bound;
    visible_bound = 0;
    }

void Client::doDrawbound( const QRect& geom, bool clear )
    {
    if( decoration != NULL && decoration->drawbound( geom, clear ))
        return; // done by decoration
    QPainter p ( workspace()->desktopWidget() );
    p.setPen( QPen( Qt::white, 5 ) );
    p.setRasterOp( Qt::XorROP );
    p.drawRect( geom );
    }

void Client::positionGeometryTip()
    {
    assert( isMove() || isResize());
    // Position and Size display
    if (options->showGeometryTip())
        {
        if( !geometryTip )
            { // save under is not necessary with opaque, and seem to make things slower
            bool save_under = ( isMove() && options->moveMode != Options::Opaque )
                        || ( isResize() && options->resizeMode != Options::Opaque );
            geometryTip = new GeometryTip( &xSizeHint, save_under );
            }
        QRect wgeom( moveResizeGeom ); // position of the frame, size of the window itself
        wgeom.setWidth( wgeom.width() - ( width() - clientSize().width()));
        wgeom.setHeight( wgeom.height() - ( height() - clientSize().height()));
        if( isShade())
            wgeom.setHeight( 0 );
        geometryTip->setGeometry( wgeom );
        if( !geometryTip->isVisible())
            {
            geometryTip->show();
            geometryTip->raise();
            }
        }
    }

class EatAllPaintEvents
    : public QObject
    {
    protected:
        virtual bool eventFilter( QObject* o, QEvent* e )
            { return e->type() == QEvent::Paint && o != geometryTip; }
    };

static EatAllPaintEvents* eater = 0;

bool Client::startMoveResize()
    {
    assert( !moveResizeMode );
    assert( QWidget::keyboardGrabber() == NULL );
    assert( QWidget::mouseGrabber() == NULL );
    if( QApplication::activePopupWidget() != NULL )
        return false; // popups have grab
    bool has_grab = false;
    if( mode == PositionCenter )
        setCursor( sizeAllCursor ); // change from arrow cursor if moving
    if( XGrabPointer( qt_xdisplay(), frameId(), False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, cursor.handle(), qt_x_time ) == Success )
        has_grab = true;
    if( XGrabKeyboard( qt_xdisplay(), frameId(), False, GrabModeAsync, GrabModeAsync, qt_x_time ) == Success )
        has_grab = true;
    if( !has_grab ) // at least one grab is necessary in order to be able to finish move/resize
        return false;
    if ( maximizeMode() != MaximizeRestore )
        resetMaximize();
    moveResizeMode = true;
    workspace()->setClientIsMoving(this);
    initialMoveResizeGeom = moveResizeGeom = geometry();
    if ( ( isMove() && options->moveMode != Options::Opaque )
      || ( isResize() && options->resizeMode != Options::Opaque ) )
        {
        grabXServer();
        kapp->sendPostedEvents();
        // we have server grab -> nothing should cause paint events
        // unfortunately, that's not completely true, Qt may generate
        // paint events on some widgets due to FocusIn(?)
        // eat them, otherwise XOR painting will be broken (#58054)
        // paint events for the geometrytip need to be allowed, though
        eater = new EatAllPaintEvents;
        kapp->installEventFilter( eater );
        }
    Notify::raise( isResize() ? Notify::ResizeStart : Notify::MoveStart );
    return true;
    }

void Client::finishMoveResize( bool cancel )
    {
    leaveMoveResize();
    if( cancel )
        setGeometry( initialMoveResizeGeom );
    else
        setGeometry( moveResizeGeom );
// FRAME    update();
    Notify::raise( isResize() ? Notify::ResizeEnd : Notify::MoveEnd );
    }

void Client::leaveMoveResize()
    {
    clearbound();
    if (geometryTip)
        {
        geometryTip->hide();
        delete geometryTip;
        geometryTip = NULL;
        }
    if ( ( isMove() && options->moveMode != Options::Opaque )
      || ( isResize() && options->resizeMode != Options::Opaque ) )
        ungrabXServer();
    XUngrabKeyboard( qt_xdisplay(), qt_x_time );
    XUngrabPointer( qt_xdisplay(), qt_x_time );
    workspace()->setClientIsMoving(0);
    if( move_faked_activity )
        workspace()->unfakeActivity( this );
    move_faked_activity = false;
    moveResizeMode = false;
    delete eater;
    eater = 0;
    }

void Client::handleMoveResize( int x, int y, int x_root, int y_root )
    {
    if(( mode == PositionCenter && !isMovable())
        || ( mode != PositionCenter && ( isShade() || !isResizable())))
        return;

    if ( !moveResizeMode )
        {
        QPoint p( QPoint( x, y ) - moveOffset );
        if (p.manhattanLength() >= 6)
            {
            if( !startMoveResize())
                {
                buttonDown = false;
                return;
                }
            }
        else
            return;
        }

    // ShadeHover or ShadeActive, ShadeNormal was already avoided above
    if ( mode != PositionCenter && shade_mode != ShadeNone ) // SHADE
        setShade( ShadeNone );

    QPoint globalPos( x_root, y_root );
    // these two points limit the geometry rectangle, i.e. if bottomleft resizing is done,
    // the bottomleft corner should be at is at (topleft.x(), bottomright().y())
    QPoint topleft = globalPos - moveOffset;
    QPoint bottomright = globalPos + invertedMoveOffset;
    QRect previousMoveResizeGeom = moveResizeGeom;

    // TODO move whole group when moving its leader or when the leader is not mapped?

    // compute bounds
    QRect desktopArea = workspace()->clientArea( WorkArea, globalPos, desktop());
    int left_marge, right_marge, top_marge, bottom_marge, titlebar_marge;
    if( unrestrictedMoveResize ) // unrestricted, just don't let it go out completely
        left_marge = right_marge = top_marge = bottom_marge = titlebar_marge = 5;
    else // restricted move/resize - keep at least part of the titlebar always visible 
        {        
        // how much must remain visible when moved away in that direction
        left_marge = 100 + border_right;
        right_marge = 100 + border_left;
        // width/height change with opaque resizing, use the initial ones
        titlebar_marge = initialMoveResizeGeom.height();
        top_marge = border_bottom;
        bottom_marge = border_top;
        }

    bool update = false;
    if( isResize())
        {
        // first resize (without checking constrains), then snap, then check bounds, then check constrains
        QRect orig = initialMoveResizeGeom;
        Sizemode sizemode = SizemodeAny;
        switch ( mode )
            {
            case PositionTopLeft:
                moveResizeGeom =  QRect( topleft, orig.bottomRight() ) ;
                break;
            case PositionBottomRight:
                moveResizeGeom =  QRect( orig.topLeft(), bottomright ) ;
                break;
            case PositionBottomLeft:
                moveResizeGeom =  QRect( QPoint( topleft.x(), orig.y() ), QPoint( orig.right(), bottomright.y()) ) ;
                break;
            case PositionTopRight:
                moveResizeGeom =  QRect( QPoint( orig.x(), topleft.y() ), QPoint( bottomright.x(), orig.bottom()) ) ;
                break;
            case PositionTop:
                moveResizeGeom =  QRect( QPoint( orig.left(), topleft.y() ), orig.bottomRight() ) ;
                sizemode = SizemodeFixedH; // try not to affect height
                break;
            case PositionBottom:
                moveResizeGeom =  QRect( orig.topLeft(), QPoint( orig.right(), bottomright.y() ) ) ;
                sizemode = SizemodeFixedH;
                break;
            case PositionLeft:
                moveResizeGeom =  QRect( QPoint( topleft.x(), orig.top() ), orig.bottomRight() ) ;
                sizemode = SizemodeFixedW;
                break;
            case PositionRight:
                moveResizeGeom =  QRect( orig.topLeft(), QPoint( bottomright.x(), orig.bottom() ) ) ;
                sizemode = SizemodeFixedW;
                break;
            case PositionCenter:
            default:
                assert( false );
                break;
            }
        // TODO snap
        if( moveResizeGeom.bottom() < desktopArea.top() + top_marge )
            moveResizeGeom.setBottom( desktopArea.top() + top_marge );
        if( moveResizeGeom.top() > desktopArea.bottom() - bottom_marge )
            moveResizeGeom.setTop( desktopArea.bottom() - bottom_marge );
        if( moveResizeGeom.right() < desktopArea.left() + left_marge )
            moveResizeGeom.setRight( desktopArea.left() + left_marge );
        if( moveResizeGeom.left() > desktopArea.right() - right_marge )
            moveResizeGeom.setLeft(desktopArea.right() - right_marge );
        if( !unrestrictedMoveResize && moveResizeGeom.top() < desktopArea.top() ) // titlebar mustn't go out
            moveResizeGeom.setTop( desktopArea.top());

        QSize size = adjustedSize( moveResizeGeom.size(), sizemode );
        // the new topleft and bottomright corners (after checking size constrains), if they'll be needed
        topleft = QPoint( moveResizeGeom.right() - size.width() + 1, moveResizeGeom.bottom() - size.height() + 1 );
        bottomright = QPoint( moveResizeGeom.left() + size.width() - 1, moveResizeGeom.top() + size.height() - 1 );
        orig = moveResizeGeom;
        switch ( mode )
            { // these 4 corners ones are copied from above
            case PositionTopLeft:
                moveResizeGeom =  QRect( topleft, orig.bottomRight() ) ;
                break;
            case PositionBottomRight:
                moveResizeGeom =  QRect( orig.topLeft(), bottomright ) ;
                break;
            case PositionBottomLeft:
                moveResizeGeom =  QRect( QPoint( topleft.x(), orig.y() ), QPoint( orig.right(), bottomright.y()) ) ;
                break;
            case PositionTopRight:
                moveResizeGeom =  QRect( QPoint( orig.x(), topleft.y() ), QPoint( bottomright.x(), orig.bottom()) ) ;
                break;
            // The side ones can't be copied exactly - if aspect ratios are specified, both dimensions may change.
            // Therefore grow to the right/bottom if needed.
            // TODO it should probably obey gravity rather than always using right/bottom ?
            case PositionTop:
                moveResizeGeom =  QRect( QPoint( orig.left(), topleft.y() ), QPoint( bottomright.x(), orig.bottom()) ) ;
                break;
            case PositionBottom:
                moveResizeGeom =  QRect( orig.topLeft(), QPoint( bottomright.x(), bottomright.y() ) ) ;
                break;
            case PositionLeft:
                moveResizeGeom =  QRect( QPoint( topleft.x(), orig.top() ), QPoint( orig.right(), bottomright.y()));
                break;
            case PositionRight:
                moveResizeGeom =  QRect( orig.topLeft(), QPoint( bottomright.x(), bottomright.y() ) ) ;
                break;
            case PositionCenter:
            default:
                assert( false );
                break;
            }
        if( moveResizeGeom.size() != previousMoveResizeGeom.size())
            update = true;
        }
    else if( isMove())
        {
        assert( mode == PositionCenter );
        // first move, then snap, then check bounds
        moveResizeGeom.moveTopLeft( topleft );
        moveResizeGeom.moveTopLeft( workspace()->adjustClientPosition( this, moveResizeGeom.topLeft() ) );
        if( moveResizeGeom.bottom() <= desktopArea.top() + titlebar_marge ) // titlebar mustn't go out
            moveResizeGeom.moveBottom( desktopArea.top() + titlebar_marge - 1 );
        // no need to check top_marge, titlebar_marge already handles it
        if( moveResizeGeom.top() > desktopArea.bottom() - bottom_marge )
            moveResizeGeom.moveTop( desktopArea.bottom() - bottom_marge );
        if( moveResizeGeom.right() < desktopArea.left() + left_marge )
            moveResizeGeom.moveRight( desktopArea.left() + left_marge );
        if( moveResizeGeom.left() > desktopArea.right() - right_marge )
            moveResizeGeom.moveLeft(desktopArea.right() - right_marge );
        if( moveResizeGeom.topLeft() != previousMoveResizeGeom.topLeft())
            update = true;
        }
    else
        assert( false );

    if( update )
        {
        if(( isResize() ? options->resizeMode : options->moveMode ) == Options::Opaque )
            {
            setGeometry( moveResizeGeom );
            positionGeometryTip();
            }
        else if(( isResize() ? options->resizeMode : options->moveMode ) == Options::Transparent )
            {
            clearbound();  // it's necessary to move the geometry tip when there's no outline
            positionGeometryTip(); // shown, otherwise it would cause repaint problems in case
            drawbound( moveResizeGeom ); // they overlap; the paint event will come after this,
            }                               // so the geometry tip will be painted above the outline
        }
    if ( isMove() )
      workspace()->clientMoved(globalPos, qt_x_time);
    }


} // namespace
