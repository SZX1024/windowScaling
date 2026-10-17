#include "magpieeffect.h"

#include <QAction>
#include <QDBusConnection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QLoggingCategory>

#include <core/output.h>
#include <effect/effectwindow.h>

Q_LOGGING_CATEGORY(KWIN_MAGPIE, "kwin_effect_magpie", QtInfoMsg)

namespace KWin
{

MagpieEffect::MagpieEffect()
    : OffscreenEffect()
{
    m_toggleAction = new QAction(this);
    m_toggleAction->setObjectName(QStringLiteral("ToggleMagpie"));
    m_toggleAction->setText(i18n("Toggle Magpie Window Scaling"));

    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, {QKeySequence(Qt::META | Qt::ALT | Qt::Key_M)});
    KGlobalAccel::self()->setShortcut(m_toggleAction, {QKeySequence(Qt::META | Qt::ALT | Qt::Key_M)});

    connect(m_toggleAction, &QAction::triggered, this, &MagpieEffect::toggle);

    if (effects) {
        connect(effects, &EffectsHandler::windowDeleted, this, &MagpieEffect::handleWindowDeleted);
        connect(effects, &EffectsHandler::windowClosed, this, &MagpieEffect::slotWindowClosed);
    }

    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusConnection::sessionBus().registerObject(QStringLiteral("/Magpie"), QStringLiteral("org.kde.KWin.Magpie"), this, QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportAllSlots);
    }

    qCWarning(KWIN_MAGPIE) << "Magpie effect initialized successfully.";
}

MagpieEffect::~MagpieEffect()
{
    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/Magpie"));
    }

    if (m_active && m_targetWindow) {
        unredirect(m_targetWindow);
        m_targetWindow = nullptr;
        m_active = false;
    }

    qCWarning(KWIN_MAGPIE) << "Magpie effect unloaded.";
}

bool MagpieEffect::supported()
{
    return effects && OffscreenEffect::supported();
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

void MagpieEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    if (effects) {
        effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    }
}

void MagpieEffect::postPaintScreen()
{
    if (effects) {
        effects->postPaintScreen();
    }
}

void MagpieEffect::toggle()
{
    qCWarning(KWIN_MAGPIE) << "Magpie toggle invoked. Current state active:" << m_active;

    if (!m_active) {
        if (!effects) {
            qCWarning(KWIN_MAGPIE) << "Cannot toggle Magpie: effects handler is null";
            return;
        }

        EffectWindow *window = effects->activeWindow();
        if (!window) {
            qCWarning(KWIN_MAGPIE) << "Cannot activate Magpie: no active window currently focused";
            return;
        }

        if (!window->isNormalWindow() && !window->isDialog()) {
            qCWarning(KWIN_MAGPIE) << "Cannot activate Magpie: window is not normal/dialog. Type:" << window->windowType();
            return;
        }

        m_targetWindow = window;
        m_active = true;
        redirect(m_targetWindow);

        qCWarning(KWIN_MAGPIE) << "Magpie ACTIVATED for window:" << m_targetWindow << "geometry:" << m_targetWindow->frameGeometry();
        effects->addRepaintFull();
    } else {
        if (m_targetWindow) {
            unredirect(m_targetWindow);
            m_targetWindow = nullptr;
        }
        m_active = false;

        qCWarning(KWIN_MAGPIE) << "Magpie DEACTIVATED";
        if (effects) {
            effects->addRepaintFull();
        }
    }
}

void MagpieEffect::handleWindowDeleted(EffectWindow *w)
{
    if (w == m_targetWindow) {
        qCWarning(KWIN_MAGPIE) << "Target window deleted, resetting Magpie state";
        m_targetWindow = nullptr;
        m_active = false;
        if (effects) {
            effects->addRepaintFull();
        }
    }
}

void MagpieEffect::slotWindowClosed(EffectWindow *w)
{
    if (w == m_targetWindow) {
        qCWarning(KWIN_MAGPIE) << "Target window closed, unredirecting and resetting Magpie state";
        if (m_active && m_targetWindow) {
            unredirect(m_targetWindow);
        }
        m_targetWindow = nullptr;
        m_active = false;
        if (effects) {
            effects->addRepaintFull();
        }
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
    const qreal winX = winGeom.x();
    const qreal winY = winGeom.y();

    const qreal outW = outputRect.width();
    const qreal outH = outputRect.height();
    const qreal outX = outputRect.x();
    const qreal outY = outputRect.y();

    if (winW <= 0.0 || winH <= 0.0 || outW <= 0.0 || outH <= 0.0) {
        OffscreenEffect::apply(window, mask, data, quads);
        return;
    }

    const qreal scale = std::min(outW / winW, outH / winH);
    const qreal scaledW = winW * scale;
    const qreal scaledH = winH * scale;

    const qreal targetX = outX + (outW - scaledW) / 2.0;
    const qreal targetY = outY + (outH - scaledH) / 2.0;

    const qreal tx = targetX - winX * scale;
    const qreal ty = targetY - winY * scale;

    data.setXScale(scale);
    data.setYScale(scale);
    data.setXTranslation(tx);
    data.setYTranslation(ty);

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
        return MagpieEffect::supported();
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
