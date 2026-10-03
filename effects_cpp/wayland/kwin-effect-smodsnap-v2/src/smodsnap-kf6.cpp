/*
 * SPDX-FileCopyrightText: 2024 Souris
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "smodsnap.h"

#include "core/rendertarget.h"

#include <KConfig>
#include <KConfigGroup>

namespace KWin
{

void SmodSnapEffect::loadTextures()
{
    KConfig config(QStringLiteral(":/effects/smodsnap/animation/animrc"));
    KConfigGroup generalGroup(&config, QStringLiteral("General"));

    m_frames   = generalGroup.readEntry("frames", 0);
    m_speed    = generalGroup.readEntry("speed",  0);
    m_scale    = generalGroup.readEntry("scale",  1.0);
    int width  = generalGroup.readEntry("width",  0);
    int height = generalGroup.readEntry("height", 0);
    m_size     = QPoint(width, height);

    m_texture.resize(m_frames);

    for (int i = 0; i < m_frames; ++i) {
        m_texture[i] = GLTexture::upload(QPixmap(QStringLiteral(":/effects/smodsnap/animation/frame") + QString::number(i + 1)));
        m_texture[i]->setFilter(GL_LINEAR);
        m_texture[i]->setWrapMode(GL_CLAMP_TO_EDGE);
    }
}

void SmodSnapEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &region, LogicalOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, region, screen);

    if (anim1->m_active || anim2->m_active) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
        binder.shader()->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);

        const auto toXYZ = renderTarget.colorDescription()->containerColorimetry().toXYZ();
        binder.shader()->setUniform(GLShader::Vec3Uniform::PrimaryBrightness, QVector3D(toXYZ(1, 0), toXYZ(1, 1), toXYZ(1, 2)));

        const auto scale = viewport.scale();

        if (!anim1->m_finished) {
            const QRectF pixelGeometry = snapToPixelGridF(anim1->m_rect.scaled(scale));
            QMatrix4x4 mvp = viewport.projectionMatrix();
            mvp.translate(anim1->m_rect.x() * scale, anim1->m_rect.y() * scale);
            binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);
            GLTexture *texture = m_texture[anim1->m_frame].get();
            texture->render(pixelGeometry.size());
        }

        if (!anim2->m_finished) {
            const QRectF pixelGeometry = snapToPixelGridF(anim2->m_rect.scaled(scale));
            QMatrix4x4 mvp = viewport.projectionMatrix();
            mvp.translate(anim2->m_rect.x() * scale, anim2->m_rect.y() * scale);
            binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);
            GLTexture *texture = m_texture[anim2->m_frame].get();
            texture->render(pixelGeometry.size());
        }

        glDisable(GL_BLEND);
    }
}

}
