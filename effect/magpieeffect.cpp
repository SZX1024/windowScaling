#include "magpieeffect.h"

#include <QAction>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(KWIN_MAGPIE, "kwin_effect_magpie", QtWarningMsg)

namespace KWin
{

MagpieEffect::MagpieEffect()
    : Effect()
{
    m_toggleAction = new QAction(this);
    m_toggleAction->setObjectName(QStringLiteral("ToggleMagpie"));
    m_toggleAction->setText(i18n("Toggle Magpie Window Scaling"));

    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, {QKeySequence(Qt::META | Qt::ALT | Qt::Key_M)});
    KGlobalAccel::self()->setShortcut(m_toggleAction, {QKeySequence(Qt::META | Qt::ALT | Qt::Key_M)});

    connect(m_toggleAction, &QAction::triggered, this, &MagpieEffect::toggle);

    qCInfo(KWIN_MAGPIE) << "Magpie effect initialized successfully.";
}

MagpieEffect::~MagpieEffect()
{
    qCInfo(KWIN_MAGPIE) << "Magpie effect unloaded.";
}

bool MagpieEffect::supported()
{
    return effects && effects->isOpenGLCompositing();
}

bool MagpieEffect::isActive() const
{
    return m_active;
}

void MagpieEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags);
}

void MagpieEffect::prePaintScreen(ScreenPrePaintData &data)
{
    effects->prePaintScreen(data);
}

void MagpieEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
}

void MagpieEffect::postPaintScreen()
{
    effects->postPaintScreen();
}

void MagpieEffect::toggle()
{
    m_active = !m_active;
    qCInfo(KWIN_MAGPIE) << "Magpie effect toggled. Active:" << m_active;
    effects->addRepaintFull();
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
        return false;
    }

    Effect *createEffect() const override
    {
        return new MagpieEffect();
    }
};

} // namespace KWin

#include "magpieeffect.moc"
