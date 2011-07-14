/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; see the file COPYING.  if not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#include "blur.h"
#include "blurshader.h"

#include <X11/Xatom.h>

#include <QMatrix4x4>
#include <QLinkedList>
#include <KConfigGroup>
#include <KDebug>

namespace KWin
{

KWIN_EFFECT(blur, BlurEffect)
KWIN_EFFECT_SUPPORTED(blur, BlurEffect::supported())
KWIN_EFFECT_ENABLEDBYDEFAULT(blur, BlurEffect::enabledByDefault())


BlurEffect::BlurEffect()
{
    shader = BlurShader::create();

    // Offscreen texture that's used as the target for the horizontal blur pass
    // and the source for the vertical pass.
    tex = new GLTexture(displayWidth(), displayHeight());
    tex->setFilter(GL_LINEAR);
    tex->setWrapMode(GL_CLAMP_TO_EDGE);

    target = new GLRenderTarget(tex);

    net_wm_blur_region = XInternAtom(display(), "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
    effects->registerPropertyType(net_wm_blur_region, true);

    reconfigure(ReconfigureAll);

    // ### Hackish way to announce support.
    //     Should be included in _NET_SUPPORTED instead.
    if (shader->isValid() && target->valid()) {
        XChangeProperty(display(), rootWindow(), net_wm_blur_region, net_wm_blur_region,
                        32, PropModeReplace, 0, 0);
    } else {
        XDeleteProperty(display(), rootWindow(), net_wm_blur_region);
    }
    connect(effects, SIGNAL(windowAdded(EffectWindow*)), this, SLOT(slotWindowAdded(EffectWindow*)));
    connect(effects, SIGNAL(propertyNotify(EffectWindow*,long)), this, SLOT(slotPropertyNotify(EffectWindow*,long)));
}

BlurEffect::~BlurEffect()
{
    effects->registerPropertyType(net_wm_blur_region, false);
    XDeleteProperty(display(), rootWindow(), net_wm_blur_region);

    delete shader;
    delete target;
    delete tex;
}

void BlurEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    KConfigGroup cg = EffectsHandler::effectConfig("Blur");
    int radius = qBound(2, cg.readEntry("BlurRadius", 12), 14);
    shader->setRadius(radius);

    if (!shader->isValid())
        XDeleteProperty(display(), rootWindow(), net_wm_blur_region);
}

void BlurEffect::updateBlurRegion(EffectWindow *w) const
{
    QRegion region;

    const QByteArray value = w->readProperty(net_wm_blur_region, XA_CARDINAL, 32);
    if (value.size() > 0 && !(value.size() % (4 * sizeof(unsigned long)))) {
        const unsigned long *cardinals = reinterpret_cast<const unsigned long*>(value.constData());
        for (unsigned int i = 0; i < value.size() / sizeof(unsigned long);) {
            int x = cardinals[i++];
            int y = cardinals[i++];
            int w = cardinals[i++];
            int h = cardinals[i++];
            region += QRect(x, y, w, h);
        }
    }

    if (region.isEmpty() && !value.isNull()) {
        // Set the data to a dummy value.
        // This is needed to be able to distinguish between the value not
        // being set, and being set to an empty region.
        w->setData(WindowBlurBehindRole, 1);
    } else
        w->setData(WindowBlurBehindRole, region);
}

void BlurEffect::slotWindowAdded(EffectWindow *w)
{
    updateBlurRegion(w);
}

void BlurEffect::slotPropertyNotify(EffectWindow *w, long atom)
{
    if (w && atom == net_wm_blur_region)
        updateBlurRegion(w);
}

bool BlurEffect::enabledByDefault()
{
    GLPlatform *gl = GLPlatform::instance();

    if (gl->isIntel())
        return false;

    return true;
}

bool BlurEffect::supported()
{
    bool supported = GLRenderTarget::supported() && GLTexture::NPOTTextureSupported() &&
                     (GLSLBlurShader::supported() || ARBBlurShader::supported());

    if (supported) {
        int maxTexSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

        if (displayWidth() > maxTexSize || displayHeight() > maxTexSize)
            supported = false;
    }
    return supported;
}

QRect BlurEffect::expand(const QRect &rect) const
{
    const int radius = shader->radius();
    return rect.adjusted(-radius, -radius, radius, radius);
}

QRegion BlurEffect::expand(const QRegion &region) const
{
    QRegion expanded;

    if (region.rectCount() < 20) {
        foreach (const QRect & rect, region.rects())
        expanded += expand(rect);
    } else
        expanded += expand(region.boundingRect());

    return expanded;
}

QRegion BlurEffect::blurRegion(const EffectWindow *w) const
{
    QRegion region;

    const QVariant value = w->data(WindowBlurBehindRole);
    if (value.isValid()) {
        const QRegion appRegion = qvariant_cast<QRegion>(value);
        if (!appRegion.isEmpty()) {
            if (w->hasDecoration() && effects->decorationSupportsBlurBehind()) {
                region = w->shape();
                region -= w->decorationInnerRect();
                region |= appRegion.translated(w->contentsRect().topLeft()) &
                          w->contentsRect();
            } else
                region = appRegion & w->contentsRect();
        } else {
            // An empty region means that the blur effect should be enabled
            // for the whole window.
            region = w->shape();
        }
    } else if (w->hasDecoration() && effects->decorationSupportsBlurBehind()) {
        // If the client hasn't specified a blur region, we'll only enable
        // the effect behind the decoration.
        region = w->shape();
        region -= w->decorationInnerRect();
    }

    return region;
}

void BlurEffect::drawRegion(const QRegion &region)
{
    const int vertexCount = region.rectCount() * 6;
    if (vertices.size() < vertexCount)
        vertices.resize(vertexCount);

    int i = 0;
    foreach (const QRect & r, region.rects()) {
        vertices[i++] = QVector2D(r.x() + r.width(), r.y());
        vertices[i++] = QVector2D(r.x(),             r.y());
        vertices[i++] = QVector2D(r.x(),             r.y() + r.height());
        vertices[i++] = QVector2D(r.x(),             r.y() + r.height());
        vertices[i++] = QVector2D(r.x() + r.width(), r.y() + r.height());
        vertices[i++] = QVector2D(r.x() + r.width(), r.y());
    }
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setData(vertexCount, 2, (float*)vertices.constData(), (float*)vertices.constData());
    vbo->render(GL_TRIANGLES);
}

void BlurEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    EffectWindowList windows = effects->stackingOrder();
    QLinkedList<QRegion> blurRegions;
    bool checkDecos = effects->decorationsHaveAlpha() && effects->decorationSupportsBlurBehind();
    bool clipChanged = false;

    effects->prePaintScreen(data, time);

    // If the whole screen will be repainted anyway, there is no point in
    // adding to the paint region.
    if (data.mask & (PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS | PAINT_SCREEN_TRANSFORMED))
        return;

    if (effects->activeFullScreenEffect()) {
        // search for a window which has the force blur rule set
        // if there is such a window we need to do the normal blur handling
        // if there is no such window blur is blocked during the fullscreen effect
        bool hasForce = false;
        foreach (EffectWindow *w, windows) {
            if (w->data(WindowForceBlurRole).toBool()) {
                hasForce = true;
                break;
            }
        }
        if (!hasForce) {
            return;
        }
    }

    // Walk the window list top->bottom and check if the paint region is fully
    // contained within opaque windows and doesn't intersect any blurred region.
    QRegion paint = data.paint;
    for (int i = windows.count() - 1; i >= 0; --i) {
        EffectWindow *window = windows.at(i);
        if (!window->isPaintingEnabled())
            continue;

        if (!window->hasAlpha()) {
            paint -= window->contentsRect().translated(window->pos());
            if (paint.isEmpty())
                break;
        }

        // The window decoration is treated as an object below the window
        // contents, so check it after the contents.
        if (window->hasAlpha() || (checkDecos && window->hasDecoration())) {
            QRegion r = blurRegion(window);
            if (r.isEmpty())
                continue;

            r = expand(r.translated(window->pos()));
            if (r.intersects(paint))
                break;
        }
    }

    if (paint.isEmpty())
        return;

    // Walk the list again bottom->top and expand the paint region when
    // it intersects a blurred region.
    foreach (EffectWindow *window, windows) {
        if (!window->isPaintingEnabled())
            continue;

        if (!window->hasAlpha() && !(checkDecos && window->hasDecoration()))
            continue;

        QRegion r = blurRegion(window);
        if (r.isEmpty())
            continue;

        r = expand(r.translated(window->pos()));

        // We can't do a partial repaint of a blurred region
        if (r.intersects(data.paint)) {
            data.paint += r;
            clipChanged = true;
        } else
            blurRegions.append(r);
    }

    while (clipChanged) {
        clipChanged = false;
        QMutableLinkedListIterator<QRegion> i(blurRegions);
        while (i.hasNext()) {
            const QRegion r = i.next();
            if (!r.intersects(data.paint))
                continue;

            data.paint += r;
            clipChanged = true;
            i.remove();
        }
    }

    // Force the scene to call paintGenericScreen() so the windows are painted bottom -> top
    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS;
}

bool BlurEffect::shouldBlur(const EffectWindow *w, int mask, const WindowPaintData &data) const
{
    if (!target->valid() || !shader->isValid())
        return false;

    // Don't blur anything if we're painting top-to-bottom
    if (!(mask & (PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS |
                  PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS |
                  PAINT_SCREEN_TRANSFORMED)))
        return false;

    if (effects->activeFullScreenEffect() && !w->data(WindowForceBlurRole).toBool())
        return false;

    if (w->isDesktop())
        return false;

    bool scaled = !qFuzzyCompare(data.xScale, 1.0) && !qFuzzyCompare(data.yScale, 1.0);
    bool translated = data.xTranslate || data.yTranslate;

    if (scaled || translated || (mask & PAINT_WINDOW_TRANSFORMED))
        return false;

    bool blurBehindDecos = effects->decorationsHaveAlpha() &&
                effects->decorationSupportsBlurBehind();

    if (!w->hasAlpha() && !(blurBehindDecos && w->hasDecoration()))
        return false;

    return true;
}

void BlurEffect::drawWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (shouldBlur(w, mask, data)) {
        const QRect screen(0, 0, displayWidth(), displayHeight());
        const QRegion shape = blurRegion(w).translated(w->pos()) & screen;

        if (!shape.isEmpty() && region.intersects(shape.boundingRect()))
            doBlur(shape, screen, data.opacity * data.contents_opacity);
    }

    // Draw the window over the blurred area
    effects->drawWindow(w, mask, region, data);
}

void BlurEffect::paintEffectFrame(EffectFrame *frame, QRegion region, double opacity, double frameOpacity)
{
    const QRect screen(0, 0, displayWidth(), displayHeight());
    bool valid = target->valid() && shader->isValid();
    QRegion shape = frame->geometry().adjusted(-5, -5, 5, 5) & screen;
    if (valid && !shape.isEmpty() && region.intersects(shape.boundingRect()) && frame->style() != EffectFrameNone) {
        doBlur(shape, screen, opacity * frameOpacity);
    }
    effects->paintEffectFrame(frame, region, opacity, frameOpacity);
}

void BlurEffect::doBlur(const QRegion& shape, const QRect& screen, const float opacity)
{
    const QRegion expanded = expand(shape) & screen;
    const QRect r = expanded.boundingRect();

    // Create a scratch texture and copy the area in the back buffer that we're
    // going to blur into it
    GLTexture scratch(r.width(), r.height());
    scratch.setFilter(GL_LINEAR);
    scratch.setWrapMode(GL_CLAMP_TO_EDGE);
    scratch.bind();

    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r.x(), displayHeight() - r.y() - r.height(),
                        r.width(), r.height());

    // Draw the texture on the offscreen framebuffer object, while blurring it horizontally
    GLRenderTarget::pushRenderTarget(target);

    shader->bind();
    shader->setDirection(Qt::Horizontal);
    shader->setPixelDistance(1.0 / r.width());

    // Set up the texture matrix to transform from screen coordinates
    // to texture coordinates.
#ifndef KWIN_HAVE_OPENGLES
    glMatrixMode(GL_TEXTURE);
#endif
    pushMatrix();
    QMatrix4x4 textureMatrix;
    textureMatrix.scale(1.0 / scratch.width(), -1.0 / scratch.height(), 1);
    textureMatrix.translate(-r.x(), -scratch.height() - r.y(), 0);
    loadMatrix(textureMatrix);
    shader->setTextureMatrix(textureMatrix);

    drawRegion(expanded);

    GLRenderTarget::popRenderTarget();
    scratch.unbind();
    scratch.discard();

    // Now draw the horizontally blurred area back to the backbuffer, while
    // blurring it vertically and clipping it to the window shape.
    tex->bind();

    shader->setDirection(Qt::Vertical);
    shader->setPixelDistance(1.0 / tex->height());

    // Modulate the blurred texture with the window opacity if the window isn't opaque
    if (opacity < 1.0) {
#ifndef KWIN_HAVE_OPENGLES
        glPushAttrib(GL_COLOR_BUFFER_BIT);
#endif
        glEnable(GL_BLEND);
        glBlendColor(0, 0, 0, opacity);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    }

    // Set the up the texture matrix to transform from screen coordinates
    // to texture coordinates.
    textureMatrix.setToIdentity();
    textureMatrix.scale(1.0 / tex->width(), -1.0 / tex->height(), 1);
    textureMatrix.translate(0, -tex->height(), 0);
    loadMatrix(textureMatrix);
    shader->setTextureMatrix(textureMatrix);

    drawRegion(shape);

    popMatrix();
#ifndef KWIN_HAVE_OPENGLES
    glMatrixMode(GL_MODELVIEW);
#endif

    if (opacity < 1.0) {
        glDisable(GL_BLEND);
#ifndef KWIN_HAVE_OPENGLES
        glPopAttrib();
#endif
    }

    tex->unbind();
    shader->unbind();
}

} // namespace KWin

