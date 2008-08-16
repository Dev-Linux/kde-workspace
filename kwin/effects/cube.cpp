/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "cube.h"

#include <kaction.h>
#include <kactioncollection.h>
#include <klocale.h>
#include <kwinconfig.h>
#include <kconfiggroup.h>
#include <kcolorscheme.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdebug.h>

#include <QColor>
#include <QRect>
#include <QEvent>
#include <QKeyEvent>

#include <math.h>

#include <GL/gl.h>

namespace KWin
{

KWIN_EFFECT( cube, CubeEffect )

CubeEffect::CubeEffect()
    : activated( false )
    , cube_painting( false )
    , keyboard_grab( false )
    , schedule_close( false )
    , frontDesktop( 0 )
    , cubeOpacity( 1.0 )
    , displayDesktopName( true )
    , reflection( true )
    , rotating( false )
    , desktopChangedWhileRotating( false )
    , paintCaps( true )
    , rotationDirection( Left )
    , verticalRotationDirection( Upwards )
    , verticalPosition( Normal )
    , wallpaper( NULL )
    , texturedCaps( true )
    , capTexture( NULL )
    , manualAngle( 0.0 )
    , manualVerticalAngle( 0.0 )
    , currentShape( TimeLine::EaseInOutCurve )
    , start( false )
    , stop( false )
    , reflectionPainting( false )
    , slide( false )
    , oldDesktop( 0 )
    , activeScreen( 0 )
    , animateDesktopChange( false )
    , bigCube( false )
    , bottomCap( false )
    , capListCreated( false )
    , capList( 0 )
    {
    KConfigGroup conf = effects->effectConfig( "Cube" );
    borderActivate = (ElectricBorder)conf.readEntry( "BorderActivate", (int)ElectricNone );
    effects->reserveElectricBorder( borderActivate );

    cubeOpacity = (float)conf.readEntry( "Opacity", 80 )/100.0f;
    displayDesktopName = conf.readEntry( "DisplayDesktopName", true );
    reflection = conf.readEntry( "Reflection", true );
    rotationDuration = conf.readEntry( "RotationDuration", 500 );
    backgroundColor = conf.readEntry( "BackgroundColor", QColor( Qt::black ) );
    animateDesktopChange = conf.readEntry( "AnimateDesktopChange", false );
    bigCube = conf.readEntry( "BigCube", false );
    capColor = conf.readEntry( "CapColor", KColorScheme( QPalette::Active, KColorScheme::Window ).background().color() );
    paintCaps = conf.readEntry( "Caps", true );
    QString file = conf.readEntry( "Wallpaper", QString("") );
    if( !file.isEmpty() )
        {
        QImage img = QImage( file );
        if( !img.isNull() )
            {
            wallpaper = new GLTexture( img );
            }
        }
    texturedCaps = conf.readEntry( "TexturedCaps", true );
    if( texturedCaps )
        {
        QImage img = QImage( KGlobal::dirs()->findResource( "appdata", "cubecap.png" ) );
        if( !img.isNull() )
            {
            // change the alpha value of each pixel
            for( int x=0; x<img.width(); x++ )
                {
                for( int y=0; y<img.height(); y++ )
                    {
                    QRgb pixel = img.pixel( x, y );
                    if( x == 0 || x == img.width()-1 || y == 0 || y == img.height()-1 )
                        img.setPixel( x, y, qRgba( capColor.red(), capColor.green(), capColor.blue(), 255*cubeOpacity ) );
                    else
                        {
                        if( qAlpha(pixel) < 255 )
                            {
                            // Pixel is transparent - has to be blended with cap color
                            int red   = (qAlpha(pixel)/255)*(qRed(pixel)/255) + (1 - qAlpha(pixel)/255 )*capColor.red();
                            int green = (qAlpha(pixel)/255)*(qGreen(pixel)/255) + (1 - qAlpha(pixel)/255 )*capColor.green();
                            int blue  = (qAlpha(pixel)/255)*(qBlue(pixel)/255) + (1 - qAlpha(pixel)/255 )*capColor.blue();
                            int alpha = qAlpha(pixel) + (255 - qAlpha(pixel))*cubeOpacity;
                            img.setPixel( x, y, qRgba( red, green, blue, alpha ) );
                            }
                        else
                            {
                            img.setPixel( x, y, qRgba( qRed(pixel), qGreen(pixel), qBlue(pixel), ((float)qAlpha(pixel))*cubeOpacity ) );
                            }
                        }
                    }
                }
            capTexture = new GLTexture( img );
            capTexture->setFilter( GL_LINEAR );
            capTexture->setWrapMode( GL_CLAMP_TO_EDGE );
            }
        }

    timeLine.setCurveShape( TimeLine::EaseInOutCurve );
    timeLine.setDuration( rotationDuration );

    verticalTimeLine.setCurveShape( TimeLine::EaseInOutCurve );
    verticalTimeLine.setDuration( rotationDuration );

    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = static_cast< KAction* >( actionCollection->addAction( "Cube" ));
    a->setText( i18n("Desktop Cube" ));
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F11 ));
    connect( a, SIGNAL( triggered( bool )), this, SLOT( toggle()));
    }

CubeEffect::~CubeEffect()
    {
    effects->unreserveElectricBorder( borderActivate );
    delete wallpaper;
    delete capTexture;
    }

void CubeEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( activated )
        {
        //kDebug();
        data.mask |= PAINT_SCREEN_TRANSFORMED | Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS | PAINT_SCREEN_BACKGROUND_FIRST;

        if( rotating || start || stop )
            {
            timeLine.addTime( time );
            }
        if( verticalRotating )
            {
            verticalTimeLine.addTime( time );
            }
        }
    effects->prePaintScreen( data, time );
    }

void CubeEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( activated )
        {
        //kDebug();
        QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
        if( effects->numScreens() > 1 && (slide || bigCube ) )
            rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
        QRect fullRect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );

        // background
        float clearColor[4];
        glGetFloatv( GL_COLOR_CLEAR_VALUE, clearColor );
        glClearColor( backgroundColor.redF(), backgroundColor.greenF(), backgroundColor.blueF(), 1.0 );
        glClear( GL_COLOR_BUFFER_BIT );
        glClearColor( clearColor[0], clearColor[1], clearColor[2], clearColor[3] );

        // wallpaper
        if( wallpaper && !slide )
            {            
            wallpaper->bind();
            wallpaper->render( region, rect );
            wallpaper->unbind();
            }

        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        if( effects->numScreens() > 1 && !slide && !bigCube )
            {
            windowsOnOtherScreens.clear();
            // unfortunatelly we have to change the projection matrix in dual screen mode
            glMatrixMode( GL_PROJECTION );
            glPushMatrix();
            glLoadIdentity();
            float fovy = 60.0f;
            float aspect = 1.0f;
            float zNear = 0.1f;
            float zFar = 100.0f;
            float ymax = zNear * tan( fovy  * M_PI / 360.0f );
            float ymin = -ymax;
            float xmin =  ymin * aspect;
            float xmax = ymax * aspect;
            float xTranslate = 0.0;
            float yTranslate = 0.0;
            bool projectionSet = false;
            if( rect.x() == 0 && rect.width() != fullRect.width() )
                {
                // horizontal layout: left screen
                glFrustum( xmin*0.5, xmax*1.5, ymin, ymax, zNear, zFar );
                xTranslate = rect.width()*0.5;
                projectionSet = true;
                }
            if( rect.x() != 0 && rect.width() != fullRect.width() )
                {
                // horizontal layout: right screen
                glFrustum( xmin*1.5, xmax*0.5, ymin, ymax, zNear, zFar );
                xTranslate = rect.width()*0.5;
                projectionSet = true;
                }
            if( rect.y() == 0 && rect.height() != fullRect.height() )
                {
                glFrustum( xmin, xmax, ymin*1.5, ymax*0.5, zNear, zFar );
                yTranslate = rect.height()*0.5;
                projectionSet = true;
                }
            if( rect.y() != 0 && rect.height() != fullRect.height() )
                {
                glFrustum( xmin, xmax, ymin*0.5, ymax*1.5, zNear, zFar );
                yTranslate = rect.height()*0.5;
                projectionSet = true;
                }
            if( !projectionSet )
                {
                // this should never happen; nevertheless reset the projection to the default
                glPopMatrix();
                glPushMatrix();
                }
            glMatrixMode( GL_MODELVIEW );
            glPushMatrix();
            glTranslatef( xTranslate, yTranslate, 0.0 );
            }

        // reflection
        if( reflection && (!slide) )
            {
            glPushMatrix();
            float scaleFactor = 10000 * tan( 60.0 * M_PI / 360.0f )/rect.height();
            glScalef( 1.0, -1.0, 1.0 );
            glTranslatef( 0.0, -rect.height()*2, 0.0 );

            glEnable( GL_CLIP_PLANE0 );
            reflectionPainting = true;
            paintScene( mask, region, data );
            reflectionPainting = false;
            glDisable( GL_CLIP_PLANE0 );

            glPopMatrix();
            glPushMatrix();
            glTranslatef( 0.0, rect.height(), 0.0 );
            if( effects->numScreens() > 1 && rect.width() != fullRect.width() && !slide && !bigCube )
                {
                // have to change the reflection area in horizontal layout and right screen
                glTranslatef( -rect.width(), 0.0, 0.0 );
                }
            float vertices[] = {
                rect.x(), 0.0, 0.0,
                rect.x()+rect.width(), 0.0, 0.0,
                (rect.x()+rect.width())*scaleFactor, 0.0, -5000,
                (-rect.x()-rect.width())*scaleFactor, 0.0, -5000 };
            // foreground
            float alpha = 0.7;
            if( start )
                alpha = 0.3 + 0.4 * timeLine.value();
            if( stop )
                alpha = 0.3 + 0.4 * ( 1.0 - timeLine.value() );
            glColor4f( 0.0, 0.0, 0.0, alpha );
            glBegin( GL_POLYGON );
            glVertex3f( vertices[0], vertices[1], vertices[2] );
            glVertex3f( vertices[3], vertices[4], vertices[5] );
            // rearground
            alpha = -1.0;
            glColor4f( 0.0, 0.0, 0.0, alpha );
            glVertex3f( vertices[6], vertices[7], vertices[8] );
            glVertex3f( vertices[9], vertices[10], vertices[11] );
            glEnd();
            glPopMatrix();
            }
        glPushMatrix();
        paintScene( mask, region, data );
        glPopMatrix();

        if( effects->numScreens() > 1 && !slide && !bigCube  )
            {
            glPopMatrix();
            // revert change of projection matrix
            glMatrixMode( GL_PROJECTION );
            glPopMatrix();
            glMatrixMode( GL_MODELVIEW );
            }

        glDisable( GL_BLEND );
        glPopAttrib();

        // desktop name box - inspired from coverswitch
        if( displayDesktopName && (!slide) )
            {
            QColor color_frame;
            QColor color_text;
            color_frame = KColorScheme( QPalette::Active, KColorScheme::Window ).background().color();
            color_frame.setAlphaF( 0.5 );
            color_text = KColorScheme( QPalette::Active, KColorScheme::Window ).foreground().color();
            if( start )
                {
                color_frame.setAlphaF( 0.5 * timeLine.value() );
                color_text.setAlphaF( timeLine.value() );
                }
            if( stop )
                {
                color_frame.setAlphaF( 0.5 - 0.5 * timeLine.value() );
                color_text.setAlphaF( 1.0 - timeLine.value() );
                }
            QFont text_font;
            text_font.setBold( true );
            text_font.setPointSize( 14 );
            glPushAttrib( GL_CURRENT_BIT );
            glColor4f( color_frame.redF(), color_frame.greenF(), color_frame.blueF(), color_frame.alphaF());
            QRect frameRect = QRect( rect.width()*0.33f + rect.x(),
                rect.height() + rect.y() - QFontMetrics( text_font ).height() * 1.2f,
                rect.width()*0.33f,
                QFontMetrics( text_font ).height() * 1.2f );
            renderRoundBoxWithEdge( frameRect );
            effects->paintText( effects->desktopName( frontDesktop ),
                frameRect.center(),
                frameRect.width(),
                color_text,
                text_font );
            glPopAttrib();
            }
        if( stop && timeLine.value() == 1.0 )
            {
            effects->paintScreen( mask, region, data );
            }
        if( effects->numScreens() > 1 && !slide && !bigCube  )
            {
            foreach( EffectWindow* w, windowsOnOtherScreens )
                {
                WindowPaintData wData( w );
                if( start && !w->isDesktop() && !w->isDock() )
                    wData.opacity *= (1.0 - timeLine.value());
                if( stop && !w->isDesktop() && !w->isDock() )
                    wData.opacity *= timeLine.value();
                effects->paintWindow( w, 0, QRegion( w->x(), w->y(), w->width(), w->height() ), wData );
                }
            }
        }
    else
        {
        effects->paintScreen( mask, region, data );
        }
    }

void CubeEffect::paintScene( int mask, QRegion region, ScreenPaintData& data )
    {
    QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
    if( effects->numScreens() > 1 && (slide || bigCube ) )
        rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
    float xScale = 1.0;
    float yScale = 1.0;
    if( effects->numScreens() > 1 && !slide && !bigCube  )
        {
        QRect fullRect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
        xScale = (float)rect.width()/(float)fullRect.width();
        yScale = (float)rect.height()/(float)fullRect.height();
        if( start )
            {
            xScale = xScale + (1.0 - xScale) * (1.0 - timeLine.value());
            yScale = yScale + (1.0 - yScale) * (1.0 - timeLine.value());
            if( rect.x() > 0 )
                glTranslatef( -rect.x()*(1.0 - timeLine.value()), 0.0, 0.0 );
            if( rect.y() > 0 )
                glTranslatef( 0.0, -rect.y()*(1.0 - timeLine.value()), 0.0 );
            }
        if( stop )
            {
            xScale = xScale + (1.0 - xScale) * timeLine.value();
            yScale = yScale + (1.0 - yScale) * timeLine.value();
            if( rect.x() > 0 )
                glTranslatef( -rect.x()*timeLine.value(), 0.0, 0.0 );
            if( rect.y() > 0 )
                glTranslatef( 0.0, -rect.y()*timeLine.value(), 0.0 );
            }
        glScalef( xScale, yScale, 1.0 );
        rect = fullRect;
        }
    int rightSteps = effects->numberOfDesktops()/2;
    int leftSteps = rightSteps+1;
    int rightSideCounter = 0;
    int leftSideCounter = 0;
    float internalCubeAngle = 360.0f / effects->numberOfDesktops();
    cube_painting = true;
    int desktopIndex = 0;
    float zTranslate = 100.0;
    if( start )
        zTranslate *= timeLine.value();
    if( stop )
        zTranslate *= ( 1.0 - timeLine.value() );
    if( slide )
        zTranslate = 0.0;

    bool topCapAfter = false;
    bool topCapBefore = false;

    // Rotation of the cube
    float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2 )/(float)effects->numberOfDesktops() * 180.0f);
    float point = rect.width()/2*tan(cubeAngle*0.5f*M_PI/180.0f);
    float zTexture = rect.width()/2*tan(45.0f*M_PI/180.0f);
    if( verticalRotating || verticalPosition != Normal || manualVerticalAngle != 0.0 )
        {
        // change the verticalPosition if manualVerticalAngle > 90 or < -90 degrees
        if( manualVerticalAngle <= -90.0 )
            {
            manualVerticalAngle += 90.0;
            if( verticalPosition == Normal )
                verticalPosition = Down;
            if( verticalPosition == Up )
                verticalPosition = Normal;
            }
        if( manualVerticalAngle >= 90.0 )
            {
            manualVerticalAngle -= 90.0;
            if( verticalPosition == Normal )
                verticalPosition = Up;
            if( verticalPosition == Down )
                verticalPosition = Normal;
            }
        float angle = 0.0;
        if( verticalPosition == Up )
            {
            angle = 90.0;
            if( !verticalRotating)
                {
                if( manualVerticalAngle < 0.0 )
                    angle += manualVerticalAngle;
                else
                    manualVerticalAngle = 0.0;
                }
            }
        else if( verticalPosition == Down )
            {
            angle = -90.0;
            if( !verticalRotating)
                {
                if( manualVerticalAngle > 0.0 )
                    angle += manualVerticalAngle;
                else
                    manualVerticalAngle = 0.0;
                }
            }
        else
            {
            angle = manualVerticalAngle;
            }
        if( verticalRotating )
            {
            angle *= verticalTimeLine.value();
            if( verticalPosition == Normal && verticalRotationDirection == Upwards )
                angle = -90.0 + 90*verticalTimeLine.value();
            if( verticalPosition == Normal && verticalRotationDirection == Downwards )
                angle = 90.0 - 90*verticalTimeLine.value();
            angle += manualVerticalAngle * (1.0-verticalTimeLine.value());
            }
        if( stop )
            angle *= (1.0 - timeLine.value());
        glTranslatef( rect.width()/2, rect.height()/2, -point-zTranslate );
        glRotatef( angle, 1.0, 0.0, 0.0 );
        glTranslatef( -rect.width()/2, -rect.height()/2, point+zTranslate );

        // calculate if the caps have to be painted before/after or during desktop painting
        if( paintCaps )
            {
            float M[16];
            float P[16];
            float V[4];
            glGetFloatv( GL_PROJECTION_MATRIX, P );
            glGetFloatv( GL_MODELVIEW_MATRIX, M );
            glGetFloatv( GL_VIEWPORT, V );

            // calculate y coordinate of the top of front desktop
            float X = M[0]*0.0 + M[4]*0.0 + M[8]*(-zTranslate) + M[12]*1;
            float Y = M[1]*0.0 + M[5]*0.0 + M[9]*(-zTranslate) + M[13]*1;
            float Z = M[2]*0.0 + M[6]*0.0 + M[10]*(-zTranslate) + M[14]*1;
            float W = M[3]*0.0 + M[7]*0.0 + M[11]*(-zTranslate) + M[15]*1;
            float clipY = P[1]*X + P[5]*Y + P[9]*Z + P[13]*W;
            float clipW = P[3]*X + P[7]*Y + P[11]*Z + P[15]*W;
            float normY = clipY/clipW;
            float yFront = (V[3]/2)*normY+(V[3]-V[1])/2;

            // calculate y coordinate of the bottom of front desktop
            X = M[0]*0.0 + M[4]*rect.height() + M[8]*(-zTranslate) + M[12]*1;
            Y = M[1]*0.0 + M[5]*rect.height() + M[9]*(-zTranslate) + M[13]*1;
            Z = M[2]*0.0 + M[6]*rect.height() + M[10]*(-zTranslate) + M[14]*1;
            W = M[3]*0.0 + M[7]*rect.height() + M[11]*(-zTranslate) + M[15]*1;
            clipY = P[1]*X + P[5]*Y + P[9]*Z + P[13]*W;
            clipW = P[3]*X + P[7]*Y + P[11]*Z + P[15]*W;
            normY = clipY/clipW;
            float yFrontBottom = (V[3]/2)*normY+(V[3]-V[1])/2;

            // change matrix to a rear position
            glPushMatrix();
            glTranslatef( 0.0, 0.0, -point-zTranslate );
            float desktops = (effects->numberOfDesktops()/2.0);
            glRotatef( desktops*internalCubeAngle, 1.0, 0.0, 0.0 );
            glTranslatef( 0.0, 0.0, point );
            glGetFloatv(GL_MODELVIEW_MATRIX, M);
            // calculate y coordinate of the top of rear desktop
            X = M[0]*0.0 + M[4]*0.0 + M[8]*0.0 + M[12]*1;
            Y = M[1]*0.0 + M[5]*0.0 + M[9]*0.0 + M[13]*1;
            Z = M[2]*0.0 + M[6]*0.0 + M[10]*0.0 + M[14]*1;
            W = M[3]*0.0 + M[7]*0.0 + M[11]*0.0 + M[15]*1;
            clipY = P[1]*X + P[5]*Y + P[9]*Z + P[13]*W;
            clipW = P[3]*X + P[7]*Y + P[11]*Z + P[15]*W;
            normY = clipY/clipW;
            float yBack = (V[3]/2)*normY+(V[3]-V[1])/2;

            // calculate y coordniate of the bottom of rear desktop
            glTranslatef( 0.0, -rect.height(), 0.0 );
            glGetFloatv(GL_MODELVIEW_MATRIX, M);
            X = M[0]*0.0 + M[4]*0.0 + M[8]*0.0 + M[12]*1;
            Y = M[1]*0.0 + M[5]*0.0 + M[9]*0.0 + M[13]*1;
            Z = M[2]*0.0 + M[6]*0.0 + M[10]*0.0 + M[14]*1;
            W = M[3]*0.0 + M[7]*0.0 + M[11]*0.0 + M[15]*1;
            clipY = P[1]*X + P[5]*Y + P[9]*Z + P[13]*W;
            clipW = P[3]*X + P[7]*Y + P[11]*Z + P[15]*W;
            normY = clipY/clipW;
            float yBackBottom = (V[3]/2)*normY+(V[3]-V[1])/2;
            glPopMatrix();
            if( yBack >= yFront )
                topCapAfter = true;
            if( yBackBottom <= yFrontBottom )
                topCapBefore = true;
            }
        }
    if( rotating || (manualAngle != 0.0) )
        {
        if( manualAngle > internalCubeAngle * 0.5f )
            {
            manualAngle -= internalCubeAngle;
            frontDesktop--;
            if( frontDesktop == 0 )
                frontDesktop = effects->numberOfDesktops();
            }
        if( manualAngle < -internalCubeAngle * 0.5f )
            {
            manualAngle += internalCubeAngle;
            frontDesktop++;
            if( frontDesktop > effects->numberOfDesktops() )
                frontDesktop = 1;
            }
        float rotationAngle = internalCubeAngle * timeLine.value();
        if( rotationAngle > internalCubeAngle * 0.5f )
            {
            rotationAngle -= internalCubeAngle;
            if( !desktopChangedWhileRotating )
                {
                desktopChangedWhileRotating = true;
                if( rotationDirection == Left )
                    {
                    frontDesktop++;
                    }
                else if( rotationDirection == Right )
                    {
                    frontDesktop--;
                    }
                if( frontDesktop > effects->numberOfDesktops() )
                    frontDesktop = 1;
                else if( frontDesktop == 0 )
                    frontDesktop = effects->numberOfDesktops();
                }
            }
        if( rotationDirection == Left )
            {
            rotationAngle *= -1;
            }
        if( stop )
            rotationAngle = manualAngle * (1.0 - timeLine.value());
        else
            rotationAngle += manualAngle * (1.0 - timeLine.value());
        glTranslatef( rect.width()/2, rect.height()/2, -point-zTranslate );
        glRotatef( rotationAngle, 0.0, 1.0, 0.0 );
        glTranslatef( -rect.width()/2, -rect.height()/2, point+zTranslate );
        }

    if( topCapBefore || topCapAfter )
        {
        if( (topCapAfter && !reflectionPainting) || (topCapBefore && reflectionPainting) )
            {
            // paint the bottom cap
            bottomCap = true;
            glTranslatef( 0.0, rect.height(), 0.0 );
            paintCap( point, zTexture );
            glTranslatef( 0.0, -rect.height(), 0.0 );
            bottomCap = false;
            }
        if( (topCapBefore && !reflectionPainting) || (topCapAfter && reflectionPainting) )
            {
            // paint the top cap
            paintCap( point, zTexture );
            }
        }

    for( int i=0; i<effects->numberOfDesktops(); i++ )
        {
        if( !topCapAfter && !topCapBefore && i == effects->numberOfDesktops()/2 -1 && !slide )
            {
            // paint the bottom cap
            bottomCap = true;
            glTranslatef( 0.0, rect.height(), 0.0 );
            paintCap( point, zTexture );
            glTranslatef( 0.0, -rect.height(), 0.0 );
            bottomCap = false;
            // paint the top cap
            paintCap( point, zTexture );
            }
        if( i%2 == 0 &&  i != effects->numberOfDesktops() -1)
            {
            // desktops on the right (including back)
            desktopIndex = rightSteps - rightSideCounter;
            rightSideCounter++;
            }
        else
            {
            // desktops on the left (including front)
            desktopIndex = leftSteps + leftSideCounter;
            leftSideCounter++;
            }

        // start painting the cube
        painting_desktop = (desktopIndex + frontDesktop )%effects->numberOfDesktops();
        if( painting_desktop == 0 )
            {
            painting_desktop = effects->numberOfDesktops();
            }
        if( slide )
            {
            // only paint required desktops during slide phase
            if( painting_desktop != frontDesktop )
                {
                int leftDesktop = frontDesktop - 1;
                int rightDesktop = frontDesktop + 1;
                if( leftDesktop == 0 )
                    leftDesktop = effects->numberOfDesktops();
                if( rightDesktop > effects->numberOfDesktops() )
                    rightDesktop = 1;
                if( !desktopChangedWhileRotating && rotationDirection == Left && painting_desktop != rightDesktop )
                    {
                    continue;
                    }
                if( desktopChangedWhileRotating && rotationDirection == Left && painting_desktop != leftDesktop )
                    {
                    continue;
                    }
                if( !desktopChangedWhileRotating && rotationDirection == Right && painting_desktop != leftDesktop )
                    {
                    continue;
                    }
                if( desktopChangedWhileRotating && rotationDirection == Right && painting_desktop != rightDesktop )
                    {
                    continue;
                    }
                }
            }
        ScreenPaintData newData = data;
        RotationData rot = RotationData();
        rot.axis = RotationData::YAxis;
        rot.angle = internalCubeAngle * desktopIndex;
        rot.xRotationPoint = rect.width()/2;
        rot.zRotationPoint = -point;
        newData.rotation = &rot;
        newData.zTranslate = -zTranslate;
        effects->paintScreen( mask, region, newData );
        }
    if( topCapBefore || topCapAfter )
        {
        if( (topCapAfter && !reflectionPainting) || (topCapBefore && reflectionPainting) )
            {
            // paint the top cap
            paintCap( point, zTexture );
            }
        if( (topCapBefore && !reflectionPainting) || (topCapAfter && reflectionPainting) )
            {
            // paint the bottom cap
            bottomCap = true;
            glTranslatef( 0.0, rect.height(), 0.0 );
            paintCap( point, zTexture );
            glTranslatef( 0.0, -rect.height(), 0.0 );
            bottomCap = false;
            }
        }
    cube_painting = false;
    painting_desktop = effects->currentDesktop();
    }

void CubeEffect::paintCap( float z, float zTexture )
    {
    QRect rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop());
    if( ( !paintCaps ) || effects->numberOfDesktops() <= 2 )
        return;
    float opacity = cubeOpacity;
    if( start )
        opacity = 1.0 - (1.0 - opacity)*timeLine.value();
    if( stop )
        opacity = 1.0 - (1.0 - opacity)*( 1.0 - timeLine.value() );
    glColor4f( capColor.redF(), capColor.greenF(), capColor.blueF(), opacity );
    float angle = 360.0f/effects->numberOfDesktops();
    glPushMatrix();
    float zTranslate = 100.0;
    if( start )
        zTranslate *= timeLine.value();
    if( stop )
        zTranslate *= ( 1.0 - timeLine.value() );
    glTranslatef( rect.width()/2, 0.0, -z-zTranslate );
    glRotatef( (1-frontDesktop)*angle, 0.0, 1.0, 0.0 );

    if( !capListCreated )
        {
        capListCreated = true;
        glNewList( capList, GL_COMPILE_AND_EXECUTE );
        bool texture = false;
        if( texturedCaps && effects->numberOfDesktops() > 3 && capTexture )
            {
            texture = true;
            paintCapStep( z, zTexture, true );
            }
        else
            paintCapStep( z, zTexture, false );
        glEndList();
        }
    else
        glCallList( capList );
    glPopMatrix();
    }

void CubeEffect::paintCapStep( float z, float zTexture, bool texture )
    {
    QRect rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop());
    float angle = 360.0f/effects->numberOfDesktops();
    if( texture )
        capTexture->bind();
    for( int i=0; i<effects->numberOfDesktops(); i++ )
        {
        int triangleRows = effects->numberOfDesktops()*5;
        float zTriangleDistance = z/(float)triangleRows;
        float widthTriangle = tan( angle*0.5 * M_PI/180.0 ) * zTriangleDistance;
        float currentWidth = 0.0;
        glBegin( GL_TRIANGLES );
        float cosValue = cos( i*angle  * M_PI/180.0 );
        float sinValue = sin( i*angle  * M_PI/180.0 );
        for( int j=0; j<triangleRows; j++ )
            {
            float previousWidth = currentWidth;
            currentWidth = tan( angle*0.5 * M_PI/180.0 ) * zTriangleDistance * (j+1);
            int evenTriangles = 0;
            int oddTriangles = 0;
            for( int k=0; k<floor(currentWidth/widthTriangle*2-1+0.5f); k++ )
                {
                float x1 = -previousWidth;
                float x2 = -currentWidth;
                float x3 = 0.0;
                float z1 = 0.0;
                float z2 = 0.0;
                float z3 = 0.0;
                if( k%2 == 0 )
                    {
                    x1 += evenTriangles*widthTriangle*2;
                    x2 += evenTriangles*widthTriangle*2;
                    x3 = x2+widthTriangle*2;
                    z1 = j*zTriangleDistance;
                    z2 = (j+1)*zTriangleDistance;
                    z3 = (j+1)*zTriangleDistance;
                    float xRot = cosValue * x1 - sinValue * z1;
                    float zRot = sinValue * x1 + cosValue * z1;
                    x1 = xRot;
                    z1 = zRot;
                    xRot = cosValue * x2 - sinValue * z2;
                    zRot = sinValue * x2 + cosValue * z2;
                    x2 = xRot;
                    z2 = zRot;
                    xRot = cosValue * x3 - sinValue * z3;
                    zRot = sinValue * x3 + cosValue * z3;
                    x3 = xRot;
                    z3 = zRot;
                    evenTriangles++;
                    }
                else
                    {
                    x1 += oddTriangles*widthTriangle*2;
                    x2 += (oddTriangles+1)*widthTriangle*2;
                    x3 = x1+widthTriangle*2;
                    z1 = j*zTriangleDistance;
                    z2 = (j+1)*zTriangleDistance;
                    z3 = j*zTriangleDistance;
                    float xRot = cosValue * x1 - sinValue * z1;
                    float zRot = sinValue * x1 + cosValue * z1;
                    x1 = xRot;
                    z1 = zRot;
                    xRot = cosValue * x2 - sinValue * z2;
                    zRot = sinValue * x2 + cosValue * z2;
                    x2 = xRot;
                    z2 = zRot;
                    xRot = cosValue * x3 - sinValue * z3;
                    zRot = sinValue * x3 + cosValue * z3;
                    x3 = xRot;
                    z3 = zRot;
                    oddTriangles++;
                    }
                float texX1 = 0.0;
                float texX2 = 0.0;
                float texX3 = 0.0;
                float texY1 = 0.0;
                float texY2 = 0.0;
                float texY3 = 0.0;
                if( texture )
                    {
                    texX1 = x1/(rect.width())+0.5;
                    texY1 = 0.5 - z1/zTexture * 0.5;
                    texX2 = x2/(rect.width())+0.5;
                    texY2 = 0.5 - z2/zTexture * 0.5;
                    texX3 = x3/(rect.width())+0.5;
                    texY3 = 0.5 - z3/zTexture * 0.5;
                    glTexCoord2f( texX1, texY1 );
                    }
                glVertex3f( x1, 0.0, z1 );
                if( texture )
                    {
                    glTexCoord2f( texX2, texY2 );
                    }
                glVertex3f( x2, 0.0, z2 );
                if( texture )
                    {
                    glTexCoord2f( texX3, texY3 );
                    }
                glVertex3f( x3, 0.0, z3 );
                }
            }
        glEnd();
        }
    if( texture )
        {
        capTexture->unbind();
        }
    }

void CubeEffect::postPaintScreen()
    {
    effects->postPaintScreen();
    if( activated )
        {
        //kDebug();
        if( start )
            {
            if( timeLine.value() == 1.0 )
                {
                start = false;
                timeLine.setProgress(0.0);
                // more rotations?
                if( !rotations.empty() )
                    {
                    rotationDirection = rotations.dequeue();
                    rotating = true;
                    // change the curve shape if current shape is not easeInOut
                    if( currentShape != TimeLine::EaseInOutCurve )
                        {
                        // more rotations follow -> linear curve
                        if( !rotations.empty() )
                            {
                            currentShape = TimeLine::LinearCurve;
                            }
                        // last rotation step -> easeOut curve
                        else
                            {
                            currentShape = TimeLine::EaseOutCurve;
                            }
                        timeLine.setCurveShape( currentShape );
                        }
                    else
                        {
                        // if there is at least one more rotation, we can change to easeIn
                        if( !rotations.empty() )
                            {
                            currentShape = TimeLine::EaseInCurve;
                            timeLine.setCurveShape( currentShape );
                            }
                        }
                    }
                }
            effects->addRepaintFull();
            return; // schedule_close could have been called, start has to finish first
            }
        if( stop )
            {
            if( timeLine.value() == 1.0 )
                {
                stop = false;
                timeLine.setProgress(0.0);
                activated = false;
                // set the new desktop
                if( keyboard_grab )
                    effects->ungrabKeyboard();
                keyboard_grab = false;
                effects->destroyInputWindow( input );

                effects->setActiveFullScreenEffect( 0 );

                // delete the GL lists
                glDeleteLists( capList, 1 );
                }
            effects->addRepaintFull();
            }
        if( rotating || verticalRotating )
            {
            if( rotating && timeLine.value() == 1.0 )
                {
                timeLine.setProgress(0.0);
                rotating = false;
                desktopChangedWhileRotating = false;
                manualAngle = 0.0;
                // more rotations?
                if( !rotations.empty() )
                    {
                    rotationDirection = rotations.dequeue();
                    rotating = true;
                    // change the curve shape if current shape is not easeInOut
                    if( currentShape != TimeLine::EaseInOutCurve )
                        {
                        // more rotations follow -> linear curve
                        if( !rotations.empty() )
                            {
                            currentShape = TimeLine::LinearCurve;
                            }
                        // last rotation step -> easeOut curve
                        else
                            {
                            currentShape = TimeLine::EaseOutCurve;
                            }
                        timeLine.setCurveShape( currentShape );
                        }
                    else
                        {
                        // if there is at least one more rotation, we can change to easeIn
                        if( !rotations.empty() )
                            {
                            currentShape = TimeLine::EaseInCurve;
                            timeLine.setCurveShape( currentShape );
                            }
                        }
                    }
                else
                    {
                    // reset curve shape if there are no more rotations
                    if( currentShape != TimeLine::EaseInOutCurve )
                        {
                        currentShape = TimeLine::EaseInOutCurve;
                        timeLine.setCurveShape( currentShape );
                        }
                    }
                }
            if( verticalRotating && verticalTimeLine.value() == 1.0 )
                {
                verticalTimeLine.setProgress(0.0);
                verticalRotating = false;
                manualVerticalAngle = 0.0;
                // more rotations?
                if( !verticalRotations.empty() )
                    {
                    verticalRotationDirection = verticalRotations.dequeue();
                    verticalRotating = true;
                    if( verticalRotationDirection == Upwards )
                        {
                        if( verticalPosition == Normal )
                            verticalPosition = Up;
                        if( verticalPosition == Down )
                            verticalPosition = Normal;
                        }
                    if( verticalRotationDirection == Downwards )
                        {
                        if( verticalPosition == Normal )
                            verticalPosition = Down;
                        if( verticalPosition == Up )
                            verticalPosition = Normal;
                        }
                    }
                }
            effects->addRepaintFull();
            return; // rotation has to end before cube is closed
            }
        if( schedule_close )
            {
            schedule_close = false;
            if( !slide )
                {
                effects->setCurrentDesktop( frontDesktop );
                stop = true;
                }
            else
                {
                activated = false;
                slide = false;
                timeLine.setDuration( rotationDuration );
                effects->setActiveFullScreenEffect( 0 );
                }
            effects->addRepaintFull();
            }
        }
    }

void CubeEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( activated )
        {
        //kDebug();
        if( cube_painting )
            {
            if( ( w->isDesktop() || w->isDock() ) && w->screen() != activeScreen && w->isOnDesktop( effects->currentDesktop() ) )
                {
                windowsOnOtherScreens.append( w );
                }
            if( (start || stop) && w->screen() != activeScreen && w->isOnDesktop( effects->currentDesktop() )
                && !w->isDesktop() && !w->isDock() )
                {
                windowsOnOtherScreens.append( w );
                }
            if( w->isOnDesktop( painting_desktop ))
                {
                QRect rect = effects->clientArea( FullArea, activeScreen, painting_desktop );
                if( w->x() < rect.x() )
                    {
                    data.quads = data.quads.splitAtX( -w->x() );
                    }
                if( w->x() + w->width() > rect.x() + rect.width() )
                    {
                    data.quads = data.quads.splitAtX( rect.width() - w->x() );
                    }
                if( w->y() < rect.y() )
                    {
                    data.quads = data.quads.splitAtY( -w->y() );
                    }
                if( w->y() + w->height() > rect.y() + rect.height() )
                    {
                    data.quads = data.quads.splitAtY( rect.height() - w->y() );
                    }
                w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                }
            else
                {
                // check for windows belonging to the previous desktop
                int prev_desktop = painting_desktop -1;
                if( prev_desktop == 0 )
                    prev_desktop = effects->numberOfDesktops();
                if( w->isOnDesktop( prev_desktop ) )
                    {
                    QRect rect = effects->clientArea( FullArea, activeScreen, prev_desktop);
                    if( w->x()+w->width() > rect.x() + rect.width() )
                        {
                        w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                        data.quads = data.quads.splitAtX( rect.width() - w->x() );
                        if( w->y() < rect.y() )
                            {
                            data.quads = data.quads.splitAtY( -w->y() );
                            }
                        if( w->y() + w->height() > rect.y() + rect.height() )
                            {
                            data.quads = data.quads.splitAtY( rect.height() - w->y() );
                            }
                        data.setTransformed();
                        effects->prePaintWindow( w, data, time );
                        return;
                        }
                    }
                // check for windows belonging to the next desktop
                int next_desktop = painting_desktop +1;
                if( next_desktop > effects->numberOfDesktops() )
                    next_desktop = 1;
                if( w->isOnDesktop( next_desktop ) )
                    {
                    QRect rect = effects->clientArea( FullArea, activeScreen, next_desktop);
                    if( w->x() < rect.x() )
                        {
                        w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                        data.quads = data.quads.splitAtX( -w->x() );
                        if( w->y() < rect.y() )
                            {
                            data.quads = data.quads.splitAtY( -w->y() );
                            }
                        if( w->y() + w->height() > rect.y() + rect.height() )
                            {
                            data.quads = data.quads.splitAtY( rect.height() - w->y() );
                            }
                        data.setTransformed();
                        effects->prePaintWindow( w, data, time );
                        return;
                        }
                    }
                w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                }
            }
        }
    effects->prePaintWindow( w, data, time );
    }

void CubeEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( activated && cube_painting )
        {
        //kDebug() << w->caption();
        float opacity = cubeOpacity;
        if( start )
            {
            opacity = 1.0 - (1.0 - opacity)*timeLine.value();
            if( reflectionPainting )
                opacity = 0.5 + ( cubeOpacity - 0.5 )*timeLine.value();
            // fade in windows belonging to different desktops
            if( painting_desktop == effects->currentDesktop() && (!w->isOnDesktop( painting_desktop )) )
                opacity = timeLine.value() * cubeOpacity;
            }
        if( stop )
            {
            opacity = 1.0 - (1.0 - opacity)*( 1.0 - timeLine.value() );
            if( reflectionPainting )
                opacity = 0.5 + ( cubeOpacity - 0.5 )*( 1.0 - timeLine.value() );
            // fade out windows belonging to different desktops
            if( painting_desktop == effects->currentDesktop() && (!w->isOnDesktop( painting_desktop )) )
                opacity = cubeOpacity * (1.0 - timeLine.value());
            }
        bool slideOpacity = false;
        // fade in windows from different desktops in first slide step
        if( slide && painting_desktop == oldDesktop && (!w->isOnDesktop( painting_desktop )) )
            {
            opacity = timeLine.value();
            slideOpacity = true;
            }
        // fade out windows from different desktops in last slide step
        if( slide && rotations.empty() && painting_desktop == effects->currentDesktop() && (!w->isOnDesktop( painting_desktop )) )
            {
            opacity = 1.0 - timeLine.value();
            slideOpacity = true;
            }
        // check for windows belonging to the previous desktop
        int prev_desktop = painting_desktop -1;
        if( prev_desktop == 0 )
            prev_desktop = effects->numberOfDesktops();
        int next_desktop = painting_desktop +1;
        if( next_desktop > effects->numberOfDesktops() )
            next_desktop = 1;
        if( w->isOnDesktop( prev_desktop ) && ( mask & PAINT_WINDOW_TRANSFORMED ) )
            {
            QRect rect = effects->clientArea( FullArea, activeScreen, prev_desktop);
            data.xTranslate = -rect.width();
            WindowQuadList new_quads;
            foreach( WindowQuad quad, data.quads )
                {
                if( quad.right() > rect.width() - w->x() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        if( w->isOnDesktop( next_desktop ) && ( mask & PAINT_WINDOW_TRANSFORMED ) )
            {
            QRect rect = effects->clientArea( FullArea, activeScreen, next_desktop);
            data.xTranslate = rect.width();
            WindowQuadList new_quads;
            foreach( WindowQuad quad, data.quads )
                {
                if( w->x() + quad.right() <= rect.x() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        QRect rect = effects->clientArea( FullArea, activeScreen, painting_desktop );

        if( start || stop )
            {
            // we have to change opacity values for fade in/out of windows which are shown on front-desktop
            if( prev_desktop == effects->currentDesktop() && w->x() < rect.x() )
                {
                if( start )
                    opacity = timeLine.value() * cubeOpacity;
                if( stop )
                    opacity = cubeOpacity * (1.0 - timeLine.value());
                }
            if( next_desktop == effects->currentDesktop() && w->x() + w->width() > rect.x() + rect.width() )
                {
                if( start )
                    opacity = timeLine.value() * cubeOpacity;
                if( stop )
                    opacity = cubeOpacity * (1.0 - timeLine.value());
                }
            }
        if( !slide || (slide && !w->isDesktop()) || slideOpacity )
            data.opacity *= opacity;

        if( w->isOnDesktop(painting_desktop) && w->x() < rect.x() )
            {
            WindowQuadList new_quads;
            foreach( WindowQuad quad, data.quads )
                {
                if( quad.right() > -w->x() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        if( w->isOnDesktop(painting_desktop) && w->x() + w->width() > rect.x() + rect.width() )
            {
            WindowQuadList new_quads;
            foreach( WindowQuad quad, data.quads )
                {
                if( quad.right() <= rect.width() - w->x() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        if( w->y() < rect.y() )
            {
            WindowQuadList new_quads;
            foreach( WindowQuad quad, data.quads )
                {
                if( quad.bottom() > -w->y() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        if( w->y() + w->height() > rect.y() + rect.height() )
            {
            WindowQuadList new_quads;
            foreach( WindowQuad quad, data.quads )
                {
                if( quad.bottom() <= rect.height() - w->y() )
                    {
                    new_quads.append( quad );
                    }
                }
            data.quads = new_quads;
            }
        }
    effects->paintWindow( w, mask, region, data );
    }

bool CubeEffect::borderActivated( ElectricBorder border )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return false;
    if( border == borderActivate && !activated )
        {
        kDebug() << "border activated";
        toggle();
        return true;
        }
    return false;
    }

void CubeEffect::toggle()
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    if( !activated )
        {
        setActive( true );
        }
    else
        {
        setActive( false );
        }
    }

void CubeEffect::grabbedKeyboardEvent( QKeyEvent* e )
    {
    if( stop )
        return;
    // taken from desktopgrid.cpp
    if( e->type() == QEvent::KeyPress )
        {
        int desktop = -1;
        // switch by F<number> or just <number>
        if( e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F35 )
            desktop = e->key() - Qt::Key_F1 + 1;
        else if( e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9 )
            desktop = e->key() == Qt::Key_0 ? 10 : e->key() - Qt::Key_0;
        if( desktop != -1 )
            {
            if( desktop <= effects->numberOfDesktops())
                {
                // we have to rotate to chosen desktop
                // and end effect when rotation finished
                rotateToDesktop( desktop );
                setActive( false );
                }
            return;
            }
        switch( e->key())
            { // wrap only on autorepeat
            case Qt::Key_Left:
                // rotate to previous desktop
                kDebug() << "left";
                if( !rotating && !start )
                    {
                    rotating = true;
                    rotationDirection = Left;
                    }
                else
                    {
                    rotations.enqueue( Left );
                    }
                break;
            case Qt::Key_Right:
                // rotate to next desktop
                kDebug() << "right";
                if( !rotating && !start )
                    {
                    rotating = true;
                    rotationDirection = Right;
                    }
                else
                    {
                    rotations.enqueue( Right );
                    }
                break;
            case Qt::Key_Up:
                kDebug() << "up";
                if( verticalPosition != Up )
                    {
                    if( !verticalRotating )
                        {
                        verticalRotating = true;
                        verticalRotationDirection = Upwards;
                        if( verticalPosition == Normal )
                            verticalPosition = Up;
                        if( verticalPosition == Down )
                            verticalPosition = Normal;
                        }
                    else
                        {
                        verticalRotations.enqueue( Upwards );
                        }
                    }
                else if( manualVerticalAngle < 0.0 && !verticalRotating )
                    {
                    // rotate to up position from the manual position
                    verticalRotating = true;
                    verticalRotationDirection = Upwards;
                    verticalPosition = Up;
                    manualVerticalAngle += 90.0;
                    }
                break;
            case Qt::Key_Down:
                kDebug() << "down";
                if( verticalPosition != Down )
                    {
                    if( !verticalRotating )
                        {
                        verticalRotating = true;
                        verticalRotationDirection = Downwards;
                        if( verticalPosition == Normal )
                            verticalPosition = Down;
                        if( verticalPosition == Up )
                            verticalPosition = Normal;
                        }
                    else
                        {
                        verticalRotations.enqueue( Downwards );
                        }
                    }
                else if( manualVerticalAngle > 0.0 && !verticalRotating )
                    {
                    // rotate to down position from the manual position
                    verticalRotating = true;
                    verticalRotationDirection = Downwards;
                    verticalPosition = Down;
                    manualVerticalAngle -= 90.0;
                    }
                break;
            case Qt::Key_Escape:
                rotateToDesktop( effects->currentDesktop() );
                setActive( false );
                return;
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Space:
                setActive( false );
                return;
            default:
                break;
            }
        effects->addRepaintFull();
        }
    }

void CubeEffect::rotateToDesktop( int desktop )
    {
    int tempFrontDesktop = frontDesktop;
    if( !rotations.empty() )
        {
        // all scheduled rotations will be removed as a speed up
        rotations.clear();
        }
    if( rotating && !desktopChangedWhileRotating )
        {
        // front desktop will change during the actual rotation - this has to be considered
        if( rotationDirection == Left )
            {
            tempFrontDesktop++;
            }
        else if( rotationDirection == Right )
            {
            tempFrontDesktop--;
            }
        if( tempFrontDesktop > effects->numberOfDesktops() )
            tempFrontDesktop = 1;
        else if( tempFrontDesktop == 0 )
            tempFrontDesktop = effects->numberOfDesktops();
        }
    // find the fastest rotation path from tempFrontDesktop to desktop
    int rightRotations = tempFrontDesktop - desktop;
    if( rightRotations < 0 )
        rightRotations += effects->numberOfDesktops();
    int leftRotations = desktop - tempFrontDesktop;
    if( leftRotations < 0 )
        leftRotations += effects->numberOfDesktops();
    if( leftRotations <= rightRotations )
        {
        for( int i=0; i<leftRotations; i++ )
            {
            rotations.enqueue( Left );
            }
        }
    else
        {
        for( int i=0; i<rightRotations; i++ )
            {
            rotations.enqueue( Right );
            }
        }
    if( !start && !rotating && !rotations.empty() )
        {
        rotating = true;
        rotationDirection = rotations.dequeue();
        }
    // change timeline curve if more rotations are following
    if( !rotations.empty() )
        {
        currentShape = TimeLine::EaseInCurve;
        timeLine.setCurveShape( currentShape );
        // change timeline duration in slide mode
        if( slide )
            timeLine.setDuration( rotationDuration / (rotations.count()+1) );
        }
    }

void CubeEffect::setActive( bool active )
    {
    if( active )
        {
        activated = true;
        activeScreen = effects->activeScreen();
        if( !slide )
            {
            keyboard_grab = effects->grabKeyboard( this );
            input = effects->createInputWindow( this, 0, 0, displayWidth(), displayHeight(),
                Qt::OpenHandCursor );
            frontDesktop = effects->currentDesktop();
            start = true;
            }
        effects->setActiveFullScreenEffect( this );
        kDebug() << "Cube is activated";
        verticalPosition = Normal;
        verticalRotating = false;
        manualAngle = 0.0;
        manualVerticalAngle = 0.0;
        if( reflection && !slide )
            {
            // clip parts above the reflection area
            double eqn[4] = {0.0, 1.0, 0.0, 0.0};
            glPushMatrix();
            QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
            if( effects->numScreens() > 1 && bigCube )
                rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
            glTranslatef( 0.0, rect.height(), 0.0 );
            glClipPlane( GL_CLIP_PLANE0, eqn );
            glPopMatrix();
            }
        // create the needed GL lists
        capList = glGenLists(1);
        capListCreated = false;
        
        effects->addRepaintFull();
        }
    else
        {
        schedule_close = true;
        // we have to add a repaint, to start the deactivating
        effects->addRepaintFull();
        }
    }

void CubeEffect::mouseChanged( const QPoint& pos, const QPoint& oldpos, Qt::MouseButtons buttons, 
            Qt::MouseButtons oldbuttons, Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers )
    {
    if( !activated )
        return;
    if( stop || slide )
        return;
    QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
    if( effects->numScreens() > 1 && (slide || bigCube ) )
        rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
    if( buttons.testFlag( Qt::LeftButton ) )
        {
        bool repaint = false;
        // vertical movement only if there is not a rotation
        if( !verticalRotating )
            {
            // display height corresponds to 180*
            int deltaY = oldpos.y() - pos.y();
            float deltaVerticalDegrees = (float)deltaY/rect.height()*180.0f;
            manualVerticalAngle -= deltaVerticalDegrees;
            if( deltaVerticalDegrees != 0.0 )
                repaint = true;
            }
        // horizontal movement only if there is not a rotation
        if( !rotating )
            {
            // display width corresponds to sum of angles of the polyhedron
            int deltaX = oldpos.x() - pos.x();
            float deltaDegrees = (float)deltaX/rect.width() * 360.0f;
            manualAngle -= deltaDegrees;
            if( deltaDegrees != 0.0 )
                repaint = true;
            }
        if( repaint )
            effects->addRepaintFull();
        }
    if( !oldbuttons.testFlag( Qt::LeftButton ) && buttons.testFlag( Qt::LeftButton ) )
        {
        XDefineCursor( display(), input, QCursor( Qt::ClosedHandCursor).handle() );
        }
    if( oldbuttons.testFlag( Qt::LeftButton) && !buttons.testFlag( Qt::LeftButton ) )
        {
        XDefineCursor( display(), input, QCursor( Qt::OpenHandCursor).handle() );
        }
    if( oldbuttons.testFlag( Qt::RightButton) && !buttons.testFlag( Qt::RightButton ) )
        {
        // end effect on right mouse button
        setActive( false );
        }
    }

void CubeEffect::desktopChanged( int old )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    if( activated )
        return;
    if( !animateDesktopChange )
        return;
    slide = true;
    oldDesktop = old;
    setActive( true );
    frontDesktop = old;
    rotateToDesktop( effects->currentDesktop() );
    setActive( false );
    }

} // namespace
