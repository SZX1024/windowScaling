#pragma once

#include <effect/offscreeneffect.h>
#include <effect/effecthandler.h>
#include <input_event_spy.h>
#include <input_event.h>
#include <input.h>
#include <memory>

class QAction;

namespace KWin
{

class EffectWindow;
class MagpieInputSpy;

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
    QPointF mapScaledToOriginal(const QPointF &pos) const;
    void handlePointerEvent(PointerMotionEvent *event);

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
    std::unique_ptr<MagpieInputSpy> m_inputSpy;

    friend class MagpieInputSpy;
};

class MagpieInputSpy : public InputEventSpy
{
public:
    explicit MagpieInputSpy(MagpieEffect *effect)
        : m_effect(effect)
    {
        if (input()) {
            input()->installInputEventSpy(this);
        }
    }

    ~MagpieInputSpy() override
    {
        if (input()) {
            input()->uninstallInputEventSpy(this);
        }
    }

    void pointerMotion(PointerMotionEvent *event) override
    {
        if (event->warp) {
            return;
        }
        if (m_effect && m_effect->isActive()) {
            m_effect->handlePointerEvent(event);
        }
    }

    void pointerButton(PointerButtonEvent *event) override
    {
        if (m_effect && m_effect->isActive()) {
            // Map button click strictly from visual cursor position (single source of truth)
            event->position = m_effect->mapScaledToOriginal(m_effect->m_visualCursorPos);
        }
    }

    void pointerAxis(PointerAxisEvent *event) override
    {
        if (m_effect && m_effect->isActive()) {
            event->position = m_effect->mapScaledToOriginal(m_effect->m_visualCursorPos);
        }
    }

    void touchDown(TouchDownEvent *event) override
    {
        if (m_effect && m_effect->isActive()) {
            event->pos = m_effect->mapScaledToOriginal(event->pos);
        }
    }

    void touchMotion(TouchMotionEvent *event) override
    {
        if (m_effect && m_effect->isActive()) {
            event->pos = m_effect->mapScaledToOriginal(event->pos);
        }
    }

    void tabletToolTipEvent(TabletToolTipEvent *event) override
    {
        if (m_effect && m_effect->isActive()) {
            event->position = m_effect->mapScaledToOriginal(event->position);
        }
    }

    void tabletToolAxisEvent(TabletToolAxisEvent *event) override
    {
        if (m_effect && m_effect->isActive()) {
            event->position = m_effect->mapScaledToOriginal(event->position);
        }
    }

private:
    MagpieEffect *m_effect{nullptr};
};

} // namespace KWin
