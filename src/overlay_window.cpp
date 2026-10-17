#include "overlay_window.h"

#include <QMouseEvent>
#include <QTouchEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QDebug>
#include <cmath>
#include <algorithm>

OverlayWindow::OverlayWindow(X11CaptureEngine &capture, X11InputForwarder &input, QWindow *parent)
    : QOpenGLWindow(NoPartialUpdate, parent)
    , m_capture(capture)
    , m_input(input)
{
    setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    connect(&m_renderTimer, &QTimer::timeout, this, &OverlayWindow::updateFrame);
}

OverlayWindow::~OverlayWindow()
{
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

    // Show fullscreen on current screen
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        setGeometry(screen->geometry());
    }
    showFullScreen();

    calculateLayout();
    m_active = true;

    // 60-120 FPS render loop
    m_renderTimer.start(8); // ~120 Hz update check
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
}

void OverlayWindow::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void OverlayWindow::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);
    calculateLayout();
}

void OverlayWindow::updateFrame()
{
    if (!m_active) return;

    QImage frame = m_capture.captureFrame();
    if (!frame.isNull()) {
        m_currentFrame = frame;
        requestUpdate();
    }
}

void OverlayWindow::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_currentFrame.isNull()) {
        return;
    }

    // Paint using QPainter on OpenGL window
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Draw the scaled game frame centered with black borders
    painter.drawImage(m_targetRect, m_currentFrame);
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
    QPoint gamePos = mapToGame(event->position());
    m_input.sendMotion(gamePos);
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
    // Esc or Ctrl+Alt+M quits the overlay and restores normal view
    if (event->key() == Qt::Key_Escape ||
        (event->modifiers().testFlag(Qt::ControlModifier) &&
         event->modifiers().testFlag(Qt::AltModifier) &&
         event->key() == Qt::Key_M)) {
        stopScaling();
        return;
    }
    QOpenGLWindow::keyPressEvent(event);
}
