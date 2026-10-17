#pragma once

#include <effect/effect.h>
#include <effect/effecthandler.h>

class QAction;

namespace KWin
{

class MagpieEffect : public Effect
{
    Q_OBJECT

public:
    MagpieEffect();
    ~MagpieEffect() override;

    static bool supported();

    bool isActive() const override;

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;
    void postPaintScreen() override;

public Q_SLOTS:
    void toggle();

private:
    bool m_active{false};
    QAction *m_toggleAction{nullptr};
};

} // namespace KWin
