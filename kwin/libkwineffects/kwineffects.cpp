/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#include "kwineffects.h"

#include "kwinglutils.h"
#include "kwinxrenderutils.h"

#include <QtDBus/QtDBus>
#include <QVariant>
#include <QList>
#include <QtCore/QTimeLine>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include <kdebug.h>
#include <ksharedconfig.h>
#include <kstandarddirs.h>
#include <kconfiggroup.h>

#include <assert.h>

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xfixes.h>
#endif

namespace KWin
{

void WindowPrePaintData::setTranslucent()
{
    mask |= Effect::PAINT_WINDOW_TRANSLUCENT;
    mask &= ~Effect::PAINT_WINDOW_OPAQUE;
    clip = QRegion(); // cannot clip, will be transparent
}

void WindowPrePaintData::setTransformed()
{
    mask |= Effect::PAINT_WINDOW_TRANSFORMED;
}


WindowPaintData::WindowPaintData(EffectWindow* w)
    : opacity(w->opacity())
    , contents_opacity(1.0)
    , decoration_opacity(1.0)
    , xScale(1)
    , yScale(1)
    , zScale(1)
    , xTranslate(0)
    , yTranslate(0)
    , zTranslate(0)
    , saturation(1)
    , brightness(1)
    , shader(NULL)
    , rotation(NULL)
{
    quads = w->buildQuads();
}

ScreenPaintData::ScreenPaintData()
    : xScale(1)
    , yScale(1)
    , zScale(1)
    , xTranslate(0)
    , yTranslate(0)
    , zTranslate(0)
    , rotation(NULL)
{
}

RotationData::RotationData()
    : axis(ZAxis)
    , angle(0.0)
    , xRotationPoint(0.0)
    , yRotationPoint(0.0)
    , zRotationPoint(0.0)
{
}

//****************************************
// Effect
//****************************************

Effect::Effect()
{
}

Effect::~Effect()
{
}

void Effect::reconfigure(ReconfigureFlags)
{
}

void* Effect::proxy()
{
    return NULL;
}

void Effect::windowUserMovedResized(EffectWindow* , bool, bool)
{
}

void Effect::windowMoveResizeGeometryUpdate(EffectWindow* , const QRect&)
{
}

void Effect::windowOpacityChanged(EffectWindow*, double)
{
}

void Effect::windowDeleted(EffectWindow*)
{
}

void Effect::windowMinimized(EffectWindow*)
{
}

void Effect::windowUnminimized(EffectWindow*)
{
}

void Effect::clientGroupItemSwitched(EffectWindow*, EffectWindow*)
{
}

void Effect::clientGroupItemAdded(EffectWindow*, EffectWindow*)
{
}

void Effect::clientGroupItemRemoved(EffectWindow*, EffectWindow*)
{
}

void Effect::windowInputMouseEvent(Window, QEvent*)
{
}

void Effect::grabbedKeyboardEvent(QKeyEvent*)
{
}

void Effect::propertyNotify(EffectWindow* , long)
{
}

void Effect::windowDamaged(EffectWindow*, const QRect&)
{
}

void Effect::windowGeometryShapeChanged(EffectWindow*, const QRect&)
{
}

void Effect::tabBoxAdded(int)
{
}

void Effect::tabBoxClosed()
{
}

void Effect::tabBoxUpdated()
{
}

void Effect::tabBoxKeyEvent(QKeyEvent*)
{
}

bool Effect::borderActivated(ElectricBorder)
{
    return false;
}

void Effect::mouseChanged(const QPoint&, const QPoint&, Qt::MouseButtons,
                          Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
}

void Effect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    effects->prePaintScreen(data, time);
}

void Effect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);
}

void Effect::postPaintScreen()
{
    effects->postPaintScreen();
}

void Effect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    effects->prePaintWindow(w, data, time);
}

void Effect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    effects->paintWindow(w, mask, region, data);
}

void Effect::postPaintWindow(EffectWindow* w)
{
    effects->postPaintWindow(w);
}

void Effect::paintEffectFrame(KWin::EffectFrame* frame, QRegion region, double opacity, double frameOpacity)
{
    effects->paintEffectFrame(frame, region, opacity, frameOpacity);
}

bool Effect::provides(Feature)
{
    return false;
}

void Effect::drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    effects->drawWindow(w, mask, region, data);
}

void Effect::buildQuads(EffectWindow* w, WindowQuadList& quadList)
{
    effects->buildQuads(w, quadList);
}

QRect Effect::transformWindowDamage(EffectWindow* w, const QRect& r)
{
    return effects->transformWindowDamage(w, r);
}

void Effect::setPositionTransformations(WindowPaintData& data, QRect& region, EffectWindow* w,
                                        const QRect& r, Qt::AspectRatioMode aspect)
{
    QSize size = w->size();
    size.scale(r.size(), aspect);
    data.xScale = size.width() / double(w->width());
    data.yScale = size.height() / double(w->height());
    int width = int(w->width() * data.xScale);
    int height = int(w->height() * data.yScale);
    int x = r.x() + (r.width() - width) / 2;
    int y = r.y() + (r.height() - height) / 2;
    region = QRect(x, y, width, height);
    data.xTranslate = x - w->x();
    data.yTranslate = y - w->y();
}

int Effect::displayWidth()
{
    return KWin::displayWidth();
}

int Effect::displayHeight()
{
    return KWin::displayHeight();
}

QPoint Effect::cursorPos()
{
    return effects->cursorPos();
}

double Effect::animationTime(const KConfigGroup& cfg, const QString& key, int defaultTime)
{
    int time = cfg.readEntry(key, 0);
    return time != 0 ? time : qMax(defaultTime * effects->animationTimeFactor(), 1.);
}

double Effect::animationTime(int defaultTime)
{
    // at least 1ms, otherwise 0ms times can break some things
    return qMax(defaultTime * effects->animationTimeFactor(), 1.);
}

void Effect::numberDesktopsChanged(int)
{
}

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler(CompositingType type)
    : current_paint_screen(0)
    , current_paint_window(0)
    , current_draw_window(0)
    , current_build_quads(0)
    , current_transform(0)
    , compositing_type(type)
{
    if (compositing_type == NoCompositing)
        return;
    KWin::effects = this;
}

EffectsHandler::~EffectsHandler()
{
    // All effects should already be unloaded by Impl dtor
    assert(loaded_effects.count() == 0);
}

QRect EffectsHandler::transformWindowDamage(EffectWindow* w, const QRect& r)
{
    if (current_transform < loaded_effects.size()) {
        QRect rr = loaded_effects[current_transform++].second->transformWindowDamage(w, r);
        --current_transform;
        return rr;
    } else
        return r;
}

Window EffectsHandler::createInputWindow(Effect* e, const QRect& r, const QCursor& cursor)
{
    return createInputWindow(e, r.x(), r.y(), r.width(), r.height(), cursor);
}

Window EffectsHandler::createFullScreenInputWindow(Effect* e, const QCursor& cursor)
{
    return createInputWindow(e, 0, 0, displayWidth(), displayHeight(), cursor);
}

CompositingType EffectsHandler::compositingType() const
{
    return compositing_type;
}

bool EffectsHandler::saturationSupported() const
{
    switch(compositing_type) {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    case OpenGLCompositing:
        return GLTexture::saturationSupported();
#endif
    case XRenderCompositing:
        return false; // never
    default:
        abort();
    }
}

void EffectsHandler::sendReloadMessage(const QString& effectname)
{
    QDBusMessage message = QDBusMessage::createMethodCall("org.kde.kwin", "/KWin", "org.kde.KWin", "reconfigureEffect");
    message << QString("kwin4_effect_" + effectname);
    QDBusConnection::sessionBus().send(message);
}

KConfigGroup EffectsHandler::effectConfig(const QString& effectname)
{
    KSharedConfig::Ptr kwinconfig = KSharedConfig::openConfig("kwinrc", KConfig::NoGlobals);
    return kwinconfig->group("Effect-" + effectname);
}

bool EffectsHandler::checkDriverBlacklist(const KConfigGroup& blacklist)
{
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if (effects->compositingType() == OpenGLCompositing) {
        QString vendor   = QString((const char*)glGetString(GL_VENDOR));
        QString renderer = QString((const char*)glGetString(GL_RENDERER));
        QString version  = QString((const char*)glGetString(GL_VERSION));
        foreach (const QString & key, blacklist.keyList()) {
            // the key is a word in the renderer string or vendor referrencing the vendor in case of mesa
            // e.g. "Intel" or "Ati"
            if (renderer.contains(key, Qt::CaseInsensitive) || vendor.contains(key, Qt::CaseInsensitive)) {
                // the value for current key contains a string list of driver versions which have to be blacklisted
                QStringList versions = blacklist.readEntry< QStringList >(key, QStringList());
                foreach (const QString & entry, versions) {
                    QStringList parts = entry.split(":-:");
                    if (parts.size() != 2) {
                        continue;
                    }
                    if (renderer.contains(parts[0], Qt::CaseInsensitive) &&
                            version.contains(parts[1], Qt::CaseInsensitive)) {
                        // the version matches the renderer string - this driver is blacklisted, return
                        return true;
                    }
                }
            }
        }
    }
    return false;
#else
    return false;
#endif
}


EffectsHandler* effects = 0;


//****************************************
// EffectWindow
//****************************************

EffectWindow::EffectWindow()
{
}

EffectWindow::~EffectWindow()
{
}

bool EffectWindow::isOnCurrentDesktop() const
{
    return isOnDesktop(effects->currentDesktop());
}

bool EffectWindow::isOnDesktop(int d) const
{
    return desktop() == d || isOnAllDesktops();
}

bool EffectWindow::hasDecoration() const
{
    return contentsRect() != QRect(0, 0, width(), height());
}


//****************************************
// EffectWindowGroup
//****************************************

EffectWindowGroup::~EffectWindowGroup()
{
}

//****************************************
// GlobalShortcutsEditor
//****************************************

GlobalShortcutsEditor::GlobalShortcutsEditor(QWidget *parent) :
    KShortcutsEditor(parent, GlobalAction)
{
}

/***************************************************************
 WindowQuad
***************************************************************/

WindowQuad WindowQuad::makeSubQuad(double x1, double y1, double x2, double y2) const
{
    assert(x1 < x2 && y1 < y2 && x1 >= left() && x2 <= right() && y1 >= top() && y2 <= bottom());
#ifndef NDEBUG
    if (isTransformed())
        kFatal(1212) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
    WindowQuad ret(*this);
    // vertices are clockwise starting from topleft
    ret.verts[ 0 ].px = x1;
    ret.verts[ 3 ].px = x1;
    ret.verts[ 1 ].px = x2;
    ret.verts[ 2 ].px = x2;
    ret.verts[ 0 ].py = y1;
    ret.verts[ 1 ].py = y1;
    ret.verts[ 2 ].py = y2;
    ret.verts[ 3 ].py = y2;
    // original x/y are supposed to be the same, no transforming is done here
    ret.verts[ 0 ].ox = x1;
    ret.verts[ 3 ].ox = x1;
    ret.verts[ 1 ].ox = x2;
    ret.verts[ 2 ].ox = x2;
    ret.verts[ 0 ].oy = y1;
    ret.verts[ 1 ].oy = y1;
    ret.verts[ 2 ].oy = y2;
    ret.verts[ 3 ].oy = y2;
    double my_tleft = verts[ 0 ].tx;
    double my_tright = verts[ 2 ].tx;
    double my_ttop = verts[ 0 ].ty;
    double my_tbottom = verts[ 2 ].ty;
    double tleft = (x1 - left()) / (right() - left()) * (my_tright - my_tleft) + my_tleft;
    double tright = (x2 - left()) / (right() - left()) * (my_tright - my_tleft) + my_tleft;
    double ttop = (y1 - top()) / (bottom() - top()) * (my_tbottom - my_ttop) + my_ttop;
    double tbottom = (y2 - top()) / (bottom() - top()) * (my_tbottom - my_ttop) + my_ttop;
    ret.verts[ 0 ].tx = tleft;
    ret.verts[ 3 ].tx = tleft;
    ret.verts[ 1 ].tx = tright;
    ret.verts[ 2 ].tx = tright;
    ret.verts[ 0 ].ty = ttop;
    ret.verts[ 1 ].ty = ttop;
    ret.verts[ 2 ].ty = tbottom;
    ret.verts[ 3 ].ty = tbottom;
    return ret;
}

bool WindowQuad::smoothNeeded() const
{
    // smoothing is needed if the width or height of the quad does not match the original size
    double width = verts[ 1 ].ox - verts[ 0 ].ox;
    double height = verts[ 2 ].oy - verts[ 1 ].oy;
    return(verts[ 1 ].px - verts[ 0 ].px != width || verts[ 2 ].px - verts[ 3 ].px != width
           || verts[ 2 ].py - verts[ 1 ].py != height || verts[ 3 ].py - verts[ 0 ].py != height);
}

/***************************************************************
 WindowQuadList
***************************************************************/

WindowQuadList WindowQuadList::splitAtX(double x) const
{
    WindowQuadList ret;
    foreach (const WindowQuad & quad, *this) {
#ifndef NDEBUG
        if (quad.isTransformed())
            kFatal(1212) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        bool wholeleft = true;
        bool wholeright = true;
        for (int i = 0;
                i < 4;
                ++i) {
            if (quad[ i ].x() < x)
                wholeright = false;
            if (quad[ i ].x() > x)
                wholeleft = false;
        }
        if (wholeleft || wholeright) { // is whole in one split part
            ret.append(quad);
            continue;
        }
        if (quad.left() == quad.right()) { // quad has no size
            ret.append(quad);
            continue;
        }
        ret.append(quad.makeSubQuad(quad.left(), quad.top(), x, quad.bottom()));
        ret.append(quad.makeSubQuad(x, quad.top(), quad.right(), quad.bottom()));
    }
    return ret;
}

WindowQuadList WindowQuadList::splitAtY(double y) const
{
    WindowQuadList ret;
    foreach (const WindowQuad & quad, *this) {
#ifndef NDEBUG
        if (quad.isTransformed())
            kFatal(1212) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        bool wholetop = true;
        bool wholebottom = true;
        for (int i = 0;
                i < 4;
                ++i) {
            if (quad[ i ].y() < y)
                wholebottom = false;
            if (quad[ i ].y() > y)
                wholetop = false;
        }
        if (wholetop || wholebottom) { // is whole in one split part
            ret.append(quad);
            continue;
        }
        if (quad.top() == quad.bottom()) { // quad has no size
            ret.append(quad);
            continue;
        }
        ret.append(quad.makeSubQuad(quad.left(), quad.top(), quad.right(), y));
        ret.append(quad.makeSubQuad(quad.left(), y, quad.right(), quad.bottom()));
    }
    return ret;
}

WindowQuadList WindowQuadList::makeGrid(int maxquadsize) const
{
    if (empty())
        return *this;
    // find the bounding rectangle
    double left = first().left();
    double right = first().right();
    double top = first().top();
    double bottom = first().bottom();
    foreach (const WindowQuad & quad, *this) {
#ifndef NDEBUG
        if (quad.isTransformed())
            kFatal(1212) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        left = qMin(left, quad.left());
        right = qMax(right, quad.right());
        top = qMin(top, quad.top());
        bottom = qMax(bottom, quad.bottom());
    }
    WindowQuadList ret;
    for (double x = left;
            x < right;
            x += maxquadsize) {
        for (double y = top;
                y < bottom;
                y += maxquadsize) {
            foreach (const WindowQuad & quad, *this) {
                if (QRectF(QPointF(quad.left(), quad.top()), QPointF(quad.right(), quad.bottom()))
                        .intersects(QRectF(x, y, maxquadsize, maxquadsize))) {
                    ret.append(quad.makeSubQuad(qMax(x, quad.left()), qMax(y, quad.top()),
                                                qMin(quad.right(), x + maxquadsize), qMin(quad.bottom(), y + maxquadsize)));
                }
            }
        }
    }
    return ret;
}

WindowQuadList WindowQuadList::makeRegularGrid(int xSubdivisions, int ySubdivisions) const
{
    if (empty())
        return *this;
    // find the bounding rectangle
    double left = first().left();
    double right = first().right();
    double top = first().top();
    double bottom = first().bottom();
    foreach (const WindowQuad & quad, *this) {
#ifndef NDEBUG
        if (quad.isTransformed())
            kFatal(1212) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        left = qMin(left, quad.left());
        right = qMax(right, quad.right());
        top = qMin(top, quad.top());
        bottom = qMax(bottom, quad.bottom());
    }

    double xincrement = (right - left) / xSubdivisions;
    double yincrement = (bottom - top) / ySubdivisions;
    WindowQuadList ret;
    for (double y = top;
            y < bottom;
            y += yincrement) {
        for (double x = left;
                x < right;
                x +=  xincrement) {
            foreach (const WindowQuad & quad, *this) {
                if (QRectF(QPointF(quad.left(), quad.top()), QPointF(quad.right(), quad.bottom()))
                        .intersects(QRectF(x, y, xincrement, yincrement))) {
                    ret.append(quad.makeSubQuad(qMax(x, quad.left()), qMax(y, quad.top()),
                                                qMin(quad.right(), x + xincrement), qMin(quad.bottom(), y + yincrement)));
                }
            }
        }
    }
    return ret;
}

void WindowQuadList::makeArrays(float** vertices, float** texcoords) const
{
    *vertices = new float[ count() * 6 * 2 ];
    *texcoords = new float[ count() * 6 * 2 ];
    float* vpos = *vertices;
    float* tpos = *texcoords;
    for (int i = 0;
            i < count();
            ++i) {
        *vpos++ = at(i)[ 1 ].x();
        *vpos++ = at(i)[ 1 ].y();
        *vpos++ = at(i)[ 0 ].x();
        *vpos++ = at(i)[ 0 ].y();
        *vpos++ = at(i)[ 3 ].x();
        *vpos++ = at(i)[ 3 ].y();
        *vpos++ = at(i)[ 3 ].x();
        *vpos++ = at(i)[ 3 ].y();
        *vpos++ = at(i)[ 2 ].x();
        *vpos++ = at(i)[ 2 ].y();
        *vpos++ = at(i)[ 1 ].x();
        *vpos++ = at(i)[ 1 ].y();

        *tpos++ = at(i)[ 1 ].tx;
        *tpos++ = at(i)[ 1 ].ty;
        *tpos++ = at(i)[ 0 ].tx;
        *tpos++ = at(i)[ 0 ].ty;
        *tpos++ = at(i)[ 3 ].tx;
        *tpos++ = at(i)[ 3 ].ty;
        *tpos++ = at(i)[ 3 ].tx;
        *tpos++ = at(i)[ 3 ].ty;
        *tpos++ = at(i)[ 2 ].tx;
        *tpos++ = at(i)[ 2 ].ty;
        *tpos++ = at(i)[ 1 ].tx;
        *tpos++ = at(i)[ 1 ].ty;
    }
}

WindowQuadList WindowQuadList::select(WindowQuadType type) const
{
    foreach (const WindowQuad & q, *this) {
        if (q.type() != type) { // something else than ones to select, make a copy and filter
            WindowQuadList ret;
            foreach (const WindowQuad & q, *this) {
                if (q.type() == type)
                    ret.append(q);
            }
            return ret;
        }
    }
    return *this; // nothing to filter out
}

WindowQuadList WindowQuadList::filterOut(WindowQuadType type) const
{
    foreach (const WindowQuad & q, *this) {
        if (q.type() == type) { // something to filter out, make a copy and filter
            WindowQuadList ret;
            foreach (const WindowQuad & q, *this) {
                if (q.type() != type)
                    ret.append(q);
            }
            return ret;
        }
    }
    return *this; // nothing to filter out
}

bool WindowQuadList::smoothNeeded() const
{
    foreach (const WindowQuad & q, *this)
    if (q.smoothNeeded())
        return true;
    return false;
}

bool WindowQuadList::isTransformed() const
{
    foreach (const WindowQuad & q, *this)
    if (q.isTransformed())
        return true;
    return false;
}

/***************************************************************
 PaintClipper
***************************************************************/

QStack< QRegion >* PaintClipper::areas = NULL;

PaintClipper::PaintClipper(const QRegion& allowed_area)
    : area(allowed_area)
{
    push(area);
}

PaintClipper::~PaintClipper()
{
    pop(area);
}

void PaintClipper::push(const QRegion& allowed_area)
{
    if (allowed_area == infiniteRegion())  // don't push these
        return;
    if (areas == NULL)
        areas = new QStack< QRegion >;
    areas->push(allowed_area);
}

void PaintClipper::pop(const QRegion& allowed_area)
{
    if (allowed_area == infiniteRegion())
        return;
    Q_ASSERT(areas != NULL);
    Q_ASSERT(areas->top() == allowed_area);
    areas->pop();
    if (areas->isEmpty()) {
        delete areas;
        areas = NULL;
    }
}

bool PaintClipper::clip()
{
    return areas != NULL;
}

QRegion PaintClipper::paintArea()
{
    assert(areas != NULL);   // can be called only with clip() == true
    QRegion ret = QRegion(0, 0, displayWidth(), displayHeight());
    foreach (const QRegion & r, *areas)
    ret &= r;
    return ret;
}

struct PaintClipper::Iterator::Data {
    Data() : index(0) {}
    int index;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    QVector< QRect > rects;
#endif
};

PaintClipper::Iterator::Iterator()
    : data(new Data)
{
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if (clip() && effects->compositingType() == OpenGLCompositing) {
#ifndef KWIN_HAVE_OPENGLES
        glPushAttrib(GL_SCISSOR_BIT);
#endif
        if (!effects->isRenderTargetBound())
            glEnable(GL_SCISSOR_TEST);
        data->rects = paintArea().rects();
        data->index = -1;
        next(); // move to the first one
    }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (clip() && effects->compositingType() == XRenderCompositing) {
        XserverRegion region = toXserverRegion(paintArea());
        XFixesSetPictureClipRegion(display(), effects->xrenderBufferPicture(), 0, 0, region);
        XFixesDestroyRegion(display(), region);   // it's ref-counted
    }
#endif
}

PaintClipper::Iterator::~Iterator()
{
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if (clip() && effects->compositingType() == OpenGLCompositing) {
        if (!effects->isRenderTargetBound())
            glDisable(GL_SCISSOR_TEST);
#ifndef KWIN_HAVE_OPENGLES
        glPopAttrib();
#endif
    }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (clip() && effects->compositingType() == XRenderCompositing)
        XFixesSetPictureClipRegion(display(), effects->xrenderBufferPicture(), 0, 0, None);
#endif
    delete data;
}

bool PaintClipper::Iterator::isDone()
{
    if (!clip())
        return data->index == 1; // run once
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if (effects->compositingType() == OpenGLCompositing)
        return data->index >= data->rects.count(); // run once per each area
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (effects->compositingType() == XRenderCompositing)
        return data->index == 1; // run once
#endif
    abort();
}

void PaintClipper::Iterator::next()
{
    data->index++;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if (clip() && effects->compositingType() == OpenGLCompositing && !effects->isRenderTargetBound() && data->index < data->rects.count()) {
        const QRect& r = data->rects[ data->index ];
        // Scissor rect has to be given in OpenGL coords
        glScissor(r.x(), displayHeight() - r.y() - r.height(), r.width(), r.height());
    }
#endif
}

QRect PaintClipper::Iterator::boundingRect() const
{
    if (!clip())
        return infiniteRegion();
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if (effects->compositingType() == OpenGLCompositing)
        return data->rects[ data->index ];
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (effects->compositingType() == XRenderCompositing)
        return paintArea().boundingRect();
#endif
    abort();
    return infiniteRegion();
}


/***************************************************************
 TimeLine
***************************************************************/

TimeLine::TimeLine(const int duration)
{
    m_Time = 0;
    m_Duration = duration;
    m_TimeLine = new QTimeLine(m_Duration ? m_Duration : 1); // (avoid QTimeLine warning)
    m_TimeLine->setFrameRange(0, m_Duration);
    setCurveShape(EaseInCurve);
}

TimeLine::TimeLine(const TimeLine &other)
{
    m_Time = other.m_Time;
    m_Duration = other.m_Duration;
    m_TimeLine = new QTimeLine(m_Duration ? m_Duration : 1);
    m_TimeLine->setFrameRange(0, m_Duration);
    setCurveShape(other.m_CurveShape);
    if (m_Duration != 0)
        setProgress(m_Progress);
}

TimeLine::~TimeLine()
{
    delete m_TimeLine;
}

int TimeLine::duration() const
{
    return m_Duration;
}

void TimeLine::setDuration(const int msec)
{
    m_Duration = msec;
    m_TimeLine->setDuration(m_Duration);
    m_TimeLine->setFrameRange(0, m_Duration);
}

double TimeLine::value() const
{
    Q_ASSERT(m_Duration != 0);
    return valueForTime(m_Time);
}

double TimeLine::valueForTime(const int msec) const
{
    Q_ASSERT(m_Duration != 0);
    // Catch non QTimeLine CurveShapes here, (but there are none right now)


    // else use QTimeLine ...
    return m_TimeLine->valueForTime(msec);
}

void TimeLine::addTime(const int msec)
{
    Q_ASSERT(m_Duration != 0);
    m_Time = qMin(m_Duration, m_Time + msec);
    m_Progress = (double)m_Time / m_Duration;
}

void TimeLine::removeTime(const int msec)
{
    Q_ASSERT(m_Duration != 0);
    m_Time = qMax(0, m_Time - msec);
    m_Progress = (double)m_Time / m_Duration;
}

void TimeLine::setProgress(const double progress)
{
    Q_ASSERT(m_Duration != 0);
    m_Progress = progress;
    m_Time = qRound(m_Duration * progress);
}

double TimeLine::progress() const
{
    Q_ASSERT(m_Duration != 0);
    return m_Progress;
}

int TimeLine::time() const
{
    Q_ASSERT(m_Duration != 0);
    return m_Time;
}

void TimeLine::addProgress(const double progress)
{
    Q_ASSERT(m_Duration != 0);
    m_Progress += progress;
    m_Time = (int)(m_Duration * m_Progress);
}

void TimeLine::setCurveShape(CurveShape curveShape)
{
    switch(curveShape) {
    case EaseInCurve:
        m_TimeLine->setCurveShape(QTimeLine::EaseInCurve);
        break;
    case EaseOutCurve:
        m_TimeLine->setCurveShape(QTimeLine::EaseOutCurve);
        break;
    case EaseInOutCurve:
        m_TimeLine->setCurveShape(QTimeLine::EaseInOutCurve);
        break;
    case LinearCurve:
        m_TimeLine->setCurveShape(QTimeLine::LinearCurve);
        break;
    case SineCurve:
        m_TimeLine->setCurveShape(QTimeLine::SineCurve);
        break;
    }
    m_CurveShape = curveShape;
}

/***************************************************************
 Motion1D
***************************************************************/

Motion1D::Motion1D(double initial, double strength, double smoothness)
    : Motion<double>(initial, strength, smoothness)
{
}

Motion1D::Motion1D(const Motion1D &other)
    : Motion<double>(other)
{
}

Motion1D::~Motion1D()
{
}

/***************************************************************
 Motion2D
***************************************************************/

Motion2D::Motion2D(QPointF initial, double strength, double smoothness)
    : Motion<QPointF>(initial, strength, smoothness)
{
}

Motion2D::Motion2D(const Motion2D &other)
    : Motion<QPointF>(other)
{
}

Motion2D::~Motion2D()
{
}

/***************************************************************
 WindowMotionManager
***************************************************************/

WindowMotionManager::WindowMotionManager(bool useGlobalAnimationModifier)
    :   m_useGlobalAnimationModifier(useGlobalAnimationModifier)

{
    // TODO: Allow developer to modify motion attributes
} // TODO: What happens when the window moves by an external force?

WindowMotionManager::~WindowMotionManager()
{
}

void WindowMotionManager::manage(EffectWindow *w)
{
    if (m_managedWindows.contains(w))
        return;

    double strength = 0.08;
    double smoothness = 4.0;
    if (m_useGlobalAnimationModifier && effects->animationTimeFactor()) {
        // If the factor is == 0 then we just skip the calculation completely
        strength = 0.08 / effects->animationTimeFactor();
        smoothness = effects->animationTimeFactor() * 4.0;
    }

    m_managedWindows[ w ] = WindowMotion();
    m_managedWindows[ w ].translation.setStrength(strength);
    m_managedWindows[ w ].translation.setSmoothness(smoothness);
    m_managedWindows[ w ].scale.setStrength(strength * 1.33);
    m_managedWindows[ w ].scale.setSmoothness(smoothness / 2.0);

    m_managedWindows[ w ].translation.setValue(w->pos());
    m_managedWindows[ w ].scale.setValue(QPointF(1.0, 1.0));
}

void WindowMotionManager::unmanage(EffectWindow *w)
{
    if (!m_managedWindows.contains(w))
        return;

    QPointF diffT = m_managedWindows[ w ].translation.distance();
    QPointF diffS = m_managedWindows[ w ].scale.distance();

    m_movingWindowsSet.remove(w);
    m_managedWindows.remove(w);
}

void WindowMotionManager::unmanageAll()
{
    m_managedWindows.clear();
    m_movingWindowsSet.clear();
}

void WindowMotionManager::calculate(int time)
{
    if (!effects->animationTimeFactor()) {
        // Just skip it completely if the user wants no animation
        m_movingWindowsSet.clear();
        QHash<EffectWindow*, WindowMotion>::iterator it = m_managedWindows.begin();
        for (; it != m_managedWindows.end(); it++) {
            WindowMotion *motion = &it.value();
            motion->translation.finish();
            motion->scale.finish();
        }
    }

    QHash<EffectWindow*, WindowMotion>::iterator it = m_managedWindows.begin();
    for (; it != m_managedWindows.end(); it++) {
        WindowMotion *motion = &it.value();
        bool stopped = false;

        // TODO: What happens when distance() == 0 but we are still moving fast?
        // TODO: Motion needs to be calculated from the window's center

        QPointF diffT = motion->translation.distance();
        if (diffT != QPoint(0.0, 0.0)) {
            // Still moving
            motion->translation.calculate(time);
            diffT = motion->translation.distance();
            if (qAbs(diffT.x()) < 0.5 && qAbs(motion->translation.velocity().x()) < 0.2 &&
                    qAbs(diffT.y()) < 0.5 && qAbs(motion->translation.velocity().y()) < 0.2) {
                // Hide tiny oscillations
                motion->translation.finish();
                diffT = QPoint(0.0, 0.0);
                stopped = true;
            }
        }

        QPointF diffS = motion->scale.distance();
        if (diffS != QPoint(0.0, 0.0)) {
            // Still scaling
            motion->scale.calculate(time);
            diffS = motion->scale.distance();
            if (qAbs(diffS.x()) < 0.001 && qAbs(motion->scale.velocity().x()) < 0.05 &&
                    qAbs(diffS.y()) < 0.001 && qAbs(motion->scale.velocity().y()) < 0.05) {
                // Hide tiny oscillations
                motion->scale.finish();
                diffS = QPoint(0.0, 0.0);
                stopped = true;
            }
        }

        // We just finished this window's motion
        if (stopped && diffT == QPoint(0.0, 0.0) && diffS == QPoint(0.0, 0.0))
            m_movingWindowsSet.remove(it.key());
    }
}

void WindowMotionManager::reset()
{
    if (!m_managedWindows.count())
        return;

    EffectWindowList windows = m_managedWindows.keys();

    for (int i = 0; i < windows.size(); i++) {
        EffectWindow *w = windows.at(i);
        m_managedWindows[ w ].translation.setTarget(w->pos());
        m_managedWindows[ w ].translation.finish();
        m_managedWindows[ w ].scale.setTarget(QPointF(1.0, 1.0));
        m_managedWindows[ w ].scale.finish();
    }
}

void WindowMotionManager::reset(EffectWindow *w)
{
    if (!m_managedWindows.contains(w))
        return;

    m_managedWindows[ w ].translation.setTarget(w->pos());
    m_managedWindows[ w ].translation.finish();
    m_managedWindows[ w ].scale.setTarget(QPointF(1.0, 1.0));
    m_managedWindows[ w ].scale.finish();
}

void WindowMotionManager::apply(EffectWindow *w, WindowPaintData &data)
{
    if (!m_managedWindows.contains(w))
        return;

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    data.xTranslate += m_managedWindows[ w ].translation.value().x() - w->x();
    data.yTranslate += m_managedWindows[ w ].translation.value().y() - w->y();
    data.xScale *= m_managedWindows[ w ].scale.value().x();
    data.yScale *= m_managedWindows[ w ].scale.value().y();
}

void WindowMotionManager::moveWindow(EffectWindow *w, QPoint target, double scale, double yScale)
{
    if (!m_managedWindows.contains(w))
        abort(); // Notify the effect author that they did something wrong

    if (yScale == 0.0)
        yScale = scale;
    QPointF scalePoint(scale, yScale);

    if (m_managedWindows[ w ].translation.value() == target &&
            m_managedWindows[ w ].scale.value() == scalePoint)
        return; // Window already at that position

    m_managedWindows[ w ].translation.setTarget(target);
    m_managedWindows[ w ].scale.setTarget(scalePoint);

    m_movingWindowsSet << w;
}

QRectF WindowMotionManager::transformedGeometry(EffectWindow *w) const
{
    QRectF geometry(w->geometry());

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    geometry.moveTo(m_managedWindows[ w ].translation.value());
    geometry.setWidth(geometry.width() * m_managedWindows[ w ].scale.value().x());
    geometry.setHeight(geometry.height() * m_managedWindows[ w ].scale.value().y());

    return geometry;
}

void WindowMotionManager::setTransformedGeometry(EffectWindow *w, const QRectF &geometry)
{
    m_managedWindows[ w ].translation.setValue(geometry.topLeft());
    m_managedWindows[ w ].scale.setValue(QPointF(geometry.width() / qreal(w->width()),
                                         geometry.height() / qreal(w->height())));
}

QRectF WindowMotionManager::targetGeometry(EffectWindow *w) const
{
    QRectF geometry(w->geometry());

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    geometry.moveTo(m_managedWindows[ w ].translation.target());
    geometry.setWidth(geometry.width() * m_managedWindows[ w ].scale.target().x());
    geometry.setHeight(geometry.height() * m_managedWindows[ w ].scale.target().y());

    return geometry;
}

EffectWindow* WindowMotionManager::windowAtPoint(QPoint point, bool useStackingOrder) const
{
    Q_UNUSED(useStackingOrder);
    // TODO: Stacking order uses EffectsHandler::stackingOrder() then filters by m_managedWindows
    QHash< EffectWindow*, WindowMotion >::ConstIterator it = m_managedWindows.constBegin();
    while (it != m_managedWindows.constEnd()) {
        if (transformedGeometry(it.key()).contains(point))
            return it.key();
        ++it;
    }

    return NULL;
}

/***************************************************************
 EffectFramePrivate
***************************************************************/
class EffectFramePrivate
{
public:
    EffectFramePrivate();
    ~EffectFramePrivate();

    bool crossFading;
    qreal crossFadeProgress;
};

EffectFramePrivate::EffectFramePrivate()
    : crossFading(false)
    , crossFadeProgress(1.0)
{
}

EffectFramePrivate::~EffectFramePrivate()
{
}

/***************************************************************
 EffectFrame
***************************************************************/
EffectFrame::EffectFrame()
    : d(new EffectFramePrivate)
{
}

EffectFrame::~EffectFrame()
{
    delete d;
}

qreal EffectFrame::crossFadeProgress() const
{
    return d->crossFadeProgress;
}

void EffectFrame::setCrossFadeProgress(qreal progress)
{
    d->crossFadeProgress = progress;
}

bool EffectFrame::isCrossFade() const
{
    return d->crossFading;
}

void EffectFrame::enableCrossFade(bool enable)
{
    d->crossFading = enable;
}

} // namespace

#include "kwineffects.moc"
