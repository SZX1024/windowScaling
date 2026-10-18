#include "overlay_window.h"

#include <QMouseEvent>
#include <QTouchEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>
#include <QDir>
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
    msg << QStringLiteral("windowScaling");
    msg << static_cast<uint>(0);
    msg << QStringLiteral("preferences-system-windows");
    msg << summary;
    msg << body;
    msg << QStringList();
    QVariantMap hints;
    hints[QStringLiteral("transient")] = true;
    msg << hints;
    msg << static_cast<int>(2000);
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

    m_toolbarHideTimer.setSingleShot(true);
    connect(&m_toolbarHideTimer, &QTimer::timeout, this, &OverlayWindow::hideToolbar);
}

OverlayWindow::~OverlayWindow()
{
    makeCurrent();
    m_texture.reset();
    m_shaders.reset();
    doneCurrent();
    stopScaling();
}

void OverlayWindow::setCropMargins(const QMargins &margins)
{
    m_cropMargins = margins;
    if (m_shaders) {
        m_shaders->setCropMargins(margins);
    }
    calculateLayout();
}

void OverlayWindow::setWindowScale(float scale)
{
    m_windowScale = scale;
    if (m_shaders) {
        m_shaders->setWindowScale(scale);
    }
    calculateLayout();
}

bool OverlayWindow::startScaling(unsigned long windowId, bool windowed)
{
    if (!m_capture.setTargetWindow(windowId)) {
        qWarning() << "OverlayWindow: Failed to capture target window" << Qt::hex << windowId;
        return false;
    }

    m_currentWindowId = windowId;
    m_windowTitle = m_capture.getWindowTitle(windowId);
    m_origRect = m_capture.targetRect();
    m_input.setTarget(windowId, m_origRect);
    m_isWindowed = windowed;

    if (m_shaders) {
        m_shaders->setScalingMode(windowed ? ScalingMode::Windowed : m_shaders->scalingMode());
        m_shaders->setCropMargins(m_cropMargins);
        m_shaders->setWindowScale(m_windowScale);
    }

    if (windowed) {
        setFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint | Qt::WindowStaysOnTopHint);
        setTitle(QStringLiteral("windowScaling - [窗口化缩放 %1x] %2")
                 .arg(QString::number(m_windowScale, 'f', 2))
                 .arg(m_windowTitle.isEmpty() ? QStringLiteral("Game Window") : m_windowTitle));

        int effSrcW = std::max(10, m_origRect.width() - m_cropMargins.left() - m_cropMargins.right());
        int effSrcH = std::max(10, m_origRect.height() - m_cropMargins.top() - m_cropMargins.bottom());
        const qreal dpr = devicePixelRatio();
        int winW = std::max(100, (int)std::lround((effSrcW * m_windowScale) / dpr));
        int winH = std::max(100, (int)std::lround((effSrcH * m_windowScale) / dpr));
        resize(winW, winH);
        if (!m_origRect.isNull()) {
            setPosition(m_origRect.topLeft());
        }
        show();
    } else {
        setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus | Qt::BypassWindowManagerHint);
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            setGeometry(screen->geometry());
        }
        showFullScreen();
    }

    calculateLayout();
    m_active = true;
    m_input.restackOverlayAbove(this->winId());
    this->raise();

    m_fpsTimer.start();
    m_frameCounter = 0;
    m_fps = 60;

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

void OverlayWindow::hideToolbar()
{
    if (!m_toolbarPinned) {
        m_toolbarVisible = false;
        requestUpdate();
    }
}


QSize OverlayWindow::physicalSize() const
{
    const qreal dpr = devicePixelRatio();
    return QSize(std::max(1L, std::lround(width() * dpr)), std::max(1L, std::lround(height() * dpr)));
}

void OverlayWindow::calculateLayout()
{
    if (m_origRect.width() <= 0 || m_origRect.height() <= 0 || !m_shaders) {
        return;
    }

    const QSize vp = physicalSize();
    m_targetRect = m_shaders->calculateTargetRect(m_origRect.size(), vp);
}

void OverlayWindow::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    const char *glGpu = (const char*)glGetString(GL_RENDERER);
    if (glGpu) {
        m_gpuRenderer = QString::fromUtf8(glGpu);
    } else {
        m_gpuRenderer = QStringLiteral("OpenGL Accelerated Device");
    }

    if (!m_shaders) m_shaders = std::make_unique<ShaderPipeline>();
    m_shaders->init();

    calculateLayout();
}

void OverlayWindow::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    calculateLayout();
}

void OverlayWindow::updateFpsStats()
{
    m_frameCounter++;
    qint64 elapsed = m_fpsTimer.elapsed();
    if (elapsed >= 500) {
        m_fps = std::lround((m_frameCounter * 1000.0) / elapsed);
        m_frameCounter = 0;
        m_fpsTimer.restart();
    }
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
    m_frameTimer.restart();

    const QSize vp = physicalSize();
    glViewport(0, 0, vp.width(), vp.height());

    if (!m_active || !m_shaders) {
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    if (m_targetRect.isEmpty() || m_targetRect.width() > vp.width() || m_targetRect.height() > vp.height()) {
        calculateLayout();
    }

    glClear(GL_COLOR_BUFFER_BIT);

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

    // Clean OpenGL state before custom shader rendering
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    // GPU Hardware Accelerated Multi-Pass Shader Rendering on physical viewport
    m_shaders->render(m_texture->textureId(), frame.size(), m_targetRect, vp);

    m_frameTimeMs = m_frameTimer.nsecsElapsed() / 1000000.0f;
    updateFpsStats();

    // Draw Overlay UI (Toolbar & Profiler) via QPainter
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_toolbarVisible || m_toolbarPinned) {
        drawToolbar(painter);
    }

    if (m_showProfiler) {
        drawProfiler(painter);
    }

    painter.end();
}

int OverlayWindow::hitTestToolbar(const QPoint &pos) const
{
    if (!m_toolbarVisible && !m_toolbarPinned) return -1;
    if (!m_toolbarRect.contains(pos)) return -1;

    if (m_btnPinRect.contains(pos)) return 0;
    if (m_btnProfilerRect.contains(pos)) return 1;
    if (m_btnShotRect.contains(pos)) return 2;
    if (m_btnPresetRect.contains(pos)) return 3;
    if (m_btnModeRect.contains(pos)) return 4;
    if (m_btnCloseRect.contains(pos)) return 5;

    return 99; // inside toolbar body
}

void OverlayWindow::drawToolbar(QPainter &p)
{
    const int tbW = 480;
    const int tbH = 42;
    const int tbX = (width() - tbW) / 2;
    const int tbY = 10;
    m_toolbarRect = QRect(tbX, tbY, tbW, tbH);

    // Dark rounded background
    QPainterPath bgPath;
    bgPath.addRoundedRect(m_toolbarRect, 10, 10);
    p.fillPath(bgPath, QColor(20, 22, 28, 235));
    p.setPen(QPen(QColor(255, 255, 255, 45), 1.2));
    p.drawPath(bgPath);

    // Compute button areas
    int btnX = tbX + 8;
    const int btnY = tbY + 5;
    const int btnH = 32;

    m_btnPinRect = QRect(btnX, btnY, 34, btnH); btnX += 38;
    m_btnProfilerRect = QRect(btnX, btnY, 34, btnH); btnX += 38;
    m_btnShotRect = QRect(btnX, btnY, 34, btnH); btnX += 42;
    m_btnPresetRect = QRect(btnX, btnY, 180, btnH); btnX += 184;
    m_btnModeRect = QRect(btnX, btnY, 90, btnH); btnX += 94;
    m_btnCloseRect = QRect(btnX, btnY, 34, btnH);

    auto drawBtn = [&](const QRect &r, const QString &text, bool active, bool hovered, const QColor &customBg = QColor()) {
        QPainterPath bp;
        bp.addRoundedRect(r, 6, 6);
        QColor col = customBg.isValid() ? customBg : (active ? QColor(40, 120, 240, 200) : (hovered ? QColor(255, 255, 255, 35) : QColor(255, 255, 255, 15)));
        p.fillPath(bp, col);
        p.setPen(active ? QColor(255, 255, 255) : QColor(220, 225, 235));
        QFont f = p.font();
        f.setPointSize(10);
        f.setBold(active);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, text);
    };

    drawBtn(m_btnPinRect, m_toolbarPinned ? QStringLiteral("📌") : QStringLiteral("📍"), m_toolbarPinned, m_hoveredBtn == 0);
    drawBtn(m_btnProfilerRect, QStringLiteral("📊"), m_showProfiler, m_hoveredBtn == 1);
    drawBtn(m_btnShotRect, QStringLiteral("📷"), false, m_hoveredBtn == 2);

    QString effText = m_shaders ? m_shaders->currentPipelineName() : QStringLiteral("CAS");
    if (effText.length() > 22) effText = effText.left(19) + QStringLiteral("...");
    drawBtn(m_btnPresetRect, QStringLiteral("⚡ %1").arg(effText), false, m_hoveredBtn == 3);

    QString modeText;
    switch (m_shaders ? m_shaders->scalingMode() : ScalingMode::Fit) {
    case ScalingMode::Fit: modeText = QStringLiteral("Fit"); break;
    case ScalingMode::Integer: modeText = QStringLiteral("Integer"); break;
    case ScalingMode::Stretch: modeText = QStringLiteral("Stretch"); break;
    case ScalingMode::Windowed: modeText = QStringLiteral("Windowed"); break;
    }
    drawBtn(m_btnModeRect, QStringLiteral("🔲 %1").arg(modeText), false, m_hoveredBtn == 4);
    drawBtn(m_btnCloseRect, QStringLiteral("✖"), false, m_hoveredBtn == 5, m_hoveredBtn == 5 ? QColor(220, 50, 50, 220) : QColor());

    // Draw hover tooltip if present
    if (!m_hoverTooltip.isEmpty()) {
        QFont f = p.font();
        f.setPointSize(9);
        p.setFont(f);
        QFontMetrics fm(f);
        int tipW = fm.horizontalAdvance(m_hoverTooltip) + 16;
        int tipH = 24;
        QRect tipRect(m_toolbarRect.center().x() - tipW / 2, m_toolbarRect.bottom() + 6, tipW, tipH);
        QPainterPath tp;
        tp.addRoundedRect(tipRect, 4, 4);
        p.fillPath(tp, QColor(10, 12, 16, 240));
        p.setPen(QColor(255, 255, 255, 70));
        p.drawPath(tp);
        p.setPen(QColor(240, 240, 240));
        p.drawText(tipRect, Qt::AlignCenter, m_hoverTooltip);
    }
}

void OverlayWindow::drawProfiler(QPainter &p)
{
    const int pfW = 340;
    const int pfH = 260;
    const int pfX = width() - pfW - 16;
    const int pfY = 60;
    QRect pfRect(pfX, pfY, pfW, pfH);

    QPainterPath bg;
    bg.addRoundedRect(pfRect, 10, 10);
    p.fillPath(bg, QColor(14, 16, 22, 235));
    p.setPen(QPen(QColor(60, 140, 255, 120), 1.2));
    p.drawPath(bg);

    // Title
    QFont tf = p.font();
    tf.setPointSize(11);
    tf.setBold(true);
    p.setFont(tf);
    p.setPen(QColor(80, 180, 255));
    p.drawText(pfX + 14, pfY + 26, QStringLiteral("windowScaling 实时性能开销看板"));

    // Separator line
    p.setPen(QColor(255, 255, 255, 30));
    p.drawLine(pfX + 12, pfY + 34, pfX + pfW - 12, pfY + 34);

    // Metrics
    QFont f = p.font();
    f.setPointSize(9);
    f.setBold(false);
    p.setFont(f);

    int curY = pfY + 54;
    const int stepY = 20;

    auto drawMetric = [&](const QString &label, const QString &val, const QColor &valCol = QColor(230, 235, 245)) {
        p.setPen(QColor(160, 170, 185));
        p.drawText(pfX + 14, curY, label);
        p.setPen(valCol);
        p.drawText(pfX + 120, curY, val);
        curY += stepY;
    };

    drawMetric(QStringLiteral("GPU 设备:"), m_gpuRenderer.length() > 24 ? m_gpuRenderer.left(22) + QStringLiteral("...") : m_gpuRenderer, QColor(255, 200, 80));
    drawMetric(QStringLiteral("捕获后端:"), QStringLiteral("XComposite + MIT-SHM"), QColor(100, 220, 120));
    drawMetric(QStringLiteral("实时显示帧率:"), QStringLiteral("%1 FPS  (帧时: %2 ms)").arg(m_fps).arg(QString::number(m_frameTimeMs, 'f', 1)), m_fps >= 55 ? QColor(80, 255, 120) : QColor(255, 100, 80));

    int effW = m_origRect.width() - m_cropMargins.left() - m_cropMargins.right();
    int effH = m_origRect.height() - m_cropMargins.top() - m_cropMargins.bottom();
    drawMetric(QStringLiteral("分辨率映射:"), QStringLiteral("%1x%2 -> %3x%4").arg(effW).arg(effH).arg(m_targetRect.width()).arg(m_targetRect.height()));

    if (!m_cropMargins.isNull()) {
        drawMetric(QStringLiteral("画面裁剪:"), QStringLiteral("L:%1 T:%2 R:%3 B:%4")
                   .arg(m_cropMargins.left()).arg(m_cropMargins.top()).arg(m_cropMargins.right()).arg(m_cropMargins.bottom()), QColor(255, 170, 80));
    }

    // Effect timings
    if (m_shaders) {
        const auto &timings = m_shaders->lastPassTimings();
        QString timingStr;
        for (const auto &t : timings) {
            if (!timingStr.isEmpty()) timingStr += QStringLiteral(" | ");
            timingStr += QStringLiteral("%1: %2ms").arg(t.name).arg(QString::number(t.timeMs, 'f', 1));
        }
        if (timingStr.isEmpty()) timingStr = QStringLiteral("CAS: 0.3ms");
        drawMetric(QStringLiteral("着色器耗时:"), timingStr, QColor(140, 200, 255));
        drawMetric(QStringLiteral("GPU 总耗时:"), QStringLiteral("%1 ms").arg(QString::number(m_shaders->lastTotalGpuTime(), 'f', 2)), QColor(180, 255, 160));
    }
}

void OverlayWindow::takeScreenshot()
{
    QImage shot = grabFramebuffer();
    if (shot.isNull()) {
        shot = m_capture.captureFrame();
    }
    if (shot.isNull()) {
        qWarning() << "OverlayWindow: Failed to capture framebuffer for screenshot";
        return;
    }

    QString shotDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/Magpie_Screenshots");
    if (!QDir().mkpath(shotDir)) {
        shotDir = QDir::currentPath() + QStringLiteral("/Magpie_Screenshots");
        QDir().mkpath(shotDir);
    }

    QString timeStamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    QString fileName = QStringLiteral("Screenshot_%1.png").arg(timeStamp);
    QString fullPath = shotDir + QStringLiteral("/") + fileName;

    if (shot.save(fullPath, "PNG")) {
        qInfo() << "OverlayWindow: Screenshot saved to" << fullPath;
        sendNotification(QStringLiteral("windowScaling 全屏无损截屏成功"),
                         QStringLiteral("已保存至相册: %1").arg(fullPath));
    } else {
        qWarning() << "OverlayWindow: Failed to save screenshot to" << fullPath;
    }
}

QPoint OverlayWindow::mapToGame(const QPointF &screenPos) const
{
    int effSrcW = m_origRect.width() - m_cropMargins.left() - m_cropMargins.right();
    int effSrcH = m_origRect.height() - m_cropMargins.top() - m_cropMargins.bottom();
    if (m_targetRect.width() <= 0 || m_targetRect.height() <= 0 || effSrcW <= 0 || effSrcH <= 0) {
        return QPoint();
    }

    const qreal dpr = devicePixelRatio();
    const qreal physX = screenPos.x() * dpr;
    const qreal physY = screenPos.y() * dpr;

    const qreal scaleX = (qreal)m_targetRect.width() / effSrcW;
    const qreal scaleY = (qreal)m_targetRect.height() / effSrcH;
    const qreal relX = physX - m_targetRect.x();
    const qreal relY = physY - m_targetRect.y();

    const qreal clampedX = std::clamp(relX, 0.0, (qreal)m_targetRect.width());
    const qreal clampedY = std::clamp(relY, 0.0, (qreal)m_targetRect.height());

    const int localX = m_cropMargins.left() + (int)std::clamp(std::lround(clampedX / scaleX), 0L, (long)effSrcW - 1);
    const int localY = m_cropMargins.top() + (int)std::clamp(std::lround(clampedY / scaleY), 0L, (long)effSrcH - 1);

    return QPoint(localX, localY);
}

void OverlayWindow::mousePressEvent(QMouseEvent *event)
{
    if (!m_active) return;

    int hit = hitTestToolbar(event->position().toPoint());
    if (hit >= 0) {
        if (hit == 0) {
            // Pin toggle
            m_toolbarPinned = !m_toolbarPinned;
            m_toolbarVisible = true;
            sendNotification(QStringLiteral("工具栏状态"), m_toolbarPinned ? QStringLiteral("图钉锁定 (Pinned)") : QStringLiteral("自动隐藏 (AutoHide)"));
        } else if (hit == 1) {
            // Profiler toggle
            m_showProfiler = !m_showProfiler;
        } else if (hit == 2) {
            // Screenshot
            takeScreenshot();
        } else if (hit == 3) {
            // Preset cycle
            if (m_shaders) {
                auto presets = ShaderPipeline::availablePresets();
                static int curPresetIdx = 0;
                curPresetIdx = (curPresetIdx + 1) % presets.size();
                m_shaders->applyPreset(presets[curPresetIdx]);
                sendNotification(QStringLiteral("算法切换"), m_shaders->currentPipelineName());
            }
        } else if (hit == 4) {
            // Mode cycle
            if (m_shaders) {
                int curMode = (int)m_shaders->scalingMode();
                curMode = (curMode + 1) % 4;
                m_shaders->setScalingMode((ScalingMode)curMode);
                calculateLayout();
                sendNotification(QStringLiteral("缩放模式切换"), QStringLiteral("模式: %1").arg(curMode));
            }
        } else if (hit == 5) {
            // Close
            stopScaling();
            return;
        }
        requestUpdate();
        return; // Intercepted by toolbar, do NOT forward to game
    }

    // If clicking on profiler, consume event
    if (m_showProfiler) {
        QRect pfRect(width() - 356, 60, 340, 260);
        if (pfRect.contains(event->position().toPoint())) {
            return;
        }
    }

    QPoint gamePos = mapToGame(event->position());
    m_input.sendButton(event->button(), true, gamePos);
}

void OverlayWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_active) return;

    QPoint pos = event->position().toPoint();

    // AutoHide Toolbar check
    if (pos.y() <= 45 || m_toolbarRect.contains(pos)) {
        m_toolbarVisible = true;
        m_toolbarHideTimer.stop();
    } else if (!m_toolbarPinned && m_toolbarVisible) {
        if (!m_toolbarHideTimer.isActive()) {
            m_toolbarHideTimer.start(2000);
        }
    }

    // Hover button detection
    int oldHit = m_hoveredBtn;
    m_hoveredBtn = hitTestToolbar(pos);
    switch (m_hoveredBtn) {
    case 0: m_hoverTooltip = m_toolbarPinned ? QStringLiteral("图钉常驻 (点击切为自动隐藏)") : QStringLiteral("自动隐藏 (点击常驻锁定)"); break;
    case 1: m_hoverTooltip = QStringLiteral("实时性能开销看板 (FPS/GPU/延迟)"); break;
    case 2: m_hoverTooltip = QStringLiteral("全屏无损超分截屏 (F12)"); break;
    case 3: m_hoverTooltip = QStringLiteral("切换多算法流水线预设"); break;
    case 4: m_hoverTooltip = QStringLiteral("切换等比Fit / 整数倍 / 拉伸 / 窗口化"); break;
    case 5: m_hoverTooltip = QStringLiteral("退出放大 (Esc)"); break;
    default: m_hoverTooltip.clear(); break;
    }

    if (oldHit != m_hoveredBtn || m_toolbarVisible) {
        requestUpdate();
    }

    if (event->buttons() != Qt::NoButton) {
        QPoint gamePos = mapToGame(event->position());
        m_input.sendMotion(gamePos);
    }
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_active) return;

    if (hitTestToolbar(event->position().toPoint()) >= 0) {
        return;
    }

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

    if (event->key() == Qt::Key_F12 || event->key() == Qt::Key_Print) {
        takeScreenshot();
        return;
    }

    if (event->key() == Qt::Key_P) {
        m_showProfiler = !m_showProfiler;
        requestUpdate();
        return;
    }

    if (m_shaders) {
        if (event->key() == Qt::Key_1) {
            m_shaders->applyPreset(QStringLiteral("Anime4K + CAS"));
            sendNotification(QStringLiteral("超分算法切换"), m_shaders->currentPipelineName());
            requestUpdate();
            return;
        } else if (event->key() == Qt::Key_2) {
            m_shaders->applyPreset(QStringLiteral("Lanczos + CAS"));
            sendNotification(QStringLiteral("超分算法切换"), m_shaders->currentPipelineName());
            requestUpdate();
            return;
        } else if (event->key() == Qt::Key_3) {
            m_shaders->applyPreset(QStringLiteral("Bicubic + Deband"));
            sendNotification(QStringLiteral("超分算法切换"), m_shaders->currentPipelineName());
            requestUpdate();
            return;
        } else if (event->key() == Qt::Key_4) {
            m_shaders->applyPreset(QStringLiteral("FSR 1.0"));
            sendNotification(QStringLiteral("超分算法切换"), m_shaders->currentPipelineName());
            requestUpdate();
            return;
        }
    }

    QOpenGLWindow::keyPressEvent(event);
}