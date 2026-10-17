#pragma once

#include <QOpenGLWindow>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QTimer>
#include <QImage>
#include <QRect>
#include "capture.h"
#include "input.h"

class OverlayWindow : public QOpenGLWindow, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit OverlayWindow(X11CaptureEngine &capture, X11InputForwarder &input, QWindow *parent = nullptr);
    ~OverlayWindow() override;

    bool startScaling(unsigned long windowId);
    void stopScaling();

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
    void updateQuadGeometry();

    X11CaptureEngine &m_capture;
    X11InputForwarder &m_input;
    QTimer m_renderTimer;

    std::unique_ptr<QOpenGLShaderProgram> m_program;
    std::unique_ptr<QOpenGLTexture> m_texture;
    QOpenGLBuffer m_vbo;
    QOpenGLVertexArrayObject m_vao;

    QRect m_origRect;
    QRect m_targetRect;
    qreal m_scale{1.0};
    bool m_active{false};
    bool m_needsTextureUpload{false};
};
