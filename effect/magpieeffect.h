#pragma once

#include <effect/offscreeneffect.h>
#include <effect/effecthandler.h>

class QAction;

namespace KWin
{

class EffectWindow;

class MagpieEffect : public OffscreenEffect
{
    Q_OBJECT

public:
    MagpieEffect();
    ~MagpieEffect() override;

    static bool supported();

    bool isActive() const override;

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data) override;
    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data) override;

    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;
    void postPaintScreen() override;

    EffectWindow *targetWindow() const;

public Q_SLOTS:
    void toggle();
    void handleWindowDeleted(KWin::EffectWindow *w);
    void slotWindowClosed(KWin::EffectWindow *w);

protected:
    void apply(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads) override;

private:
    bool m_active{false};
    EffectWindow *m_targetWindow{nullptr};
    QAction *m_toggleAction{nullptr};
};

} // namespace KWin
