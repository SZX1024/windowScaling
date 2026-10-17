#include "overlay_window.h"

#include <QMouseEvent>
#include <QTouchEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QDBusMessage>
#include <QDBusConnection>

static void sendNotification(const QString &summary, const QString &body)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify")
    );
    msg << QStringLiteral("Magpie");
    msg << static_cast<uint>(0);
    msg << QStringLiteral("preferences-system-windows");
    msg << summary;
    msg << body;
    msg << QStringList();
    QVariantMap hints;
    hints[QStringLiteral("transient")] = true;
    msg << hints;
    msg << static_cast<int>(1500);
    QDBusConnection::sessionBus().send(msg);
}

OverlayWindow::OverlayWindow(X11CaptureEngine &capture, X11InputForwarder &input, QWindow *parent)
    : QOpenGLWindow(NoPartialUpdate, parent)
    , m_capture(capture)
    , m_input(input)
    , m_shaders(std::make_unique<ShaderPipeline>())
{
    setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus | Qt::BypassWindowManagerHint);
    connect(&m_renderTimer, &QTimer::timeout, this, &OverlayWindow::updateFrame);
}

OverlayWindow::~OverlayWindow()
{
    makeCurrent();
    m_texture.reset();
    m_shaders.reset();
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
    m_input.restackOverlayAbove(this->winId());
    this->raise();

    // 120 FPS render loop
    m_renderTimer.start(8);
    return true;
}

void OverlayWindow::stopScaling()
{
    m_renderTimer.stop();
    m_active = false;
    m_input.releaseTarget();
    m_capture.releaseTarget();
    hide();
    close();
}

void OverlayWindow::calculateLayout()
{
    if (m_origRect.width() <= 0 || m_origRect.height() <= 0 || !m_shaders) {
        return;
    }

    m_targetRect = m_shaders->calculateTargetRect(m_origRect.size(), size());
}

void OverlayWindow::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    if (!m_shaders) m_shaders = std::make_unique<ShaderPipeline>();
    m_shaders->init();

    calculateLayout();
}

void OverlayWindow::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    calculateLayout();
}

void OverlayWindow::updateFrame()
{
    if (!m_active) return;

    if (!m_capture.isTargetAlive()) {
        stopScaling();
        return;
    }

    requestUpdate();
}

void OverlayWindow::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_active || !m_shaders) {
        return;
    }

    QImage frame = m_capture.captureFrame();
    if (frame.isNull()) {
        return;
    }

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

    // GPU Hardware Accelerated Shader Rendering directly onto full-screen targetRect
    m_shaders->render(m_texture->textureId(), frame.size(), m_targetRect, size());
}

QPoint OverlayWindow::mapToGame(const QPointF &screenPos) const
{
    if (m_targetRect.width() <= 0 || m_targetRect.height() <= 0 || m_origRect.width() <= 0) {
        return QPoint();
    }

    const qreal scale = (qreal)m_targetRect.width() / m_origRect.width();
    const qreal relX = screenPos.x() - m_targetRect.x();
    const qreal relY = screenPos.y() - m_targetRect.y();

    const qreal clampedX = std::clamp(relX, 0.0, (qreal)m_targetRect.width());
    const qreal clampedY = std::clamp(relY, 0.0, (qreal)m_targetRect.height());

    const int localX = (int)std::clamp(std::lround(clampedX / scale), 0L, (long)m_origRect.width() - 1);
    const int localY = (int)std::clamp(std::lround(clampedY / scale), 0L, (long)m_origRect.height() - 1);

    return QPoint(localX, localY);
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
    m_input.restackOverlayAbove(this->winId());
    this->raise();
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
            m_input.restackOverlayAbove(this->winId());
            this->raise();
            return true;
        }
    }
    return QOpenGLWindow::event(event);
}

void OverlayWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape ||
        (event->modifiers().testFlag(Qt::ControlModifier) &&
         event->modifiers().testFlag(Qt::AltModifier) &&
         event->key() == Qt::Key_M)) {
        stopScaling();
        return;
    }

    if (m_shaders) {
        if (event->key() == Qt::Key_1) {
            m_shaders->setScaler(ScalerType::Bilinear);
            sendNotification(QStringLiteral("超分算法切换"), QString::fromUtf8(m_shaders->currentScalerName()));
            requestUpdate();
            return;
        } else if (event->key() == Qt::Key_2) {
            m_shaders->setScaler(ScalerType::Lanczos);
            sendNotification(QStringLiteral("超分算法切换"), QString::fromUtf8(m_shaders->currentScalerName()));
            requestUpdate();
            return;
        } else if (event->key() == Qt::Key_3) {
            m_shaders->setScaler(ScalerType::CAS);
            sendNotification(QStringLiteral("超分算法切换"), QString::fromUtf8(m_shaders->currentScalerName()));
            requestUpdate();
            return;
        } else if (event->key() == Qt::Key_4) {
            m_shaders->setScaler(ScalerType::Anime4K);
            sendNotification(QStringLiteral("超分算法切换"), QString::fromUtf8(m_shaders->currentScalerName()));
            requestUpdate();
            return;
        }
    }

    QOpenGLWindow::keyPressEvent(event);
}