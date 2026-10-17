#include "main_window.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QApplication>
#include <QDebug>

MainWindow::MainWindow(X11CaptureEngine &capture, X11InputForwarder &input, QWidget *parent)
    : QMainWindow(parent)
    , m_capture(capture)
    , m_input(input)
    , m_overlay(std::make_unique<OverlayWindow>(capture, input))
{
    setupUI();
    setupTray();

    refreshWindowList();

    // Default to CAS and Fit
    m_overlay->shaderPipeline()->setScaler(ScalerType::CAS);
    m_overlay->shaderPipeline()->setScalingMode(ScalingMode::Fit);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("Magpie - Linux 窗口超分辨率放大器"));
    setMinimumSize(540, 480);
    resize(580, 520);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    // Group 1: 游戏窗口检测与目标选择
    auto *gameGroup = new QGroupBox(QStringLiteral("🎮 游戏目标窗口 (Wine / Proton)"), this);
    auto *gameLayout = new QHBoxLayout(gameGroup);

    m_windowCombo = new QComboBox(gameGroup);
    m_windowCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_refreshBtn = new QPushButton(QStringLiteral("🔄 刷新"), gameGroup);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshWindowList);

    gameLayout->addWidget(m_windowCombo);
    gameLayout->addWidget(m_refreshBtn);
    mainLayout->addWidget(gameGroup);

    // Group 2: 缩放与超分算法设置
    auto *scalerGroup = new QGroupBox(QStringLiteral("✨ GPU 硬件加速超分算法"), this);
    auto *scalerLayout = new QFormLayout(scalerGroup);
    scalerLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    scalerLayout->setSpacing(10);

    m_scalerCombo = new QComboBox(scalerGroup);
    m_scalerCombo->addItem(QStringLiteral("AMD FidelityFX CAS (自适应对比度锐化，Galgame文字首选)"), (int)ScalerType::CAS);
    m_scalerCombo->addItem(QStringLiteral("Anime4K 动漫线条重建 (二次元立绘暗线强化)"), (int)ScalerType::Anime4K);
    m_scalerCombo->addItem(QStringLiteral("Lanczos3 Sinc 高保真抗振铃插值"), (int)ScalerType::Lanczos);
    m_scalerCombo->addItem(QStringLiteral("Bilinear 快速双线性插值 (核显节能)"), (int)ScalerType::Bilinear);
    connect(m_scalerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onScalerChanged);
    scalerLayout->addRow(QStringLiteral("超分算法:"), m_scalerCombo);

    m_modeCombo = new QComboBox(scalerGroup);
    m_modeCombo->addItem(QStringLiteral("等比适应全屏 (Fit / 黑边居中，Galgame标准)"), (int)ScalingMode::Fit);
    m_modeCombo->addItem(QStringLiteral("整数倍无损放大 (Integer 1x/2x/3x，复古像素游戏)"), (int)ScalingMode::Integer);
    m_modeCombo->addItem(QStringLiteral("强制拉伸铺满全屏 (Stretch)"), (int)ScalingMode::Stretch);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModeChanged);
    scalerLayout->addRow(QStringLiteral("缩放比例模式:"), m_modeCombo);

    // CAS Sharpness slider
    auto *sharpnessRow = new QWidget(scalerGroup);
    auto *sharpnessHBox = new QHBoxLayout(sharpnessRow);
    sharpnessHBox->setContentsMargins(0, 0, 0, 0);
    m_sharpnessSlider = new QSlider(Qt::Horizontal, sharpnessRow);
    m_sharpnessSlider->setRange(0, 100);
    m_sharpnessSlider->setValue(45);
    m_sharpnessLabel = new QLabel(QStringLiteral("0.45"), sharpnessRow);
    m_sharpnessLabel->setFixedWidth(36);
    connect(m_sharpnessSlider, &QSlider::valueChanged, this, &MainWindow::onSharpnessChanged);
    sharpnessHBox->addWidget(m_sharpnessSlider);
    sharpnessHBox->addWidget(m_sharpnessLabel);
    scalerLayout->addRow(QStringLiteral("CAS 锐度:"), sharpnessRow);

    // Anime4K line strength slider
    auto *animeRow = new QWidget(scalerGroup);
    auto *animeHBox = new QHBoxLayout(animeRow);
    animeHBox->setContentsMargins(0, 0, 0, 0);
    m_animeSlider = new QSlider(Qt::Horizontal, animeRow);
    m_animeSlider->setRange(0, 100);
    m_animeSlider->setValue(50);
    m_animeLabel = new QLabel(QStringLiteral("0.50"), animeRow);
    m_animeLabel->setFixedWidth(36);
    connect(m_animeSlider, &QSlider::valueChanged, this, &MainWindow::onAnimeChanged);
    animeHBox->addWidget(m_animeSlider);
    animeHBox->addWidget(m_animeLabel);
    scalerLayout->addRow(QStringLiteral("动漫线条强化:"), animeRow);

    mainLayout->addWidget(scalerGroup);

    // Group 3: 触发设置与快捷键
    auto *hotkeyGroup = new QGroupBox(QStringLiteral("⚡ 快捷键与启动"), this);
    auto *hotkeyLayout = new QFormLayout(hotkeyGroup);

    auto *delayRow = new QWidget(hotkeyGroup);
    auto *delayHBox = new QHBoxLayout(delayRow);
    delayHBox->setContentsMargins(0, 0, 0, 0);
    m_delaySlider = new QSlider(Qt::Horizontal, delayRow);
    m_delaySlider->setRange(0, 5);
    m_delaySlider->setValue(2);
    m_delayLabel = new QLabel(QStringLiteral("2 秒"), delayRow);
    m_delayLabel->setFixedWidth(36);
    connect(m_delaySlider, &QSlider::valueChanged, [this](int v) {
        m_delayLabel->setText(QStringLiteral("%1 秒").arg(v));
    });
    delayHBox->addWidget(m_delaySlider);
    delayHBox->addWidget(m_delayLabel);
    hotkeyLayout->addRow(QStringLiteral("点击放大延时:"), delayRow);

    auto *infoLabel = new QLabel(QStringLiteral("💡 全局快捷键: <b>Ctrl + Alt + M</b><br>"
                                               "💡 全屏中按 <b>1/2/3/4</b> 瞬切超分算法 | 按 <b>Esc</b> 退出全屏<br>"
                                               "💡 点击关闭窗口将<b>自动缩进系统托盘</b>后台常驻"), hotkeyGroup);
    infoLabel->setStyleSheet(QStringLiteral("color: #888;"));
    hotkeyLayout->addRow(infoLabel);

    mainLayout->addWidget(hotkeyGroup);

    // Bottom Action Button
    m_scaleBtn = new QPushButton(QStringLiteral("🚀 立即全屏放大"), this);
    m_scaleBtn->setFixedHeight(44);
    m_scaleBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    connect(m_scaleBtn, &QPushButton::clicked, this, &MainWindow::onStartScalingClicked);
    mainLayout->addWidget(m_scaleBtn);
}

void MainWindow::setupTray()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows"), windowIcon()));
    m_trayIcon->setToolTip(QStringLiteral("Magpie Linux 超分辨率工具"));

    m_trayMenu = new QMenu(this);

    auto *scaleAction = m_trayMenu->addAction(QStringLiteral("⚡ 放大当前焦点游戏"));
    connect(scaleAction, &QAction::triggered, this, [this]() {
        triggerScaling(0);
    });

    m_trayMenu->addSeparator();

    auto *showAction = m_trayMenu->addAction(QStringLiteral("⚙️ 打开设置面板"));
    connect(showAction, &QAction::triggered, this, [this]() {
        showNormal();
        activateWindow();
    });

    auto *quitAction = m_trayMenu->addAction(QStringLiteral("❌ 彻底退出"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);

    m_trayIcon->show();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) {
            hide();
        } else {
            showNormal();
            activateWindow();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore(); // Minimize to tray instead of quitting
    } else {
        event->accept();
    }
}

void MainWindow::refreshWindowList()
{
    m_windowCombo->clear();
    m_windowCombo->addItem(QStringLiteral("🔍 自动捕获当前焦点游戏 (首选)"), 0);

    auto list = m_capture.listWindows();
    for (const auto &w : list) {
        if (w.isViewable && w.width >= 400 && w.height >= 300) {
            QString title = w.title.isEmpty() ? QStringLiteral("[Wine / Game Window]") : w.title;
            QString text = QStringLiteral("0x%1 [%2x%3] %4")
                               .arg(QString::number(w.id, 16))
                               .arg(w.width)
                               .arg(w.height)
                               .arg(title);
            m_windowCombo->addItem(text, (qulonglong)w.id);
        }
    }
}

void MainWindow::onScalerChanged(int index)
{
    ScalerType type = (ScalerType)m_scalerCombo->itemData(index).toInt();
    if (m_overlay && m_overlay->shaderPipeline()) {
        m_overlay->shaderPipeline()->setScaler(type);
    }
}

void MainWindow::onModeChanged(int index)
{
    ScalingMode mode = (ScalingMode)m_modeCombo->itemData(index).toInt();
    if (m_overlay && m_overlay->shaderPipeline()) {
        m_overlay->shaderPipeline()->setScalingMode(mode);
    }
}

void MainWindow::onSharpnessChanged(int val)
{
    float s = val / 100.0f;
    m_sharpnessLabel->setText(QString::number(s, 'f', 2));
    if (m_overlay && m_overlay->shaderPipeline()) {
        m_overlay->shaderPipeline()->setSharpness(s);
    }
}

void MainWindow::onAnimeChanged(int val)
{
    float a = val / 100.0f;
    m_animeLabel->setText(QString::number(a, 'f', 2));
    if (m_overlay && m_overlay->shaderPipeline()) {
        m_overlay->shaderPipeline()->setAnimeStrength(a);
    }
}

void MainWindow::onStartScalingClicked()
{
    unsigned long targetId = m_windowCombo->currentData().toULongLong();
    int delaySec = m_delaySlider->value();

    if (delaySec > 0) {
        m_scaleBtn->setEnabled(false);
        m_scaleBtn->setText(QStringLiteral("⏳ 请在 %1 秒内切回游戏窗口...").arg(delaySec));
        QTimer::singleShot(delaySec * 1000, this, [this, targetId]() {
            m_scaleBtn->setEnabled(true);
            m_scaleBtn->setText(QStringLiteral("🚀 立即全屏放大"));
            triggerScaling(targetId);
        });
    } else {
        triggerScaling(targetId);
    }
}

void MainWindow::triggerScaling(unsigned long targetId)
{
    if (targetId == 0) {
        targetId = m_capture.getActiveWindow();
        if (targetId == 0) {
            auto list = m_capture.listWindows();
            for (const auto &w : list) {
                if (w.isViewable && w.width >= 640 && w.height >= 400) {
                    targetId = w.id;
                    break;
                }
            }
        }
    }

    if (targetId == 0) {
        qWarning() << "MainWindow: No valid target window found";
        return;
    }

    m_overlay->startScaling(targetId);
}
