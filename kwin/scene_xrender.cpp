/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*
 This is the XRender-based compositing code. The primary compositing
 backend is the OpenGL-based one, which should be more powerful
 and also possibly better documented. This backend is mostly for cases
 when the OpenGL backend cannot be used for some reason (insufficient
 performance, no usable OpenGL support at all, etc.)
 The plan is to keep it around as long as needed/possible, but if it
 proves to be too much hassle it will be dropped in the future.
 
 Docs:
 
 XRender (the protocol, but the function calls map to it):
 http://gitweb.freedesktop.org/?p=xorg/proto/renderproto.git;a=blob_plain;hb=HEAD;f=renderproto.txt
 
 XFixes (again, the protocol):
 http://gitweb.freedesktop.org/?p=xorg/proto/fixesproto.git;a=blob_plain;hb=HEAD;f=fixesproto.txt

*/

#include "scene_xrender.h"

#if defined(HAVE_XRENDER) && defined(HAVE_XFIXES)

#include "toplevel.h"
#include "client.h"
#include "deleted.h"
#include "effects.h"

#include <kxerrorhandler.h>

namespace KWin
{

//****************************************
// SceneXrender
//****************************************

// kDebug() support for the XserverRegion type
struct RegionDebug
   {   
   RegionDebug( XserverRegion r ) : rr( r ) {}   
   XserverRegion rr;   
   };   

#ifdef NDEBUG
inline
kndbgstream& operator<<( kndbgstream& stream, RegionDebug ) { return stream; }
#else
kdbgstream& operator<<( kdbgstream& stream, RegionDebug r )
    {       
    if( r.rr == None )
        return stream << "EMPTY";
    int num;
    XRectangle* rects = XFixesFetchRegion( display(), r.rr, &num );
    if( rects == NULL || num == 0 )
        return stream << "EMPTY";
    for( int i = 0;
         i < num;
         ++i )
       stream << "[" << rects[ i ].x << "+" << rects[ i ].y << " " << rects[ i ].width << "x" << rects[ i ].height << "]";
    return stream;
    }
#endif

Picture SceneXrender::buffer = None;
ScreenPaintData SceneXrender::screen_paint;

SceneXrender::SceneXrender( Workspace* ws )
    : Scene( ws )
    , front( None )
    , init_ok( false )
    {
    if( !Extensions::renderAvailable())
        {
        kDebug( 1212 ) << "No xrender extension available" << endl;
        return;
        }
    if( !Extensions::fixesRegionAvailable())
        {
        kDebug( 1212 ) << "No xfixes v3+ extension available" << endl;
        return;
        }
    KXErrorHandler xerr;
    // create XRender picture for the root window
    format = XRenderFindVisualFormat( display(), DefaultVisual( display(), DefaultScreen( display())));
    if( format == NULL )
        return; // error
    if( wspace->createOverlay())
        {
        wspace->setupOverlay( None );
        front = XRenderCreatePicture( display(), wspace->overlayWindow(), format, 0, NULL );
        }
    else
        {
        XRenderPictureAttributes pa;
        pa.subwindow_mode = IncludeInferiors;
        front = XRenderCreatePicture( display(), rootWindow(), format, CPSubwindowMode, &pa );
        }
    createBuffer();
    init_ok = !xerr.error( true );
    }

SceneXrender::~SceneXrender()
    {
    if( !init_ok )
        {
        // TODO this probably needs to clean up whatever has been created until the failure
        wspace->destroyOverlay();
        return;
        }
    XRenderFreePicture( display(), front );
    XRenderFreePicture( display(), buffer );
    wspace->destroyOverlay();
    foreach( Window* w, windows )
        delete w;
    }

bool SceneXrender::initFailed() const
    {
    return !init_ok;
    }

// Create the compositing buffer. The root window is not double-buffered,
// so it is done manually using this buffer,
void SceneXrender::createBuffer()
    {
    if( buffer != None )
        XRenderFreePicture( display(), buffer );
    Pixmap pixmap = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(), DefaultDepth( display(), DefaultScreen( display())));
    buffer = XRenderCreatePicture( display(), pixmap, format, 0, 0 );
    XFreePixmap( display(), pixmap ); // The picture owns the pixmap now
    }

// the entry point for painting
void SceneXrender::paint( QRegion damage, ToplevelList toplevels )
    {
    foreach( Toplevel* c, toplevels )
        {
        assert( windows.contains( c ));
        stacking_order.append( windows[ c ] );
        }
    int mask = 0;
    paintScreen( &mask, &damage );
    if( mask & PAINT_SCREEN_REGION )
        {
        // Use the damage region as the clip region for the root window
        XserverRegion front_region = toXserverRegion( damage );
        XFixesSetPictureClipRegion( display(), front, 0, 0, front_region );
        XFixesDestroyRegion( display(), front_region );
        // copy composed buffer to the root window
        XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
        XRenderComposite( display(), PictOpSrc, buffer, None, front, 0, 0, 0, 0, 0, 0, displayWidth(), displayHeight());
        XFixesSetPictureClipRegion( display(), front, 0, 0, None );
        XFlush( display());
        }
    else
        {
        // copy composed buffer to the root window
        XRenderComposite( display(), PictOpSrc, buffer, None, front, 0, 0, 0, 0, 0, 0, displayWidth(), displayHeight());
        XFlush( display());
        }
    // do cleanup
    stacking_order.clear();
    }

void SceneXrender::paintGenericScreen( int mask, ScreenPaintData data )
    {
    screen_paint = data; // save, transformations will be done when painting windows
    if( false ) // TODO never needed?
        Scene::paintGenericScreen( mask, data );
    else
        paintTransformedScreen( mask );
    }

/*
 Try to do optimized painting even with transformations. Since only scaling
 and translation are supported by the painting code, clipping can be done
 manually to avoid having to paint everything in every pass. Whole screen
 still need to be painted but e.g. obscured windows don't. So this below
 is basically paintSimpleScreen() with extra code to compute clipping correctly.
 
 This code assumes that the only transformations possible with XRender are those
 provided by Window/ScreenPaintData, In the (very unlikely?) case more is needed
 then paintGenericScreen() needs to be used.
*/
void SceneXrender::paintTransformedScreen( int orig_mask )
    {
    QRegion region( 0, 0, displayWidth(), displayHeight());
    QList< Phase2Data > phase2;
    QRegion allclips;
    // Draw each opaque window top to bottom, subtracting the bounding rect of
    // each window from the clip region after it's been drawn.
    for( int i = stacking_order.count() - 1; // top to bottom
         i >= 0;
         --i )
        {
        Window* w = static_cast< Window* >( stacking_order[ i ] );
        WindowPrePaintData data;
        data.mask = orig_mask | ( w->isOpaque() ? PAINT_WINDOW_OPAQUE : PAINT_WINDOW_TRANSLUCENT );
        w->resetPaintingEnabled();
        data.paint = region;
        // TODO this is wrong, transformedShape() should be used here, but is not known yet
        data.clip = w->isOpaque() ? region : QRegion();
        data.quads = w->buildQuads();
        // preparation step
        effects->prePaintWindow( effectWindow( w ), data, time_diff );
#ifndef NDEBUG
        foreach( WindowQuad q, data.quads )
            if( q.isTransformed())
                kFatal( 1212 ) << "Pre-paint calls are not allowed to transform quads!" << endl;
#endif
        if( !w->isPaintingEnabled())
            continue;
        data.paint -= allclips; // make sure to avoid already clipped areas
        if( data.paint.isEmpty()) // completely clipped
            continue;
        if( data.paint != region ) // prepaint added area to draw
            {
            region |= data.paint; // make sure other windows in that area get painted too
            painted_region |= data.paint; // make sure it makes it to the screen
            }
        // If the window is transparent, the transparent part will be done
        // in the 2nd pass.
        if( data.mask & PAINT_WINDOW_TRANSLUCENT )
            phase2.prepend( Phase2Data( w, data.paint, data.mask, data.quads ));
        if( data.mask & PAINT_WINDOW_OPAQUE )
            {
            w->setTransformedShape( QRegion());
            paintWindow( w, data.mask, data.paint, data.quads );
            // The window can clip by its opaque parts the windows below.
            region -= w->transformedShape();
            }
        }
    if( !( orig_mask & PAINT_SCREEN_BACKGROUND_FIRST ))
        paintBackground( region ); // Fill any areas of the root window not covered by windows
    // Now walk the list bottom to top, drawing translucent windows.
    // That we draw bottom to top is important now since we're drawing translucent objects
    // and also are clipping only by opaque windows.
    QRegion add_paint;
    foreach( Phase2Data d, phase2 )
        {
        Scene::Window* w = d.window;
        paintWindow( w, d.mask, d.region | add_paint, d.quads );
        // It is necessary to also add paint regions of windows below, because their
        // pre-paint's might have extended the paint area, so those areas need to be painted too.
        add_paint |= d.region;
        }
    }

// fill the screen background
void SceneXrender::paintBackground( QRegion region )
    {
    if( region != infiniteRegion())
        {
        XserverRegion background_region = toXserverRegion( region );
        XFixesSetPictureClipRegion( display(), buffer, 0, 0, background_region );
        XFixesDestroyRegion( display(), background_region );
        }
    XRenderColor col = { 0xffff, 0xffff, 0xffff, 0xffff };
    XRenderFillRectangle( display(), PictOpSrc, buffer, &col, 0, 0, displayWidth(), displayHeight());
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
    }

void SceneXrender::windowGeometryShapeChanged( Toplevel* c )
    {
    if( !windows.contains( c )) // this is ok, shape is not valid by default
        return;
    Window* w = windows[ c ];
    w->discardPicture();
    w->discardShape();
    w->discardAlpha();
    }
    
void SceneXrender::windowOpacityChanged( Toplevel* c )
    {
    if( !windows.contains( c )) // this is ok, alpha is created on demand
        return;
    Window* w = windows[ c ];
    w->discardAlpha();
    }

void SceneXrender::windowClosed( Toplevel* c, Deleted* deleted )
    {
    assert( windows.contains( c ));
    if( deleted != NULL )
        { // replace c with deleted
        Window* w = windows.take( c );
        w->updateToplevel( deleted );
        windows[ deleted ] = w;
        }
    else
        {
        delete windows.take( c );
        c->effectWindow()->setSceneWindow( NULL );
        }
    }

void SceneXrender::windowDeleted( Deleted* c )
    {
    assert( windows.contains( c ));
    delete windows.take( c );
    c->effectWindow()->setSceneWindow( NULL );
    }

void SceneXrender::windowAdded( Toplevel* c )
    {
    assert( !windows.contains( c ));
    windows[ c ] = new Window( c );
    }

// Convert QRegion to XserverRegion. This code uses XserverRegion
// only when really necessary as the shared implementation uses
// QRegion.
XserverRegion SceneXrender::toXserverRegion( QRegion region )
    {
    QVector< QRect > rects = region.rects();
    XRectangle* xr = new XRectangle[ rects.count() ];
    for( int i = 0;
         i < rects.count();
         ++i )
        {
        xr[ i ].x = rects[ i ].x();
        xr[ i ].y = rects[ i ].y();
        xr[ i ].width = rects[ i ].width();
        xr[ i ].height = rects[ i ].height();
        }
    XserverRegion ret = XFixesCreateRegion( display(), xr, rects.count());
    delete[] xr;
    return ret;
    }

//****************************************
// SceneXrender::Window
//****************************************

SceneXrender::Window::Window( Toplevel* c )
    : Scene::Window( c )
    , _picture( None )
    , format( XRenderFindVisualFormat( display(), c->visual()))
    , alpha( None )
    {
    }

SceneXrender::Window::~Window()
    {
    discardPicture();
    discardAlpha();
    discardShape();
    }

// Create XRender picture for the pixmap with the window contents.
Picture SceneXrender::Window::picture()
    {
    if( !toplevel->damage().isEmpty() && _picture != None )
        {
        XRenderFreePicture( display(), _picture );
        _picture = None;
        }
    if( _picture == None && format != NULL )
        {
        // Get the pixmap with the window contents.
        Pixmap pix = toplevel->windowPixmap();
        if( pix == None )
            return None;
        _picture = XRenderCreatePicture( display(), pix, format, 0, 0 );
        toplevel->resetDamage( toplevel->rect());
        }
    return _picture;
    }


void SceneXrender::Window::discardPicture()
    {
    if( _picture != None )
        XRenderFreePicture( display(), _picture );
    _picture = None;
    }

void SceneXrender::Window::discardAlpha()
    {
    if( alpha != None )
        XRenderFreePicture( display(), alpha );
    alpha = None;
    }

// Create XRender picture for the alpha mask.
Picture SceneXrender::Window::alphaMask( double opacity )
    {
    if( isOpaque() && opacity == 1.0 )
        return None;
    if( alpha != None && alpha_cached_opacity != opacity )
        {
        if( alpha != None )
            XRenderFreePicture( display(), alpha );
        alpha = None;
        }
    if( alpha != None )
        return alpha;
    if( opacity == 1.0 )
        { // no need to create alpha mask
        alpha_cached_opacity = 1.0;
        return None;
        }
    // Create a 1x1 8bpp pixmap containing the given opacity in the alpha channel.
    Pixmap pixmap = XCreatePixmap( display(), rootWindow(), 1, 1, 8 );
    XRenderPictFormat* format = XRenderFindStandardFormat( display(), PictStandardA8 );
    XRenderPictureAttributes pa;
    pa.repeat = True;
    alpha = XRenderCreatePicture( display(), pixmap, format, CPRepeat, &pa );
    XFreePixmap( display(), pixmap );
    XRenderColor col;
    col.alpha = int( opacity * 0xffff );
    alpha_cached_opacity = opacity;
    XRenderFillRectangle( display(), PictOpSrc, alpha, &col, 0, 0, 1, 1 );
    return alpha;
    }

// paint the window
void SceneXrender::Window::performPaint( int mask, QRegion region, WindowPaintData data )
    {
    setTransformedShape( QRegion()); // maybe nothing will be painted
    // check if there is something to paint
    bool opaque = isOpaque() && data.opacity == 1.0;
    if( mask & ( PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSLUCENT ))
        {}
    else if( mask & PAINT_WINDOW_OPAQUE )
        {
        if( !opaque )
            return;
        }
    else if( mask & PAINT_WINDOW_TRANSLUCENT )
        {
        if( opaque )
            return;
        }
    if( region != infiniteRegion())
        {
        XserverRegion clip_region = toXserverRegion( region );
        XFixesSetPictureClipRegion( display(), buffer, 0, 0, clip_region );
        XFixesDestroyRegion( display(), clip_region );
        }
    Picture pic = picture(); // get XRender picture
    if( pic == None ) // The render format can be null for GL and/or Xv visuals
        return;
    // set picture filter
    if( options->smoothScale > 0 ) // only when forced, it's slow
        {
        if( mask & PAINT_WINDOW_TRANSFORMED )
            filter = ImageFilterGood;
        else if( mask & PAINT_SCREEN_TRANSFORMED )
            filter = ImageFilterGood;
        else
            filter = ImageFilterFast;
        }
    else
        filter = ImageFilterFast;
    // do required transformations
    int x = toplevel->x();
    int y = toplevel->y();
    int width = toplevel->width();
    int height = toplevel->height();
    double xscale = 1;
    double yscale = 1;
    transformed_shape = shape();
    if( mask & PAINT_WINDOW_TRANSFORMED )
        {
        xscale *= data.xScale;
        yscale *= data.yScale;
        x += data.xTranslate;
        y += data.yTranslate;
        }
    if( mask & PAINT_SCREEN_TRANSFORMED )
        {
        xscale *= screen_paint.xScale;
        yscale *= screen_paint.yScale;
        x = int( x * screen_paint.xScale );
        y = int( y * screen_paint.yScale );
        x += screen_paint.xTranslate;
        y += screen_paint.yTranslate;
        }
    if( yscale != 1 || xscale != 1 )
        {
        XTransform xform = {{
            { XDoubleToFixed( 1 / xscale ), XDoubleToFixed( 0 ), XDoubleToFixed( 0 ) },
            { XDoubleToFixed( 0 ), XDoubleToFixed( 1 / yscale ), XDoubleToFixed( 0 ) },
            { XDoubleToFixed( 0 ), XDoubleToFixed( 0 ), XDoubleToFixed( 1 ) }
        }};
        XRenderSetPictureTransform( display(), pic, &xform );
        width = (int)(width * xscale);
        height = (int)(height * yscale);
        if( filter == ImageFilterGood )
            XRenderSetPictureFilter( display(), pic, const_cast< char* >( "good" ), NULL, 0 );
        // transform the shape for clipping in paintTransformedScreen()
        QVector< QRect > rects = transformed_shape.rects();
        for( int i = 0;
             i < rects.count();
             ++i )
            {
            QRect& r = rects[ i ];
            r = QRect( int( r.x() * xscale ), int( r.y() * yscale ),
                int( r.width() * xscale ), int( r.height() * xscale ));
            }
        transformed_shape.setRects( rects.constData(), rects.count());
        }
    if( x != toplevel->x() || y != toplevel->y())
        transformed_shape.translate( x, y );
    if( opaque )
        {
        XRenderComposite( display(), PictOpSrc, pic, None, buffer, 0, 0, 0, 0,
            x, y, width, height);
        }
    else
        {
        Picture alpha = alphaMask( data.opacity );
        XRenderComposite( display(), PictOpOver, pic, alpha, buffer, 0, 0, 0, 0,
            x, y, width, height);
        transformed_shape = QRegion();
        }
    if( xscale != 1 || yscale != 1 )
        {
        XTransform xform = {{
            { XDoubleToFixed( 1 ), XDoubleToFixed( 0 ), XDoubleToFixed( 0 ) },
            { XDoubleToFixed( 0 ), XDoubleToFixed( 1 ), XDoubleToFixed( 0 ) },
            { XDoubleToFixed( 0 ), XDoubleToFixed( 0 ), XDoubleToFixed( 1 ) }
        }};
        XRenderSetPictureTransform( display(), pic, &xform );
        if( filter == ImageFilterGood )
            XRenderSetPictureFilter( display(), pic, const_cast< char* >( "fast" ), NULL, 0 );
        }
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
    }

} // namespace
#endif
