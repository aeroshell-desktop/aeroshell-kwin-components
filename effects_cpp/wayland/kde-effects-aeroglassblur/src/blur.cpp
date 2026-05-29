/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "blur.h"
// KConfigSkeleton
#include "blurconfig.h"

#include <core/backendoutput.h>
#include <core/pixelgrid.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <effect/effecthandler.h>
#include <opengl/glplatform.h>
#include <scene/backgroundeffectitem.h>
#include <scene/decorationitem.h>
#include <scene/scene.h>
#include <scene/surfaceitem.h>
#include <scene/windowitem.h>
#include <wayland/backgroundeffect_v1.h>
#include <wayland/display.h>
#include <wayland/surface.h>
#include <wayland_server.h>
#include <window.h>

#include <QGuiApplication>
#include <QMatrix4x4>
#include <QScreen>
#include <QTime>
#include <QTimer>
#include <QWindow>
#include <cmath> // for ceil()
#include <cstdlib>
#include <QBuffer>
#include <QPainterPath>
#include <QFile>

#include <KConfigGroup>
#include <KSharedConfig>

#include <KDecoration3/Decoration>

#include "hsvrgb.h"
#include "wackyfunc.h"

#define TRANSFORMATION_DATA 128
#define OPACITY_DATA 129

#define AS_MENUREP "aeroshell-menurepresentation"

Q_LOGGING_CATEGORY(KWIN_BLUR, "kwin_effect_aeroglassblur", QtWarningMsg)

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(aeroblur);
}

namespace KWin
{

static const QByteArray s_blurAtomName = QByteArrayLiteral("_KDE_NET_WM_BLUR_BEHIND_REGION");

static QMatrix4x4 colorTransformMatrix(qreal saturation, qreal brightness)
{
    QMatrix4x4 saturationMatrix;
    if (saturation != 1.0) {
        const qreal r = (1.0 - saturation) * .2126;
        const qreal g = (1.0 - saturation) * .7152;
        const qreal b = (1.0 - saturation) * .0722;

        saturationMatrix = QMatrix4x4(r + saturation, r, r, 0.0,
                                      g, g + saturation, g, 0.0,
                                      b, b, b + saturation, 0.0,
                                      0, 0, 0, 1.0);
    }

    QMatrix4x4 brightnessMatrix;
    if (brightness != 1.0) {
        brightnessMatrix = QMatrix4x4(brightness, 0, 0, 0,
                                      0, brightness, 0, 0,
                                      0, 0, brightness, 0,
                                      0, 0, 0, brightness);
    }

    return saturationMatrix * brightnessMatrix;
}

BlurEffect::BlurEffect()
    : m_sharedMemory("kwinaero")
{
    BlurConfig::instance(effects->config());
    ensureResources();

    m_downsamplePass.shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                                QStringLiteral(":/effects/aeroblur/shaders/vertex.vert"),
                                                                                QStringLiteral(":/effects/aeroblur/shaders/downsample.frag"));
    if (!m_downsamplePass.shader) {
        qCWarning(KWIN_BLUR) << "Failed to load downsampling pass shader";
        return;
    } else {
        m_downsamplePass.mvpMatrixLocation = m_downsamplePass.shader->uniformLocation("modelViewProjectionMatrix");
        m_downsamplePass.offsetLocation = m_downsamplePass.shader->uniformLocation("offset");
        m_downsamplePass.halfpixelLocation = m_downsamplePass.shader->uniformLocation("halfpixel");
    }

    m_upsamplePass.shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                              QStringLiteral(":/effects/aeroblur/shaders/vertex.vert"),
                                                                              QStringLiteral(":/effects/aeroblur/shaders/upsample.frag"));
    if (!m_upsamplePass.shader) {
        qCWarning(KWIN_BLUR) << "Failed to load upsampling pass shader";
        return;
    } else {
        m_upsamplePass.mvpMatrixLocation = m_upsamplePass.shader->uniformLocation("modelViewProjectionMatrix");
        m_upsamplePass.offsetLocation = m_upsamplePass.shader->uniformLocation("offset");
        m_upsamplePass.halfpixelLocation = m_upsamplePass.shader->uniformLocation("halfpixel");
    }

    for(int i = 0; i < 3; i++) {
        qCWarning(KWIN_BLUR) << "Loading shader " << aeroShaderLocations[i];
        m_aeroPasses[i].shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                                   QStringLiteral(":/effects/aeroblur/shaders/vertex.vert"),
                                                                                   aeroShaderLocations[i]);
        if (!m_aeroPasses[i].shader) {
            qCWarning(KWIN_BLUR) << "Failed to load aero pass shader " << aeroShaderLocations[i];
            return;
        } else {
            m_aeroPasses[i].mvpMatrixLocation            = m_aeroPasses[i].shader->uniformLocation("modelViewProjectionMatrix");
            m_aeroPasses[i].offsetLocation               = m_aeroPasses[i].shader->uniformLocation("offset");
            m_aeroPasses[i].halfpixelLocation            = m_aeroPasses[i].shader->uniformLocation("halfpixel");
            m_aeroPasses[i].colorMatrixLocation          = m_aeroPasses[i].shader->uniformLocation("colorMatrix");
            m_aeroPasses[i].aeroColorRLocation           = m_aeroPasses[i].shader->uniformLocation("aeroColorR");
            m_aeroPasses[i].aeroColorGLocation           = m_aeroPasses[i].shader->uniformLocation("aeroColorG");
            m_aeroPasses[i].aeroColorBLocation           = m_aeroPasses[i].shader->uniformLocation("aeroColorB");
            m_aeroPasses[i].aeroColorALocation           = m_aeroPasses[i].shader->uniformLocation("aeroColorA");
            m_aeroPasses[i].aeroColorBalanceLocation     = m_aeroPasses[i].shader->uniformLocation("aeroColorBalance");
            m_aeroPasses[i].aeroAfterglowBalanceLocation = m_aeroPasses[i].shader->uniformLocation("aeroAfterglowBalance");
            m_aeroPasses[i].aeroBlurBalanceLocation      = m_aeroPasses[i].shader->uniformLocation("aeroBlurBalance");
        }

    }

    m_reflectPass.shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                             QStringLiteral(":/effects/aeroblur/shaders/vertex.vert"),
                                                                             QStringLiteral(":/effects/aeroblur/shaders/reflect.frag"));

    if (!m_reflectPass.shader) {
        qCWarning(KWIN_BLUR) << "Failed to load reflect pass shader";
        return;
    } else {
        m_reflectPass.mvpMatrixLocation = m_reflectPass.shader->uniformLocation("modelViewProjectionMatrix");
        m_reflectPass.opacityLocation   = m_reflectPass.shader->uniformLocation("opacity");
        m_reflectPass.screenResolutionLocation = m_reflectPass.shader->uniformLocation("screenResolution");
        m_reflectPass.windowPosLocation = m_reflectPass.shader->uniformLocation("windowPos");
        m_reflectPass.windowSizeLocation = m_reflectPass.shader->uniformLocation("windowSize");
        m_reflectPass.translateTextureLocation = m_reflectPass.shader->uniformLocation("translate");
        m_reflectPass.colorMatrixLocation = m_reflectPass.shader->uniformLocation("colorMatrix");
        m_reflectPass.reflectTextureLocation = m_reflectPass.shader->uniformLocation("texUnit");
        // Glow
        m_reflectPass.textureSizeLocation = m_reflectPass.shader->uniformLocation("textureSize");
        m_reflectPass.useWaylandLocation = m_reflectPass.shader->uniformLocation("useWayland");
        m_reflectPass.glowTextureLocation = m_reflectPass.shader->uniformLocation("glowTexture");
        m_reflectPass.glowEnableLocation = m_reflectPass.shader->uniformLocation("glowEnable");
        m_reflectPass.glowOpacityLocation = m_reflectPass.shader->uniformLocation("glowOpacity");

    }

    m_reflectPass.sideGlowTexture = GLTexture::upload(QPixmap(QStringLiteral(":/effects/aeroblur/framecornereffect.png")));
    m_reflectPass.sideGlowTexture->setFilter(GL_LINEAR_MIPMAP_LINEAR);
    m_reflectPass.sideGlowTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_reflectPass.sideGlowTexture_unfocus = GLTexture::upload(QPixmap(QStringLiteral(":/effects/aeroblur/framecornereffect-unfocus.png")));
    m_reflectPass.sideGlowTexture_unfocus->setFilter(GL_LINEAR_MIPMAP_LINEAR);
    m_reflectPass.sideGlowTexture_unfocus->setWrapMode(GL_CLAMP_TO_EDGE);

    initBlurStrengthValues();
    reconfigure(ReconfigureAll);

    waylandServer()->backgroundEffectManager()->addBlurCapability();

    connect(effects, &EffectsHandler::windowAdded, this, &BlurEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &BlurEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::viewRemoved, this, &BlurEffect::slotViewRemoved);

    // Fetch the blur regions for all windows
    const auto stackingOrder = effects->stackingOrder();
    for (EffectWindow *window : stackingOrder) {
        slotWindowAdded(window);
    }

    m_valid = true;
}

BlurEffect::~BlurEffect()
{
    waylandServer()->backgroundEffectManager()->removeBlurCapability();
}

void BlurEffect::initBlurStrengthValues()
{
    // This function creates an array of blur strength values that are evenly distributed

    // The range of the slider on the blur settings UI
    int numOfBlurSteps = 15;
    int remainingSteps = numOfBlurSteps;

    /*
     * Explanation for these numbers:
     *
     * The texture blur amount depends on the downsampling iterations and the offset value.
     * By changing the offset we can alter the blur amount without relying on further downsampling.
     * But there is a minimum and maximum value of offset per downsample iteration before we
     * get artifacts.
     *
     * The minOffset variable is the minimum offset value for an iteration before we
     * get blocky artifacts because of the downsampling.
     *
     * The maxOffset value is the maximum offset value for an iteration before we
     * get diagonal line artifacts because of the nature of the dual kawase blur algorithm.
     *
     * The expandSize value is the minimum value for an iteration before we reach the end
     * of a texture in the shader and sample outside of the area that was copied into the
     * texture from the screen.
     */

    // {minOffset, maxOffset, expandSize}
    blurOffsets.append({1.0, 2.0, 10}); // Down sample size / 2
    blurOffsets.append({2.0, 3.0, 20}); // Down sample size / 4
    blurOffsets.append({2.0, 5.0, 50}); // Down sample size / 8
    blurOffsets.append({3.0, 8.0, 150}); // Down sample size / 16
    // blurOffsets.append({5.0, 10.0, 400}); // Down sample size / 32
    // blurOffsets.append({7.0, ?.0});       // Down sample size / 64

    float offsetSum = 0;

    for (int i = 0; i < blurOffsets.size(); i++) {
        offsetSum += blurOffsets[i].maxOffset - blurOffsets[i].minOffset;
    }

    for (int i = 0; i < blurOffsets.size(); i++) {
        int iterationNumber = std::ceil((blurOffsets[i].maxOffset - blurOffsets[i].minOffset) / offsetSum * numOfBlurSteps);
        remainingSteps -= iterationNumber;

        if (remainingSteps < 0) {
            iterationNumber += remainingSteps;
        }

        float offsetDifference = blurOffsets[i].maxOffset - blurOffsets[i].minOffset;

        for (int j = 1; j <= iterationNumber; j++) {
            // {iteration, offset}
            blurStrengthValues.append({i + 1, blurOffsets[i].minOffset + (offsetDifference / iterationNumber) * j});
        }
    }
}

bool BlurEffect::readMemory(bool *skipFunc)
{
    if(!m_sharedMemory.attach())
    {
        qCWarning(KWIN_BLUR) << "Couldn't access shared memory! " << m_sharedMemory.nativeKey() << " " << m_sharedMemory.error();
        if(m_sharedMemory.error())
            return false;
    }
    QBuffer buffer;
    QDataStream in(&buffer);

    int ah, as, ab, ai;
    bool transparencyEnabled;
    bool skip;
    m_sharedMemory.lock();
    buffer.setData((char*)m_sharedMemory.constData(), m_sharedMemory.size());
    buffer.open(QBuffer::ReadOnly);
    in >> ah >> as >> ab >> ai >> transparencyEnabled >> skip;
    m_sharedMemory.unlock();
    m_sharedMemory.detach();

    m_aeroIntensity  = ai;
    m_aeroHue        = ah;
    m_aeroSaturation = as;
    m_aeroBrightness = ab;
    m_transparencyEnabled = transparencyEnabled;
    *skipFunc = skip;
    return true;
}

void BlurEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    auto configureAero = [&]() {
        float fR = 0, fG = 0, fB = 0, fH = 0, fS = 0, fV = 0;

        fH = (float)m_aeroHue;
        fS = ((float)m_aeroSaturation) / 100.0f;
        fV = ((float)m_aeroBrightness) / 100.0f;

        HSVtoRGB(fR, fG, fB, fH, fS, fV);

        int primaryBalance, secondaryBalance, blurBalance;
        getColorBalances(m_aeroIntensity, primaryBalance, secondaryBalance, blurBalance);

        m_aeroPrimaryBalance   = primaryBalance;
        m_aeroSecondaryBalance = secondaryBalance;
        m_aeroBlurBalance      = blurBalance;
        m_aeroPrimaryBalanceInactive = 0.4f * m_aeroPrimaryBalance;
        m_aeroBlurBalanceInactive = 0.4f * m_aeroBlurBalance + 60;

        m_aeroColorR = fR;
        m_aeroColorG = fG;
        m_aeroColorB = fB;
        m_aeroColorA = (m_aeroIntensity - 26) / 191.0f;

        getMaximizedColorization(m_aeroIntensity, m_aeroColorR, m_aeroColorG, m_aeroColorB, m_aeroColorROpaque, m_aeroColorGOpaque, m_aeroColorBOpaque);
        if(m_aeroIntensity < 26) {
            m_aeroColorA = m_aeroIntensity / 255.0f;
        }
    };
    bool skip = false;
    bool readColor = readMemory(&skip) && m_firstTimeConfig;
    if(skip && m_firstTimeConfig)
    {
        configureAero();
        for (auto &[window, data] : m_windows) {
            data.blurItem->setPixelsToExpandRepaintsBelowOpaqueRegions(m_expandSize);
        }
        effects->addRepaintFull();
        return;
    }

    BlurConfig::self()->read();
    if(!readColor)
    {
        m_aeroIntensity  = BlurConfig::aeroIntensity();
        m_aeroHue        = BlurConfig::aeroHue();
        m_aeroSaturation = BlurConfig::aeroSaturation();
        m_aeroBrightness = BlurConfig::aeroBrightness();
        m_transparencyEnabled = BlurConfig::enableTransparency();

        configureAero();
    }
    m_reflectionIntensity = BlurConfig::reflectionIntensity();

    int blurStrength = BlurConfig::blurStrength()-1;
    m_iterationCount = blurStrengthValues[blurStrength].iteration;
    m_offset = blurStrengthValues[blurStrength].offset;
    m_expandSize = blurOffsets[m_iterationCount - 1].expandSize;
    m_blurMatching = BlurConfig::blurMatching();
    m_blurNonMatching = BlurConfig::blurNonMatching();
    m_windowClasses = BlurConfig::windowClasses().split("\n");
    m_noBlurWindowClasses = BlurConfig::noBlurWindowClasses().split("\n");
    m_windowClassesColorization = BlurConfig::excludedColorization().split("\n");
    m_firefoxWindows = BlurConfig::blurFirefox().split("\n");

    m_firefoxCornerRadius = BlurConfig::firefoxCornerRadius();
    m_firefoxBlurTopMargin = BlurConfig::firefoxBlurTopMargin();
    m_firefoxHollowRegion = BlurConfig::firefoxHollowRegion();
    m_opaqueKrunner = BlurConfig::opaqueKrunner();
    m_opaqueOSD = BlurConfig::opaqueOSD();

    m_blurMenus = BlurConfig::blurMenus();
    m_blurDocks = BlurConfig::blurDocks();
    m_paintAsTranslucent = BlurConfig::paintAsTranslucent();
    m_basicColorization = BlurConfig::basicColorization();
    m_maximizeColorization = BlurConfig::maximizeColorization();
    m_enableCornerGlow = BlurConfig::enableCornerGlow();
    m_translateTexture = BlurConfig::translateTexture();
    m_texturePath = BlurConfig::textureLocation();
    ensureReflectTexture();

    m_firstTimeConfig = true;
    for (auto &[window, data] : m_windows) {
        data.blurItem->setPixelsToExpandRepaintsBelowOpaqueRegions(m_expandSize);
    }

    // Update all windows for the blur to take effect
    effects->addRepaintFull();
}

bool BlurEffect::isFirefoxWindowValid(KWin::EffectWindow *w)
{
    // Because Wayland (and Firefox probably) does things differently
    if(!w->isNormalWindow()) return false;
    QStringList classes = w->windowClass().split((' '));
    if(w->isWaylandClient())
    {
        return m_firefoxWindows.contains(classes[0]);
    }
    bool valid = classes[0] == QStringLiteral("navigator") || classes[0] == QStringLiteral("Navigator");
    if(classes.size() > 1)
    {
        valid = valid && m_firefoxWindows.contains(classes[1]);
    }
    return valid;
}

RegionF BlurEffect::applyBlurRegion(KWin::EffectWindow *w, bool useFrame)
{
    auto maximizeState = w->window()->maximizeMode();
    const auto scale = w->screen()->scale();
    const int radius = maximizeState == MaximizeMode::MaximizeFull ? 0 : m_firefoxCornerRadius * scale;

    QPainterPath path;
    if(useFrame) {
        path.addRoundedRect(0, 0, w->frameGeometry().width(), w->frameGeometry().height(), radius, radius);
    } else {
        path.addRoundedRect(0, 0, w->expandedGeometry().width(), w->expandedGeometry().height(), radius, radius);
    }

    const int topMargin = m_firefoxBlurTopMargin * scale;
    const int margin = 9 * scale;

    RegionF mask(path.toFillPolygon().toPolygon());
    if (!m_firefoxHollowRegion || (mask.boundingRect().width() <= 2 * margin || mask.boundingRect().height() < topMargin + margin)) {
        return mask;
    }

    RectF hollowRect = mask.boundingRect();
    hollowRect.setWidth(hollowRect.width() - 2 * margin);
    hollowRect.setHeight(hollowRect.height() - margin - topMargin);
    RegionF hollowRegion(hollowRect);
    mask ^= hollowRegion.translated(margin, topMargin);
    return mask;
}

void BlurEffect::updateBlurRegion(EffectWindow *w)
{
    std::optional<RegionF> content;
    std::optional<RegionF> frame;

    if (SurfaceInterface *surface = w->surface()) {
        if (!surface->blurRegion().isEmpty()) {
            content = surface->blurRegion();
        }
    }

    if (auto internal = w->internalWindow()) {
        const auto property = internal->property("kwin_blur");
        if (property.isValid()) {
            content = property.value<RegionF>();
        }
    }

    if (w->decorationHasAlpha() && decorationSupportsBlurBehind(w)) {
        frame = decorationBlurRegion(w);
    }

    if (content.has_value() || frame.has_value()) {
        BlurEffectData &data = m_windows[w];
        data.content = content;
        data.frame = frame;
        if (!data.blurItem) {
            data.blurItem = std::make_unique<BackgroundEffectItem>(w->windowItem());
        }
        data.blurItem->setPixelsToExpandRepaintsBelowOpaqueRegions(m_expandSize);
        data.blurItem->setEffectBoundingRect(blurRegion(w).boundingRect());
    } else {
        if (auto it = m_windows.find(w); it != m_windows.end()) {
            effects->makeOpenGLContextCurrent();
            m_windows.erase(it);
        }
    }

}

void BlurEffect::slotWindowAdded(EffectWindow *w)
{
    SurfaceInterface *surf = w->surface();

    if (surf) {
        windowBlurChangedConnections[w] = connect(surf, &SurfaceInterface::blurChanged, this, [this, w]() {
            if (w) {
                updateBlurRegion(w);
            }
        });
    }

    windowExpandedGeometryChangedConnections[w] = connect(w, &EffectWindow::windowExpandedGeometryChanged, this, [this,w]() {
        if (w) {
            updateBlurRegion(w);
        }
    });

    auto maximizeState = w->window()->maximizeMode();
    if (maximizeState == MaximizeMode::MaximizeFull && !w->isMinimized()) {
        m_maximizedWindows.push_front(w);
    }

    if (auto internal = w->internalWindow()) {
        internal->installEventFilter(this);
    }

    connect(w, &EffectWindow::windowMaximizedStateChanged, this, &BlurEffect::slotWindowMaximizedStateChanged);
    connect(w, &EffectWindow::minimizedChanged, this, &BlurEffect::slotMinimizedChanged);
    connect(w, &EffectWindow::windowDecorationChanged, this, [this, w]() {
        setupDecorationConnections(w);
        updateBlurRegion(w);
    });

    setupDecorationConnections(w);
    updateBlurRegion(w);
}

void BlurEffect::slotWindowDeleted(EffectWindow *w)
{
    if (auto it = m_windows.find(w); it != m_windows.end()) {
        effects->makeOpenGLContextCurrent();
        m_windows.erase(it);
    }
    if (auto it = windowBlurChangedConnections.find(w); it != windowBlurChangedConnections.end()) {
        disconnect(*it);
        windowBlurChangedConnections.erase(it);
    }

    if (auto it = std::find(m_maximizedWindows.begin(), m_maximizedWindows.end(), w); it != m_maximizedWindows.end()) {
        m_maximizedWindows.erase(it);
    }
    if (auto it = windowExpandedGeometryChangedConnections.find(w); it != windowExpandedGeometryChangedConnections.end()) {
        disconnect(*it);
        windowExpandedGeometryChangedConnections.erase(it);
    }
}

void BlurEffect::slotWindowMaximizedStateChanged(KWin::EffectWindow *w, bool horizontal, bool vertical)
{
    if (horizontal && vertical && !w->isMinimized()) {
        m_maximizedWindows.push_front(w);
    } else {
        auto it = std::find(m_maximizedWindows.begin(), m_maximizedWindows.end(), w);
        if (it != m_maximizedWindows.end()) {
            m_maximizedWindows.erase(it);
        }
    }
}

void BlurEffect::slotMinimizedChanged(KWin::EffectWindow *w)
{
    if (w->isMinimized()) {
        auto it = std::find(m_maximizedWindows.begin(), m_maximizedWindows.end(), w);
        if (it != m_maximizedWindows.end()) {
            m_maximizedWindows.erase(it);
        }
    } else {
        auto maximizeState = w->window()->maximizeMode();
        if (maximizeState == MaximizeMode::MaximizeFull) {
            m_maximizedWindows.push_front(w);
        }
    }
}

void BlurEffect::slotViewRemoved(KWin::RenderView *view)
{
    for (auto &[window, data] : m_windows) {
        if (auto it = data.render.find(view); it != data.render.end()) {
            effects->makeOpenGLContextCurrent();
            data.render.erase(it);
        }
    }
}

void BlurEffect::setupDecorationConnections(EffectWindow *w)
{
    if (!w->decoration()) {
        return;
    }

    connect(w->decoration(), &KDecoration3::Decoration::blurRegionChanged, this, [this, w]() {
        updateBlurRegion(w);
    });
}

bool BlurEffect::eventFilter(QObject *watched, QEvent *event)
{
    auto internal = qobject_cast<QWindow *>(watched);
    if (internal && event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *pe = static_cast<QDynamicPropertyChangeEvent *>(event);
        if (pe->propertyName() == "kwin_blur") {
            if (auto w = effects->findWindow(internal)) {
                updateBlurRegion(w);
            }
        }
    }
    return false;
}

bool BlurEffect::enabledByDefault()
{
    return false;
}

bool BlurEffect::supported()
{
    return effects->isOpenGLCompositing();
}

bool BlurEffect::decorationSupportsBlurBehind(const EffectWindow *w) const
{
    return w->decoration() && !w->decoration()->blurRegion().isNull();
}

RegionF BlurEffect::decorationBlurRegion(const EffectWindow *w) const
{
    if (!decorationSupportsBlurBehind(w)) {
        return {};
    }

    RegionF decorationRegion = RegionF(w->decoration()->rect()) - w->contentsRect();
    //! we return only blurred regions that belong to decoration region
    return decorationRegion.intersected(RegionF(w->decoration()->blurRegion()));
}

RegionF BlurEffect::blurRegion(EffectWindow *w) const
{
    RegionF region;

    if (auto it = m_windows.find(w); it != m_windows.end()) {
        const std::optional<RegionF> &content = it->second.content;
        const std::optional<RegionF> &frame = it->second.frame;
        if (content.has_value()) {
            if (content->isEmpty()) {
                // An empty region means that the blur effect should be enabled
                // for the whole window.
                region = w->rect();
                if (w->decorationHasAlpha() && decorationSupportsBlurBehind(w)) {
                    region &= w->contentsRect();
                }
            } else {
                if (frame.has_value()) {
                    region = frame.value();
                }
                region += content->translated(w->contentsRect().topLeft()) & w->contentsRect(); // LIKELY_BUG
            }
        } else if (frame.has_value()) {
            region = frame.value();
        }
    }

    if (w->decorationHasAlpha() && decorationSupportsBlurBehind(w)) {
        // If the client hasn't specified a blur region, we'll only enable
        // the effect behind the decoration.
        region &= w->contentsRect();
        region |= decorationBlurRegion(w);

    }

    return region;
}

void BlurEffect::prePaintScreen(ScreenPrePaintData &data)
{
    m_currentView = data.view;

    // We can avoid checking for every window by evaluating the condition here
    auto maximizedWindowsOnCurrentActivity = [&]() -> bool {
        return std::find_if(m_maximizedWindows.begin(), m_maximizedWindows.end(),
                            [](const EffectWindow *a) { return a->isOnCurrentDesktop() && a->isOnCurrentActivity(); }) != m_maximizedWindows.end();
    };

    if (m_maximizeColorization) {
        m_maximizedWindowsInCurrentActivity = maximizedWindowsOnCurrentActivity();
    }

    effects->prePaintScreen(data);
}

void BlurEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    if (isFirefoxWindowValid(w)) {
        data.setTranslucent();
    }

    effects->prePaintWindow(view, w, data);
}

bool BlurEffect::shouldBlur(const EffectWindow *w, int mask, const WindowPaintData &data) const
{
    Q_UNUSED(mask)
    Q_UNUSED(data)

    QString windowClass = w->windowClass().split(' ')[0];
    if (effects->activeFullScreenEffect() && !w->data(WindowForceBlurRole).toBool()) {
        return false;
    }

    if (w->isOutline() || w->isDesktop() || (!w->isManaged() && !(windowClass == "plasmashell" || windowClass == "kwin_x11" || windowClass == "kwin_wayland"))) {
        return false;
    }

    return true;
}

bool BlurEffect::shouldForceBlur(const EffectWindow *w) const
{
    if ((!m_blurDocks && w->isDock()) || (!m_blurMenus && (w->isMenu() || w->isDropdownMenu() || w->isPopupMenu()))) {
        return false;
    }
    // For some reason, the Alt+Tab window on Wayland is made up of two windows, one of which is completely empty
    // and has an empty window class, and.. isn't a Wayland client???'
    if (effects->waylandDisplay() && !w->isWaylandClient() && w->window()->resourceName() == "") {
        return false;
    }

    if (w->isTooltip()) {
        return false;
    }

    // Is it a Gadget window
    bool matches = (w->window()->resourceName() == "plasmashell" || w->window()->resourceClass() == "plasmashell") && w->caption() == "plasmashell_explorer";
    if (matches) {
        return true;
    }

    matches = m_windowClasses.contains(w->window()->resourceName()) || m_windowClasses.contains(w->window()->resourceClass());
    return (matches && m_blurMatching) || (!matches && m_blurNonMatching);
}

bool BlurEffect::shouldNotBlur(const EffectWindow *w) const
{
    const QString resourceName = w->window()->resourceName();
    const QString resourceClass = w->window()->resourceClass();

    for (const QString &pattern : m_noBlurWindowClasses) {
        if (pattern.isEmpty()) {
            continue;
        }
        QRegularExpression regex(pattern);
        if (regex.match(resourceName).hasMatch() || regex.match(resourceClass).hasMatch()) {
            return true;
        }
    }

    return false;
}

void BlurEffect::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    blur(renderTarget, viewport, w, mask, deviceRegion, data);

    // Draw the window over the blurred area
    effects->drawWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

void BlurEffect::ensureReflectTexture() {
    if (m_texturePath == "" || !QFile::exists(m_texturePath)) {
        m_texturePath = QStringLiteral(":/effects/aeroblur/reflection.png");
    }

    QImage textureImage(m_texturePath);

    m_reflectPass.reflectTexture = GLTexture::upload(textureImage);
    m_reflectPass.reflectTexture->setFilter(GL_LINEAR_MIPMAP_LINEAR);
    m_reflectPass.reflectTexture->setWrapMode(GL_REPEAT);
}

void BlurEffect::blur(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    auto it = m_windows.find(w);
    if (it == m_windows.end()) {
        return;
    }

    BlurEffectData &blurInfo = it->second;
    BlurRenderData &renderInfo = blurInfo.render[m_currentView];
    if (shouldNotBlur(w)) {
        return;
    }
    if (!shouldBlur(w, mask, data)) {
        return;
    }

    // HDR brightness must be handled by color management in the compositor.
    double hdr_brightness_correction = 1.0;
    if (w->screen()->backendOutput()->highDynamicRange()) {
        hdr_brightness_correction = w->screen()->backendOutput()->brightnessSetting();
    }

    // Fetch window transformation data
    QMatrix4x4 transformedMatrix;
    QVariant winData = w->data(TRANSFORMATION_DATA);
    if (!winData.isNull()) {
        transformedMatrix = winData.value<QMatrix4x4>();
    }

    // Compute the effective blur shape. Note that if the window is transformed, so will be the blur shape.
    RegionF blurShape = blurRegion(w);
    if (data.xScale() != 1 || data.yScale() != 1) {
        blurShape.scale(data.xScale(), data.yScale());
    }
    if (data.xTranslation() || data.yTranslation()) {
        blurShape.translate(data.xTranslation(), data.yTranslation());
    }

    blurShape.translate(w->pos());

    Rect backgroundRect = blurShape.boundingRect().rounded();
    /*
     * The new way of downsampling works reliably for textures with
     * even dimensions, so we shrink the bounding rectangle by 1
     * on odd-sized regions. This helps prevent the blur shaking as
     * the user resizes windows.
     */
    if (backgroundRect.width() % 2 != 0) {
        backgroundRect.setWidth(backgroundRect.width() - 1);
    }
    if (backgroundRect.height() % 2 != 0) {
        backgroundRect.setHeight(backgroundRect.height() - 1);
    }
    const Rect scaledBackgroundRect = snapToPixelGrid(backgroundRect.scaled(viewport.scale()));
    const Rect deviceBackgroundRect = snapToPixelGrid(viewport.mapToDeviceCoordinates(backgroundRect));

    auto opacity = w->opacity() * data.opacity();
    QVariant opacityData = w->data(OPACITY_DATA);
    if (!opacityData.isNull()) {
        opacity *= opacityData.value<float>();
    }

    // Get the effective shape that will be actually blurred. It's possible that all of it will be clipped.
    QList<RectF> effectiveShape;
    effectiveShape.reserve(blurShape.rects().size());
    if (deviceRegion != Region::infinite()) {
        for (const Rect &clipRect : deviceRegion.rects()) {
            const RectF deviceClipRect = clipRect.translated(-deviceBackgroundRect.topLeft());
            for (const RectF &shapeRect : blurShape.rects()) {
                const RectF deviceShapeRect = shapeRect.translated(-backgroundRect.topLeft()).scaled(viewport.scale()).rounded();
                if (const RectF intersected = deviceClipRect.intersected(deviceShapeRect); !intersected.isEmpty()) {
                    effectiveShape.append(intersected);
                }
            }
        }
    } else {
        for (const RectF &rect : blurShape.rects()) {
            effectiveShape.append(rect.translated(-backgroundRect.topLeft()).scaled(viewport.scale()).rounded());
        }
    }
    if (effectiveShape.isEmpty()) {
        return;
    }

    // Maybe reallocate offscreen render targets. Keep in mind that the first one contains
    // original background behind the window, it's not blurred.
    GLenum textureFormat = GL_RGBA8;
    if (renderTarget.texture()) {
        textureFormat = renderTarget.texture()->internalFormat();
    }

    if (renderInfo.framebuffers.size() != (m_iterationCount + 1) || renderInfo.textures[0]->size() != backgroundRect.size() || renderInfo.textures[0]->internalFormat() != textureFormat) {
        renderInfo.framebuffers.clear();
        renderInfo.textures.clear();

        glClearColor(0, 0, 0, 0);
        for (size_t i = 0; i <= m_iterationCount; ++i) {
            const QSize textureSize(std::max(1, backgroundRect.width() / (1 << i)), std::max(1, backgroundRect.height() / (1 << i)));
            auto texture = GLTexture::allocate(textureFormat, textureSize);
            if (!texture) {
                qCWarning(KWIN_BLUR) << "Failed to allocate an offscreen texture";
                return;
            }
            texture->setFilter(GL_LINEAR);
            texture->setWrapMode(GL_CLAMP_TO_EDGE);

            auto framebuffer = std::make_unique<GLFramebuffer>(texture.get());
            if (!framebuffer->valid()) {
                qCWarning(KWIN_BLUR) << "Failed to create an offscreen framebuffer";
                return;
            }
            EglContext::currentContext()->pushFramebuffer(framebuffer.get());
            glClear(GL_COLOR_BUFFER_BIT);
            EglContext::currentContext()->popFramebuffer();
            renderInfo.textures.push_back(std::move(texture));
            renderInfo.framebuffers.push_back(std::move(framebuffer));
        }
    }

    // Fetch the pixels behind the shape that is going to be blurred.
    const Region dirtyRegion = viewport.mapFromDeviceCoordinatesContained(deviceRegion) & backgroundRect;
    for (const Rect &dirtyRect : dirtyRegion.rects()) {
        renderInfo.framebuffers[0]->blitFromRenderTarget(renderTarget, viewport, dirtyRect, dirtyRect.translated(-backgroundRect.topLeft()));
    }

    // Upload the geometry: the first 6 vertices are used when downsampling and upsampling offscreen,
    // the remaining vertices are used when rendering on the screen.
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    const int vertexCount = effectiveShape.size() * 6;
    if (auto result = vbo->map<GLVertex2D>(6 + vertexCount)) {
        auto map = *result;

        size_t vboIndex = 0;

        // The geometry that will be blurred offscreen, in logical pixels.
        {
            const RectF localRect = RectF(0, 0, backgroundRect.width(), backgroundRect.height());

            const float x0 = localRect.left();
            const float y0 = localRect.top();
            const float x1 = localRect.right();
            const float y1 = localRect.bottom();

            const float u0 = x0 / backgroundRect.width();
            const float v0 = 1.0f - y0 / backgroundRect.height();
            const float u1 = x1 / backgroundRect.width();
            const float v1 = 1.0f - y1 / backgroundRect.height();

            // first triangle
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x0, y0),
                .texcoord = QVector2D(u0, v0),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x1, y1),
                .texcoord = QVector2D(u1, v1),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x0, y1),
                .texcoord = QVector2D(u0, v1),
            };

            // second triangle
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x0, y0),
                .texcoord = QVector2D(u0, v0),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x1, y0),
                .texcoord = QVector2D(u1, v0),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x1, y1),
                .texcoord = QVector2D(u1, v1),
            };
        }

        // The geometry that will be painted on screen, in device pixels.
        for (const RectF &rect : effectiveShape) {
            const float x0 = rect.left();
            const float y0 = rect.top();
            const float x1 = rect.right();
            const float y1 = rect.bottom();

            const float u0 = x0 / scaledBackgroundRect.width();
            const float v0 = 1.0f - y0 / scaledBackgroundRect.height();
            const float u1 = x1 / scaledBackgroundRect.width();
            const float v1 = 1.0f - y1 / scaledBackgroundRect.height();

            // first triangle
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x0, y0),
                .texcoord = QVector2D(u0, v0),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x1, y1),
                .texcoord = QVector2D(u1, v1),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x0, y1),
                .texcoord = QVector2D(u0, v1),
            };

            // second triangle
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x0, y0),
                .texcoord = QVector2D(u0, v0),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x1, y0),
                .texcoord = QVector2D(u1, v0),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x1, y1),
                .texcoord = QVector2D(u1, v1),
            };
        }

        if (!winData.isNull()) // If the window sends transformation data, apply it to the painted geometry, skipping the offscreen geometry
        {
            for (int ind = 6; ind < 6+vertexCount; ind++) {
                // Apply transformation to the triangle vertex
                QPointF transformed = transformedMatrix.map(QPointF(map[ind].position.x(), map[ind].position.y()));
                // Calculate new uv coordinates so the sampling doesn't get distorted
                float u = transformed.x() / scaledBackgroundRect.width();
                float v = 1.0f - transformed.y() / scaledBackgroundRect.height();
                if(v < -1 && transformed.y() > scaledBackgroundRect.height()) return; // Prevents warped animations from running for too long, making them imperceptible
                // Update vertices and uv coordinates
                map[ind].position.setX(transformed.x());
                map[ind].position.setY(transformed.y());
                map[ind].texcoord.setX(u);
                map[ind].texcoord.setY(v);
            }
        }

        vbo->unmap();
    } else {
        qCWarning(KWIN_BLUR) << "Failed to map vertex buffer";
        return;
    }

    vbo->bindArrays();

    // The downsample pass of the dual Kawase algorithm: the background will be scaled down 50% every iteration.
    {
        ShaderManager::instance()->pushShader(m_downsamplePass.shader.get());

        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(QRectF(0.0, 0.0, backgroundRect.width(), backgroundRect.height()));

        m_downsamplePass.shader->setUniform(m_downsamplePass.mvpMatrixLocation, projectionMatrix);
        m_downsamplePass.shader->setUniform(m_downsamplePass.offsetLocation, float(m_offset));

        for (size_t i = 1; i < renderInfo.framebuffers.size(); ++i) {
            const auto &read = renderInfo.framebuffers[i - 1];
            const auto &draw = renderInfo.framebuffers[i];

            const QVector2D halfpixel(0.5 / (double)read->colorAttachment()->width(),
                                      0.5 / (double)read->colorAttachment()->height());
            m_downsamplePass.shader->setUniform(m_downsamplePass.halfpixelLocation, halfpixel);

            read->colorAttachment()->bind();

            GLFramebuffer::pushFramebuffer(draw.get());
            vbo->draw(GL_TRIANGLES, 0, 6);
        }

        ShaderManager::instance()->popShader();
    }

    // The upsample pass of the dual Kawase algorithm: the background will be scaled up 200% every iteration.
    {
        ShaderManager::instance()->pushShader(m_upsamplePass.shader.get());

        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(QRectF(0.0, 0.0, backgroundRect.width(), backgroundRect.height()));

        m_upsamplePass.shader->setUniform(m_upsamplePass.mvpMatrixLocation, projectionMatrix);
        m_upsamplePass.shader->setUniform(m_upsamplePass.offsetLocation, float(m_offset / 2.5f));

        for (size_t i = renderInfo.framebuffers.size() - 1; i > 1; --i) {
            GLFramebuffer::popFramebuffer();
            const auto &read = renderInfo.framebuffers[i];

            const QVector2D halfpixel(0.5 / (double)read->colorAttachment()->width(),
                                      0.5 / (double)read->colorAttachment()->height());
            m_upsamplePass.shader->setUniform(m_upsamplePass.halfpixelLocation, halfpixel);

            read->colorAttachment()->bind();

            vbo->draw(GL_TRIANGLES, 0, 6);
        }

        ShaderManager::instance()->popShader();
    }

    const float modulation = opacity * opacity;
    const QMatrix4x4 colorMatrix = colorTransformMatrix(data.saturation() * hdr_brightness_correction, data.brightness());
    const bool opaqueMaximize = shouldOpaqueColorize(w);

    // Blurring and colorization
    {
        /*********************
         * COLORIZATION PASS *
         *********************/
        float basicAlpha = m_aeroIntensity / 255.0f;

        float pb = m_aeroPrimaryBalance;
        float sb = m_aeroSecondaryBalance;
        float bb = m_aeroBlurBalance;

        float al = m_aeroColorA;
        if (!treatAsActive(w)) {
            pb = m_aeroPrimaryBalanceInactive;
            bb = m_aeroBlurBalanceInactive;
            al *= m_transparencyEnabled ? 1.0 : 0.4f;
            basicAlpha *= 0.5f;
        }
        float r = m_aeroColorR;
        float g = m_aeroColorG;
        float b = m_aeroColorB;

        AeroPasses selectedPass = AeroPasses::AERO;

        bool basicCol = m_basicColorization;
        bool useTransparency = m_transparencyEnabled;

        // A window is maximized, use opaque colorization
        if (opaqueMaximize) {
            basicAlpha = 1.0;
            basicCol = true;
            useTransparency = true;
            r = m_aeroColorROpaque;
            g = m_aeroColorGOpaque;
            b = m_aeroColorBOpaque;
        }

        if (basicCol) {
            selectedPass = AeroPasses::BASIC;
        }

        if (!useTransparency) {
            selectedPass = AeroPasses::OPAQUE;
        }

        ShaderManager::instance()->pushShader(m_aeroPasses[selectedPass].shader.get());

        QMatrix4x4 projectionMatrix = viewport.projectionMatrix();
        projectionMatrix.translate(scaledBackgroundRect.x(), scaledBackgroundRect.y());

        GLFramebuffer::popFramebuffer();
        const auto &read = renderInfo.framebuffers[1];


        const QVector2D halfpixel(0.5 / (double)read->colorAttachment()->width(),
                                  0.5 / (double)read->colorAttachment()->height());
        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].mvpMatrixLocation, projectionMatrix);
        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].halfpixelLocation, halfpixel);
        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].offsetLocation, float(m_offset / 2.5f));
        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].colorMatrixLocation, colorMatrix);

        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].aeroColorRLocation, r);
        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].aeroColorGLocation, g);
        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].aeroColorBLocation, b);
        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].aeroColorALocation, al);
        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].aeroColorBalanceLocation,     (basicCol) ? basicAlpha : (pb / 100.0f));
        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].aeroAfterglowBalanceLocation, sb / 100.0f);
        m_aeroPasses[selectedPass].shader->setUniform(m_aeroPasses[selectedPass].aeroBlurBalanceLocation,      bb / 100.0f);

        read->colorAttachment()->bind();

        if (modulation < 1.0) {
            glEnable(GL_BLEND);
            glBlendColor(0, 0, 0, modulation);
            glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
        }

        vbo->draw(GL_TRIANGLES, 6, vertexCount);

        if (modulation < 1.0) {
            glDisable(GL_BLEND);
        }

        ShaderManager::instance()->popShader();
    }

    // Reflection and corner shines
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        float finalOpacity = (float)opacity * (float)m_reflectionIntensity / 100.0f;
        if (opaqueMaximize) {
            finalOpacity *= 0.6f;

            if (!treatAsActive(w)) {
                finalOpacity *= 0.5f;
            }
        }

        QSize screenSize = KWin::effects->virtualScreenSize();
        GLTexture *reflectTex = m_reflectPass.reflectTexture.get();
        GLTexture *glowTex = !treatAsActive(w) ? m_reflectPass.sideGlowTexture_unfocus.get() : m_reflectPass.sideGlowTexture.get();
        bool enableGlow = shouldHaveCornerGlow(w) && m_enableCornerGlow && glowTex && !opaqueMaximize;
        if (reflectTex || enableGlow){
            ShaderManager::instance()->pushShader(m_reflectPass.shader.get());

            QMatrix4x4 projectionMatrix = viewport.projectionMatrix();
            projectionMatrix.translate(scaledBackgroundRect.x(), scaledBackgroundRect.y());
            const auto scale = viewport.scale();

            m_reflectPass.shader->setUniform(m_reflectPass.mvpMatrixLocation, projectionMatrix);
            m_reflectPass.shader->setUniform(m_reflectPass.screenResolutionLocation, QVector2D(screenSize.width() * scale, screenSize.height() * scale));
            m_reflectPass.shader->setUniform(m_reflectPass.windowPosLocation, QVector2D(scaledBackgroundRect.x(), scaledBackgroundRect.y()));
            m_reflectPass.shader->setUniform(m_reflectPass.windowSizeLocation, QVector2D(backgroundRect.width(), backgroundRect.height()));
            m_reflectPass.shader->setUniform(m_reflectPass.opacityLocation, float(finalOpacity));
            m_reflectPass.shader->setUniform(m_reflectPass.translateTextureLocation, m_translateTexture ? float(1.0) : float(0.0));
            m_reflectPass.shader->setUniform(m_reflectPass.colorMatrixLocation, colorMatrix);

            bool useWayland = effects->waylandDisplay() != nullptr; // Determine whether to flip the textures or not
            auto renderTexture = renderTarget.texture();
            if (renderTexture) {
                auto transformKind = renderTarget.texture()->contentTransform().kind();
                useWayland = useWayland && (transformKind != OutputTransform::Kind::Normal);
            }
            m_reflectPass.shader->setUniform(m_reflectPass.useWaylandLocation, useWayland);

            // Glow part
            m_reflectPass.shader->setUniform(m_reflectPass.glowEnableLocation, enableGlow);
            if (enableGlow) {
                m_reflectPass.shader->setUniform(m_reflectPass.glowEnableLocation, enableGlow);
                m_reflectPass.shader->setUniform(m_reflectPass.textureSizeLocation, QVector2D(glowTex->width(), glowTex->height()));
                m_reflectPass.shader->setUniform(m_reflectPass.glowOpacityLocation, float(opacity * 0.8));

                glUniform1i(m_reflectPass.glowTextureLocation, 1);
                glActiveTexture(GL_TEXTURE1);
                glowTex->bind();
            }

            glUniform1i(m_reflectPass.reflectTextureLocation, 0);
            glActiveTexture(GL_TEXTURE0);
            reflectTex->bind();

            vbo->draw(GL_TRIANGLES, 6, vertexCount);

            ShaderManager::instance()->popShader();
        }
        glDisable(GL_BLEND);
    }

    vbo->unbindArrays();
}

bool BlurEffect::shouldOpaqueColorize(const EffectWindow *w) const
{
    auto maximizeState = w->window()->maximizeMode();
    auto maximizedWindowsShareScreen = [&]() -> bool {
        return std::find_if(m_maximizedWindows.begin(), m_maximizedWindows.end(),
                            [&](const EffectWindow *a) { return a->screen() == w->screen(); }) != m_maximizedWindows.end();
    };

    QString windowClass = w->windowClass().split(' ')[1];

    bool opaqueMaximize = false;

    if(m_maximizeColorization) {
        opaqueMaximize = maximizeState == MaximizeMode::MaximizeFull && windowClass != "kwin";

        if(!m_maximizedWindowsInCurrentActivity) opaqueMaximize = false;
        // docks present in the same screen as a maximized window
        // panels, vtp sidebar window, etc
        else if(w->isDock()) opaqueMaximize = maximizedWindowsShareScreen();
        // tabbox
        else if(effects->waylandDisplay() && !w->isWaylandClient() && w->window()->resourceName() == "") opaqueMaximize = false;

        // regular plasmashell windows
        // vistastart, tray dialogs, etc
        if(w->window()->resourceClass() == "org.kde.plasmashell") opaqueMaximize = false;
    }

    if(w->window()->resourceName() == "krunner" && w->window()->resourceClass() == "krunner" && m_opaqueKrunner) opaqueMaximize = true;
    if(w->isOnScreenDisplay() && m_opaqueOSD) opaqueMaximize = true;

    return opaqueMaximize;
}

bool BlurEffect::shouldHaveCornerGlow(const EffectWindow *w) const
{
    QString windowClass = w->windowClass().split(' ')[1];
    if(w->isOnScreenDisplay() || w->isTooltip() || w->isSplash()) return false;
    if(w->caption() == AS_MENUREP || (windowClass != "kwin" && w->isDock())) return false; // Disables panels and start menu
    return true;
}

bool BlurEffect::treatAsActive(const EffectWindow *w)
{
    QString windowClass = w->windowClass().split(' ')[1];
    if (m_basicColorization && (w->isDock() || w->caption() == AS_MENUREP)) return false;
    if(w->caption() == "aeroshell-tabbox" && !w->isManaged()) return true;
    if(effects->waylandDisplay() && !w->isWaylandClient() && w->window()->resourceName() == "") return true;
    return (w->isOnScreenDisplay() || w->isFullScreen() || windowClass == "plasmashell" || windowClass == "kwin" || w == effects->activeWindow());
}

bool BlurEffect::isActive() const
{
    return m_valid && !effects->isScreenLocked();
}

bool BlurEffect::blocksDirectScanout() const
{
    return false;
}

} // namespace KWin

#include "moc_blur.cpp"
