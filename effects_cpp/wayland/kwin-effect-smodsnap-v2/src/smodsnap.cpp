/*
 * SPDX-FileCopyrightText: 2024 Souris
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "smodsnap.h"
#include "smod.h"

#include <KDecoration3/Decoration>
#include <KDecoration3/DecoratedWindow>

static void ensureResources()
{
    Q_INIT_RESOURCE(smodsnap);
}

namespace KWin
{

SmodSnapEffect::SmodSnapEffect()
{
    connect(effects, &EffectsHandler::windowAdded, this, &SmodSnapEffect::windowAdded);

    reconfigure(ReconfigureAll);

    // NOTE is this needed?
    //effects->makeOpenGLContextCurrent();

    anim1 = new SnapAnimation();
    anim2 = new SnapAnimation();

    loadTextures();

    m_shader = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture,
        QString(),
        QStringLiteral(":/effects/smodsnap/shaders/shader.frag")
    );
}

SmodSnapEffect::~SmodSnapEffect()
{
    if (anim1) {
        delete anim1;
    }

    if (anim2) {
        delete anim2;
    }
}

bool SmodSnapEffect::supported()
{
    return effects->isOpenGLCompositing() && SMOD::resourceExists(QStringLiteral("snapeffecttextures"));
}

void SmodSnapEffect::reconfigure(Effect::ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    SMOD::registerResource(QStringLiteral("snapeffecttextures"));

    if (effects->compositingType() == OpenGLCompositing) {
        ensureResources();
    }
}

void SmodSnapEffect::windowAdded(KWin::EffectWindow *w)
{
    if (w->isOutline()) {
        if (!anim1->m_active) {
            anim1->m_active = true;
            anim1->m_finished = false;
            anim1->m_frame = 0;
            anim1->m_progress = 0;
            anim1->m_clock.reset();

            const QPoint framesize = m_size * m_scale;
            const QPoint pos = effects->cursorPos().toPoint() - (framesize / 2);
            anim1->m_rect = Rect(pos, QSize(framesize.x(), framesize.y()));
        } else if (!anim2->m_active) {
            anim2->m_active = true;
            anim2->m_finished = false;
            anim2->m_frame = 0;
            anim2->m_progress = 0;
            anim2->m_clock.reset();

            const QPoint framesize = m_size * m_scale;
            const QPoint pos = effects->cursorPos().toPoint() - (framesize / 2);
            anim2->m_rect = Rect(pos, QSize(framesize.x(), framesize.y()));
        }
    }
}

void SmodSnapEffect::prePaintScreen(ScreenPrePaintData &data)
{
    if (anim1->m_active) {
        const int time = anim2->m_clock.tick(data.view).count();

        // NOTE we need to do (m_frames + 1) here so the last frame
        // will play for the same amount of time as the rest
        anim1->m_progress = (anim1->m_progress + time) % (m_speed * (m_frames + 1));
        anim1->m_frame = (int)((qreal)anim1->m_progress / (qreal)m_speed) % (m_frames + 1);

        if (anim1->m_frame == m_frames) {
            anim1->m_finished = true;
        } else {
            data.paint = data.paint.united(anim1->m_rect);
        }
    }

    if (anim2->m_active) {
        const int time = anim2->m_clock.tick(data.view).count();

        // NOTE we need to do (m_frames + 1) here so the last animation frame
        // will play for the same amount of time as the rest
        anim2->m_progress = (anim2->m_progress + time) % (m_speed * (m_frames + 1));
        anim2->m_frame = qRound((qreal)anim2->m_progress / (qreal)m_speed) % (m_frames + 1);

        if (anim2->m_frame == m_frames) {
            anim2->m_finished = true;
        } else {
            data.paint = data.paint.united(anim2->m_rect);
        }
    }

    effects->prePaintScreen(data);
}

void SmodSnapEffect::postPaintScreen()
{
    if (anim1->m_active) {
        effects->addRepaint(anim1->m_rect);

        if (anim1->m_finished) {
            anim1->m_active = false;
        }
    }

    if (anim2->m_active) {
        effects->addRepaint(anim2->m_rect);

        if (anim2->m_finished) {
            anim2->m_active = false;
        }
    }

    effects->postPaintScreen();
}

}
