#pragma once

#include <effect/offscreeneffect.h>
#include <effect/effecthandler.h>
#include <input_event.h>
#include <wayland_server.h>
#include <wayland/seat.h>
#include <wayland/surface.h>
#include <memory>

class QAction;

namespace KWin
{

class EffectWindow;

class MagpieEffect : public OffscreenEffect
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.Magpie")

public:
    MagpieEffect();
    ~MagpieEffect() override;

    static bool supported();

    bool isActive() const override;

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data) override;
    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data) override;

    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;
    void postPaintScreen() override;

    EffectWindow *targetWindow() const;
    QPointF mapScaledToSurface(const QPointF &pos) const;

    // First-class input interception (Option 3: Fullscreen Input Overlay)
    void pointerMotion(PointerMotionEvent *event) override;
    void pointerButton(PointerButtonEvent *event) override;
    void pointerAxis(PointerAxisEvent *event) override;

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;
    void touchCancel() override;

public Q_SLOTS:
    Q_SCRIPTABLE void toggle();
    Q_SCRIPTABLE bool isMagpieActive() const;
    void handleWindowDeleted(KWin::EffectWindow *w);
    void slotWindowClosed(KWin::EffectWindow *w);

protected:
    void apply(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads) override;

private:
    void activateOnWindow(EffectWindow *w);
    void deactivate();
    void renderVisualCursor(const RenderViewport &viewport);

    bool m_active{false};
    EffectWindow *m_targetWindow{nullptr};
    QAction *m_toggleAction{nullptr};
    QPointF m_visualCursorPos;
};

} // namespace KWin