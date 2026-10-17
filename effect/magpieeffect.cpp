#include "magpieeffect.h"

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QLoggingCategory>

#include <core/output.h>
#include <core/renderviewport.h>
#include <effect/effectwindow.h>
#include <opengl/gltexture.h>
#include <opengl/glshadermanager.h>
#include <opengl/glshader.h>

Q_LOGGING_CATEGORY(KWIN_MAGPIE, "kwin_effect_magpie", QtInfoMsg)

namespace KWin
{

static void sendDesktopNotification(const QString &summary, const QString &body)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify")
    );
    msg << QStringLiteral("Magpie");
    msg << static_cast<uint>(0);
    msg << QStringLiteral("preferences-system-windows-effect-magpie");
    msg << summary;
    msg << body;
    msg << QStringList();
    QVariantMap hints;
    hints[QStringLiteral("transient")] = true;
    msg << hints;
    msg << static_cast<int>(2000);
    QDBusConnection::sessionBus().send(msg);
}

MagpieEffect::MagpieEffect()
    : OffscreenEffect()
{
    m_toggleAction = new QAction(this);
    m_toggleAction->setObjectName(QStringLiteral("ToggleMagpie"));
    m_toggleAction->setText(i18n("Toggle Magpie Window Scaling"));

    KGlobalAccel::setGlobalShortcut(m_toggleAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_M));

    connect(m_toggleAction, &QAction::triggered, this, &MagpieEffect::toggle);

    if (effects) {
        connect(effects, &EffectsHandler::windowDeleted, this, &MagpieEffect::handleWindowDeleted);
        connect(effects, &EffectsHandler::windowClosed, this, &MagpieEffect::slotWindowClosed);
        effects->hideOnScreenMessage();
    }

    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusConnection::sessionBus().registerObject(QStringLiteral("/Magpie"), QStringLiteral("org.kde.KWin.Magpie"), this, QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportAllSlots);
    }
}

MagpieEffect::~MagpieEffect()
{
    deactivate();

    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/Magpie"));
    }
}

bool MagpieEffect::supported()
{
    return true;
}

bool MagpieEffect::isActive() const
{
    return m_active && (m_targetWindow != nullptr);
}

bool MagpieEffect::isMagpieActive() const
{
    return m_active;
}

EffectWindow *MagpieEffect::targetWindow() const
{
    return m_targetWindow;
}

void MagpieEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags);
}

void MagpieEffect::prePaintScreen(ScreenPrePaintData &data)
{
    if (m_active && m_targetWindow) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }
    if (effects) {
        effects->prePaintScreen(data);
    }
}

void MagpieEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    if (m_active && w == m_targetWindow) {
        data.mask |= PAINT_WINDOW_TRANSFORMED;
    }
    if (effects) {
        effects->prePaintWindow(view, w, data);
    }
}

void MagpieEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    if (m_active) {
        if (w != m_targetWindow) {
            data.multiplyOpacity(0.0);
        } else {
            data.setOpacity(1.0);
        }
    }

    if (effects) {
        effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);
    }
}

void MagpieEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    if (effects) {
        effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    }

    if (m_active && m_targetWindow) {
        renderVisualCursor(viewport);
    }
}

void MagpieEffect::postPaintScreen()
{
    if (effects) {
        effects->postPaintScreen();
    }
}

void MagpieEffect::renderVisualCursor(const RenderViewport &viewport)
{
    if (!effects) {
        return;
    }

    PlatformCursorImage cursor = effects->cursorImage();
    if (cursor.isNull()) {
        return;
    }

    const QImage img = cursor.image();
    const QPointF hotSpot = cursor.hotSpot();
    const QPointF drawPos = m_visualCursorPos - hotSpot;

    std::unique_ptr<GLTexture> tex = GLTexture::upload(img);
    if (!tex) {
        return;
    }

    tex->setFilter(GL_LINEAR);
    tex->bind();

    GLShader *shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
    if (shader) {
        QMatrix4x4 mvp = viewport.projectionMatrix();
        mvp.translate(drawPos.x(), drawPos.y());
        shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);
        tex->render(img.size());
        ShaderManager::instance()->popShader();
    }

    tex->unbind();
}

void MagpieEffect::activateOnWindow(EffectWindow *window)
{
    if (!window || window->isSpecialWindow()) {
        return;
    }

    m_targetWindow = window;
    m_active = true;
    redirect(m_targetWindow);

    if (effects) {
        m_visualCursorPos = effects->cursorPos();
        effects->hideCursor();
    }

    m_inputSpy = std::make_unique<MagpieInputSpy>(this);

    QString name = m_targetWindow->caption();
    if (name.isEmpty()) {
        name = QStringLiteral("Window");
    }
    sendDesktopNotification(QStringLiteral("Magpie 已激活"), QStringLiteral("正在等比全屏放大：%1\n按 Ctrl+Alt+M 退出").arg(name));

    if (effects) {
        effects->addRepaintFull();
    }
}

void MagpieEffect::deactivate()
{
    m_inputSpy.reset();

    if (effects && effects->isCursorHidden()) {
        effects->showCursor();
    }

    if (m_targetWindow) {
        unredirect(m_targetWindow);
        m_targetWindow = nullptr;
    }
    if (m_active) {
        m_active = false;
        sendDesktopNotification(QStringLiteral("Magpie 已退出"), QStringLiteral("窗口已恢复原始尺寸"));
    }

    if (effects) {
        effects->addRepaintFull();
    }
}

void MagpieEffect::toggle()
{
    if (!m_active) {
        if (!effects) {
            return;
        }

        EffectWindow *window = effects->activeWindow();
        if (!window) {
            sendDesktopNotification(QStringLiteral("Magpie 待命"), QStringLiteral("请点击需要放大的目标窗口"));
            effects->startInteractiveWindowSelection([this](EffectWindow *w) {
                if (w) {
                    activateOnWindow(w);
                }
            });
            return;
        }

        activateOnWindow(window);
    } else {
        deactivate();
    }
}

void MagpieEffect::handleWindowDeleted(EffectWindow *w)
{
    if (w == m_targetWindow) {
        deactivate();
    }
}

void MagpieEffect::slotWindowClosed(EffectWindow *w)
{
    if (w == m_targetWindow) {
        deactivate();
    }
}

QPointF MagpieEffect::mapScaledToOriginal(const QPointF &pos) const
{
    if (!m_targetWindow) {
        return pos;
    }

    LogicalOutput *output = m_targetWindow->screen();
    if (!output && effects) {
        output = effects->activeScreen();
    }
    if (!output && effects && !effects->screens().isEmpty()) {
        output = effects->screens().first();
    }
    if (!output) {
        return pos;
    }

    const RectF outputRect = output->geometryF();
    const RectF winGeom = m_targetWindow->frameGeometry();

    const qreal winW = winGeom.width();
    const qreal winH = winGeom.height();

    if (winW <= 0.0 || winH <= 0.0 || outputRect.width() <= 0.0 || outputRect.height() <= 0.0) {
        return pos;
    }

    const qreal scale = std::min(outputRect.width() / winW, outputRect.height() / winH);
    const qreal scaledW = winW * scale;
    const qreal scaledH = winH * scale;

    const qreal targetX = outputRect.x() + (outputRect.width() - scaledW) / 2.0;
    const qreal targetY = outputRect.y() + (outputRect.height() - scaledH) / 2.0;

    const qreal relX = pos.x() - targetX;
    const qreal relY = pos.y() - targetY;

    const qreal clampedRelX = std::clamp(relX, 0.0, scaledW);
    const qreal clampedRelY = std::clamp(relY, 0.0, scaledH);

    const qreal origX = winGeom.x() + clampedRelX / scale;
    const qreal origY = winGeom.y() + clampedRelY / scale;

    return QPointF(origX, origY);
}

void MagpieEffect::handlePointerEvent(PointerMotionEvent *event)
{
    if (!m_active || !m_targetWindow) {
        return;
    }

    m_visualCursorPos = event->position;

    // Map screen cursor position to game window coordinate and warp physical pointer
    const QPointF origPos = mapScaledToOriginal(m_visualCursorPos);
    if (input()) {
        input()->warpPointer(origPos);
    }

    if (effects) {
        effects->addRepaintFull();
    }
}

void MagpieEffect::apply(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads)
{
    if (!m_active || window != m_targetWindow || !m_targetWindow) {
        OffscreenEffect::apply(window, mask, data, quads);
        return;
    }

    LogicalOutput *output = window ? window->screen() : nullptr;
    if (!output && effects) {
        output = effects->activeScreen();
    }
    if (!output && effects && !effects->screens().isEmpty()) {
        output = effects->screens().first();
    }
    if (!output) {
        OffscreenEffect::apply(window, mask, data, quads);
        return;
    }

    const RectF outputRect = output->geometryF();
    const RectF winGeom = window->frameGeometry();

    const qreal winW = winGeom.width();
    const qreal winH = winGeom.height();

    if (winW <= 0.0 || winH <= 0.0 || outputRect.width() <= 0.0 || outputRect.height() <= 0.0) {
        OffscreenEffect::apply(window, mask, data, quads);
        return;
    }

    // Aspect-preserving scale
    const qreal scale = std::min(outputRect.width() / winW, outputRect.height() / winH);

    // Exact Center alignment:
    const QPointF screenCenter(outputRect.x() + outputRect.width() / 2.0,
                               outputRect.y() + outputRect.height() / 2.0);
    const QPointF windowPos = window->pos();
    const QPointF scaledHalfSize(winW * scale / 2.0, winH * scale / 2.0);

    const QPointF translation = screenCenter - windowPos - scaledHalfSize;

    data.setXScale(scale);
    data.setYScale(scale);
    data.setXTranslation(translation.x());
    data.setYTranslation(translation.y());

    OffscreenEffect::apply(window, mask, data, quads);
}

class MagpieEffectFactory : public EffectPluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EffectPluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KPluginFactory)

public:
    MagpieEffectFactory() = default;
    ~MagpieEffectFactory() override = default;

    bool isSupported() const override
    {
        return true;
    }

    bool enabledByDefault() const override
    {
        return true;
    }

    Effect *createEffect() const override
    {
        return new MagpieEffect();
    }
};

} // namespace KWin

#include "magpieeffect.moc"