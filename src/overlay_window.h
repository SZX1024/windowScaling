#pragma once

#include <QOpenGLWindow>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QTimer>
#include <QImage>
#include <QRect>
#include "capture.h"
#include "input.h"
#include "shader_pipeline.h"

class OverlayWindow : public QOpenGLWindow, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit OverlayWindow(X11CaptureEngine &capture, X11InputForwarder &input, QWindow *parent = nullptr);
    ~OverlayWindow() override;

    bool startScaling(unsigned long windowId);
    void stopScaling();

    ShaderPipeline *shaderPipeline() { return m_shaders.get(); }

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

    // Keyboard (Esc or Ctrl+Alt+M to exit)
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateFrame();

private:
    QPoint mapToGame(const QPointF &screenPos) const;
    void calculateLayout();

    X11CaptureEngine &m_capture;
    X11InputForwarder &m_input;
    QTimer m_renderTimer;

    std::unique_ptr<ShaderPipeline> m_shaders;
    std::unique_ptr<QOpenGLTexture> m_texture;

    QRect m_origRect;
    QRect m_targetRect;
    bool m_active{false};
};
