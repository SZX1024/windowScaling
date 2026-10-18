#pragma once

#include <QOpenGLWindow>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QTimer>
#include <QImage>
#include <QRect>
#include <QElapsedTimer>
#include <QDateTime>
#include "capture.h"
#include "input.h"
#include "shader_pipeline.h"

class OverlayWindow : public QOpenGLWindow, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit OverlayWindow(X11CaptureEngine &capture, X11InputForwarder &input, QWindow *parent = nullptr);
    ~OverlayWindow() override;

    bool startScaling(unsigned long windowId, bool windowed = false);
    void stopScaling();
    void takeScreenshot();

    ShaderPipeline *shaderPipeline() { return m_shaders.get(); }

    void setCropMargins(const QMargins &margins);
    QMargins cropMargins() const { return m_cropMargins; }

    void setWindowScale(float scale);
    float windowScale() const { return m_windowScale; }

    void setToolbarPinned(bool pinned) { m_toolbarPinned = pinned; }
    bool isToolbarPinned() const { return m_toolbarPinned; }

    void setShowProfiler(bool show) { m_showProfiler = show; }
    bool isShowProfiler() const { return m_showProfiler; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // Mouse input
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    // Touchscreen input
    bool event(QEvent *event) override;

    // Keyboard (Esc or Ctrl+Alt+M to exit, F12 for screenshot)
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateFrame();
    void hideToolbar();

private:
    QSize physicalSize() const;
    QPoint mapToGame(const QPointF &screenPos) const;
    void calculateLayout();
    void updateFpsStats();
    void drawToolbar(QPainter &painter);
    void drawProfiler(QPainter &painter);
    int hitTestToolbar(const QPoint &pos) const;

    X11CaptureEngine &m_capture;
    X11InputForwarder &m_input;
    QTimer m_renderTimer;

    std::unique_ptr<ShaderPipeline> m_shaders;
    std::unique_ptr<QOpenGLTexture> m_texture;

    QRect m_origRect;
    QRect m_targetRect;
    QMargins m_cropMargins{0, 0, 0, 0};
    bool m_active{false};
    bool m_isWindowed{false};
    float m_windowScale{1.5f};
    unsigned long m_currentWindowId{0};
    QString m_windowTitle;

    // Toolbar state
    bool m_toolbarVisible{false};
    bool m_toolbarPinned{false};
    QRect m_toolbarRect;
    QRect m_btnPinRect;
    QRect m_btnProfilerRect;
    QRect m_btnShotRect;
    QRect m_btnPresetRect;
    QRect m_btnModeRect;
    QRect m_btnCloseRect;
    int m_hoveredBtn{-1};
    QString m_hoverTooltip;
    QTimer m_toolbarHideTimer;

    // Profiler metrics
    bool m_showProfiler{false};
    QString m_gpuRenderer;
    int m_fps{0};
    int m_frameCounter{0};
    QElapsedTimer m_fpsTimer;
    float m_frameTimeMs{0.0f};
    QElapsedTimer m_frameTimer;
};
