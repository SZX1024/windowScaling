#include "overlay_window.h"

#include <QMouseEvent>
#include <QTouchEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QMatrix4x4>
#include <QDebug>
#include <cmath>
#include <algorithm>

static const char *vshaderSource = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char *fshaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    FragColor = texture(uTexture, TexCoord);
}
)";

OverlayWindow::OverlayWindow(X11CaptureEngine &capture, X11InputForwarder &input, QWindow *parent)
    : QOpenGLWindow(NoPartialUpdate, parent)
    , m_capture(capture)
    , m_input(input)
    , m_vbo(QOpenGLBuffer::VertexBuffer)
{
    setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    connect(&m_renderTimer, &QTimer::timeout, this, &OverlayWindow::updateFrame);
}

OverlayWindow::~OverlayWindow()
{
    makeCurrent();
    m_texture.reset();
    m_program.reset();
    m_vbo.destroy();
    m_vao.destroy();
    doneCurrent();
    stopScaling();
}

bool OverlayWindow::startScaling(unsigned long windowId)
{
    if (!m_capture.setTargetWindow(windowId)) {
        qWarning() << "OverlayWindow: Failed to capture target window" << Qt::hex << windowId;
        return false;
    }

    m_origRect = m_capture.targetRect();
    m_input.setTarget(windowId, m_origRect);

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        setGeometry(screen->geometry());
    }
    showFullScreen();

    calculateLayout();
    m_active = true;

    // 120 FPS continuous render loop
    m_renderTimer.start(8);
    return true;
}

void OverlayWindow::stopScaling()
{
    m_renderTimer.stop();
    m_active = false;
    m_capture.releaseTarget();
    hide();
}

void OverlayWindow::calculateLayout()
{
    if (m_origRect.width() <= 0 || m_origRect.height() <= 0) {
        return;
    }

    const int screenW = width();
    const int screenH = height();

    const qreal scaleX = (qreal)screenW / m_origRect.width();
    const qreal scaleY = (qreal)screenH / m_origRect.height();
    m_scale = std::min(scaleX, scaleY);

    const int scaledW = std::lround(m_origRect.width() * m_scale);
    const int scaledH = std::lround(m_origRect.height() * m_scale);

    const int targetX = (screenW - scaledW) / 2;
    const int targetY = (screenH - scaledH) / 2;

    m_targetRect = QRect(targetX, targetY, scaledW, scaledH);
    updateQuadGeometry();
}

void OverlayWindow::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    m_program = std::make_unique<QOpenGLShaderProgram>();
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vshaderSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fshaderSource);
    m_program->link();

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();

    // 6 vertices (2 triangles): pos (x,y), tex (u,v)
    m_vbo.allocate(sizeof(float) * 24);

    m_program->enableAttributeArray(0);
    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 2, sizeof(float) * 4);

    m_program->enableAttributeArray(1);
    m_program->setAttributeBuffer(1, GL_FLOAT, sizeof(float) * 2, 2, sizeof(float) * 4);

    m_vao.release();
    m_vbo.release();

    updateQuadGeometry();
}

void OverlayWindow::updateQuadGeometry()
{
    if (!m_vbo.isCreated() || m_targetRect.isEmpty()) {
        return;
    }

    const float x1 = m_targetRect.left();
    const float y1 = m_targetRect.top();
    const float x2 = m_targetRect.right() + 1.0f;
    const float y2 = m_targetRect.bottom() + 1.0f;

    // Standard texture coords: (0,0) top-left, (1,1) bottom-right
    const float vertices[] = {
        // Triangle 1
        x1, y1, 0.0f, 0.0f,
        x1, y2, 0.0f, 1.0f,
        x2, y1, 1.0f, 0.0f,
        // Triangle 2
        x2, y1, 1.0f, 0.0f,
        x1, y2, 0.0f, 1.0f,
        x2, y2, 1.0f, 1.0f,
    };

    m_vbo.bind();
    m_vbo.write(0, vertices, sizeof(vertices));
    m_vbo.release();
}

void OverlayWindow::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    calculateLayout();
}

void OverlayWindow::updateFrame()
{
    if (!m_active) return;
    requestUpdate();
}

void OverlayWindow::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_active || !m_program) {
        return;
    }

    // Capture latest frame from X11 Composite/SHM
    QImage frame = m_capture.captureFrame();
    if (frame.isNull()) {
        return;
    }

    // Allocate or update texture
    if (!m_texture || m_texture->width() != frame.width() || m_texture->height() != frame.height()) {
        m_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        m_texture->setSize(frame.width(), frame.height());
        m_texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        m_texture->allocateStorage();
        m_texture->setMinificationFilter(QOpenGLTexture::Linear);
        m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    }

    m_texture->setData(QOpenGLTexture::BGRA, QOpenGLTexture::UInt8, frame.constBits());

    // Setup 2D Orthographic Projection (0 to width, height to 0)
    QMatrix4x4 proj;
    proj.ortho(0.0f, width(), height(), 0.0f, -1.0f, 1.0f);

    m_program->bind();
    m_program->setUniformValue("uProjection", proj);
    m_program->setUniformValue("uTexture", 0);

    m_texture->bind(0);
    m_vao.bind();

    glDrawArrays(GL_TRIANGLES, 0, 6);

    m_vao.release();
    m_texture->release();
    m_program->release();
}

QPoint OverlayWindow::mapToGame(const QPointF &screenPos) const
{
    if (m_targetRect.width() <= 0 || m_targetRect.height() <= 0 || m_scale <= 0.0) {
        return QPoint();
    }

    const qreal relX = screenPos.x() - m_targetRect.x();
    const qreal relY = screenPos.y() - m_targetRect.y();

    const qreal clampedX = std::clamp(relX, 0.0, (qreal)m_targetRect.width());
    const qreal clampedY = std::clamp(relY, 0.0, (qreal)m_targetRect.height());

    const int origX = m_origRect.x() + std::lround(clampedX / m_scale);
    const int origY = m_origRect.y() + std::lround(clampedY / m_scale);

    return QPoint(origX, origY);
}

void OverlayWindow::mousePressEvent(QMouseEvent *event)
{
    if (!m_active) return;
    QPoint gamePos = mapToGame(event->position());
    m_input.sendButton(event->button(), true, gamePos);
}

void OverlayWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_active) return;
    // CRITICAL FIX: Only forward pointer motion when dragging (holding down a mouse button)!
    // If no button is pressed, allow cursor to move smoothly and freely across full screen without warping.
    if (event->buttons() != Qt::NoButton) {
        QPoint gamePos = mapToGame(event->position());
        m_input.sendMotion(gamePos);
    }
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_active) return;
    QPoint gamePos = mapToGame(event->position());
    m_input.sendButton(event->button(), false, gamePos);
}

void OverlayWindow::wheelEvent(QWheelEvent *event)
{
    if (!m_active) return;
    QPoint gamePos = mapToGame(event->position());
    m_input.sendWheel(event->angleDelta().y(), gamePos);
}

bool OverlayWindow::event(QEvent *event)
{
    if (event->type() == QEvent::TouchBegin) {
        auto *te = static_cast<QTouchEvent*>(event);
        if (!te->points().isEmpty()) {
            QPoint gamePos = mapToGame(te->points().first().position());
            m_input.sendTouchDown(gamePos);
            return true;
        }
    } else if (event->type() == QEvent::TouchUpdate) {
        auto *te = static_cast<QTouchEvent*>(event);
        if (!te->points().isEmpty()) {
            QPoint gamePos = mapToGame(te->points().first().position());
            m_input.sendTouchMotion(gamePos);
            return true;
        }
    } else if (event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel) {
        auto *te = static_cast<QTouchEvent*>(event);
        if (!te->points().isEmpty()) {
            QPoint gamePos = mapToGame(te->points().first().position());
            m_input.sendTouchUp(gamePos);
            return true;
        }
    }
    return QOpenGLWindow::event(event);
}

void OverlayWindow::keyPressEvent(QKeyEvent *event)
{
    // Esc or Ctrl+Alt+M closes the overlay immediately
    if (event->key() == Qt::Key_Escape ||
        (event->modifiers().testFlag(Qt::ControlModifier) &&
         event->modifiers().testFlag(Qt::AltModifier) &&
         event->key() == Qt::Key_M)) {
        stopScaling();
        return;
    }
    QOpenGLWindow::keyPressEvent(event);
}
