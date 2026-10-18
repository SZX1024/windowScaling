#include "main_window.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

MainWindow::MainWindow(X11CaptureEngine &capture, X11InputForwarder &input, QWidget *parent)
    : QMainWindow(parent)
    , m_capture(capture)
    , m_input(input)
    , m_overlay(std::make_unique<OverlayWindow>(capture, input))
{
    setupUI();
    setupTray();

    // Load initial profile
    auto curP = ProfileManager::instance().currentProfile();
    loadProfileToUI(curP);

    refreshWindowList();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("windowScaling - Linux 窗口超分辨率放大器 (Galgame 专属)"));
    setMinimumSize(780, 560);
    resize(840, 620);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    rootLayout->addWidget(splitter);

    // Left Navigation Sidebar (Magpie style)
    m_navList = new QListWidget(splitter);
    m_navList->setFixedWidth(190);
    m_navList->addItem(QStringLiteral("🏠  主页 (Home)"));
    m_navList->addItem(QStringLiteral("🎮  配置档 (Profiles)"));
    m_navList->addItem(QStringLiteral("⚡  缩放模式与效果"));
    m_navList->addItem(QStringLiteral("⚙️  设置 (Settings)"));
    m_navList->addItem(QStringLiteral("ℹ️  关于 (About)"));
    m_navList->setCurrentRow(0);
    connect(m_navList, &QListWidget::currentRowChanged, this, &MainWindow::onNavChanged);

    // Right Page Stack (Magpie page hierarchy)
    m_pageStack = new QStackedWidget(splitter);
    m_pageStack->addWidget(createHomePage());
    m_pageStack->addWidget(createProfilesPage());
    m_pageStack->addWidget(createScalingModesPage());
    m_pageStack->addWidget(createSettingsPage());
    m_pageStack->addWidget(createAboutPage());

    splitter->addWidget(m_navList);
    splitter->addWidget(m_pageStack);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setCollapsible(0, false);
}

void MainWindow::onNavChanged(int row)
{
    if (row >= 0 && row < m_pageStack->count()) {
        m_pageStack->setCurrentIndex(row);
    }
}

// -------------------------------------------------------------
// Page 0: 主页 (HomePage)
// -------------------------------------------------------------
QWidget *MainWindow::createHomePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    // 1. 缩放激活卡片组 (Activation)
    auto *actGroup = new QGroupBox(QStringLiteral("⚡ 缩放激活 (Activation)"), page);
    auto *actLayout = new QFormLayout(actGroup);
    actLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    actLayout->setSpacing(10);

    auto *fullKeyLabel = new QLabel(QStringLiteral("<b>Ctrl + Alt + M</b>"), actGroup);
    actLayout->addRow(QStringLiteral("全屏超分辨率快捷键:"), fullKeyLabel);

    auto *winKeyLabel = new QLabel(QStringLiteral("<b>Ctrl + Alt + W</b>"), actGroup);
    actLayout->addRow(QStringLiteral("窗口化缩放快捷键:"), winKeyLabel);

    auto *delayRow = new QWidget(actGroup);
    auto *delayHBox = new QHBoxLayout(delayRow);
    delayHBox->setContentsMargins(0, 0, 0, 0);
    m_delaySlider = new QSlider(Qt::Horizontal, delayRow);
    m_delaySlider->setRange(0, 5);
    m_delaySlider->setValue(2);
    m_delayLabel = new QLabel(QStringLiteral("2 秒"), delayRow);
    m_delayLabel->setFixedWidth(40);
    connect(m_delaySlider, &QSlider::valueChanged, [this](int v) {
        m_delayLabel->setText(QStringLiteral("%1 秒").arg(v));
    });
    delayHBox->addWidget(m_delaySlider);
    delayHBox->addWidget(m_delayLabel);
    actLayout->addRow(QStringLiteral("点击放大延时:"), delayRow);

    layout->addWidget(actGroup);

    // 2. 目标游戏窗口卡片组 (Target Window)
    auto *winGroup = new QGroupBox(QStringLiteral("🎯 目标游戏窗口 (Target Window)"), page);
    auto *winLayout = new QVBoxLayout(winGroup);
    winLayout->setSpacing(10);

    auto *winRow = new QHBoxLayout();
    m_homeWindowCombo = new QComboBox(winGroup);
    m_homeWindowCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_homeWindowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onWindowSelected);

    m_homeRefreshBtn = new QPushButton(QStringLiteral("🔄 刷新"), winGroup);
    connect(m_homeRefreshBtn, &QPushButton::clicked, this, &MainWindow::refreshWindowList);

    winRow->addWidget(m_homeWindowCombo);
    winRow->addWidget(m_homeRefreshBtn);
    winLayout->addLayout(winRow);

    m_homeProfileStatus = new QLabel(QStringLiteral("当前匹配配置档: <b>默认 (Default)</b>"), winGroup);
    winLayout->addWidget(m_homeProfileStatus);

    layout->addWidget(winGroup);

    layout->addStretch();

    // 3. 立即启动操作按钮 (Scale Now)
    auto *btnRow = new QHBoxLayout();
    m_scaleFullscreenBtn = new QPushButton(QStringLiteral("🚀 立即全屏放大 (Scale Fullscreen)"), page);
    m_scaleFullscreenBtn->setFixedHeight(44);
    connect(m_scaleFullscreenBtn, &QPushButton::clicked, this, &MainWindow::onStartScalingClicked);

    m_scaleWindowedBtn = new QPushButton(QStringLiteral("🔲 立即窗口化放大 (Scale Windowed)"), page);
    m_scaleWindowedBtn->setFixedHeight(44);
    connect(m_scaleWindowedBtn, &QPushButton::clicked, this, &MainWindow::onStartWindowedClicked);

    btnRow->addWidget(m_scaleFullscreenBtn, 3);
    btnRow->addWidget(m_scaleWindowedBtn, 2);
    layout->addLayout(btnRow);

    return page;
}

// -------------------------------------------------------------
// Page 1: 配置档 (ProfilesPage)
// -------------------------------------------------------------
QWidget *MainWindow::createProfilesPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    // 1. 配置档管理
    auto *profGroup = new QGroupBox(QStringLiteral("🎮 游戏配置档管理 (Profiles)"), page);
    auto *profLayout = new QHBoxLayout(profGroup);

    m_profileCombo = new QComboBox(profGroup);
    m_profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto profNames = ProfileManager::instance().listProfileNames();
    m_profileCombo->addItems(profNames);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onProfileSelected);

    m_newProfileBtn = new QPushButton(QStringLiteral("➕ 新建"), profGroup);
    connect(m_newProfileBtn, &QPushButton::clicked, this, &MainWindow::onNewProfileClicked);

    m_saveProfileBtn = new QPushButton(QStringLiteral("💾 保存"), profGroup);
    connect(m_saveProfileBtn, &QPushButton::clicked, this, &MainWindow::onSaveProfileClicked);

    m_deleteProfileBtn = new QPushButton(QStringLiteral("🗑️ 删除"), profGroup);
    connect(m_deleteProfileBtn, &QPushButton::clicked, this, &MainWindow::onDeleteProfileClicked);

    profLayout->addWidget(m_profileCombo);
    profLayout->addWidget(m_newProfileBtn);
    profLayout->addWidget(m_saveProfileBtn);
    profLayout->addWidget(m_deleteProfileBtn);
    layout->addWidget(profGroup);

    // 2. 窗口匹配规则 (Rules)
    auto *ruleGroup = new QGroupBox(QStringLiteral("🎯 游戏窗口自动匹配规则 (Matching Rules)"), page);
    auto *ruleLayout = new QFormLayout(ruleGroup);
    ruleLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ruleLayout->setSpacing(10);

    m_ruleClassEdit = new QLineEdit(ruleGroup);
    m_ruleClassEdit->setPlaceholderText(QStringLiteral("游戏进程类名 (如 senren.exe / wine)"));
    ruleLayout->addRow(QStringLiteral("窗口类名 (WM_CLASS):"), m_ruleClassEdit);

    m_ruleTitleEdit = new QLineEdit(ruleGroup);
    m_ruleTitleEdit->setPlaceholderText(QStringLiteral("窗口标题匹配正则 (如 千恋＊万花)"));
    ruleLayout->addRow(QStringLiteral("窗口标题 (WM_NAME):"), m_ruleTitleEdit);

    m_fillRuleBtn = new QPushButton(QStringLiteral("🎯 从当前游戏目标自动填入规则"), ruleGroup);
    connect(m_fillRuleBtn, &QPushButton::clicked, this, &MainWindow::onFillRulesFromCurrentGame);
    ruleLayout->addRow(QString(), m_fillRuleBtn);

    auto *ruleTip = new QLabel(QStringLiteral("💡 启动放大时，Magpie 会自动检测焦点窗口并根据以上规则匹配加载专属配置。"), ruleGroup);
    ruleTip->setStyleSheet(QStringLiteral("color: gray;"));
    ruleLayout->addRow(QString(), ruleTip);

    layout->addWidget(ruleGroup);
    layout->addStretch();

    return page;
}

// -------------------------------------------------------------
// Page 2: 缩放模式与效果 (ScalingModesPage)
// -------------------------------------------------------------
QWidget *MainWindow::createScalingModesPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    // 1. 缩放比例模式 (Scaling Modes)
    auto *modeGroup = new QGroupBox(QStringLiteral("📐 缩放比例模式 (Scaling Modes)"), page);
    auto *modeLayout = new QFormLayout(modeGroup);
    modeLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    modeLayout->setSpacing(10);

    m_modeCombo = new QComboBox(modeGroup);
    m_modeCombo->addItem(QStringLiteral("等比适应全屏 (Fit / 自动黑边居中，Galgame标准)"), (int)ScalingMode::Fit);
    m_modeCombo->addItem(QStringLiteral("整数倍无损放大 (Integer 1x/2x/3x，复古点阵像素)"), (int)ScalingMode::Integer);
    m_modeCombo->addItem(QStringLiteral("强制拉伸铺满全屏 (Stretch)"), (int)ScalingMode::Stretch);
    m_modeCombo->addItem(QStringLiteral("窗口化缩放模式 (Windowed Mode，自定义窗口倍率)"), (int)ScalingMode::Windowed);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModeChanged);
    modeLayout->addRow(QStringLiteral("缩放比例模式:"), m_modeCombo);

    m_windowScaleRow = new QWidget(modeGroup);
    auto *wsHBox = new QHBoxLayout(m_windowScaleRow);
    wsHBox->setContentsMargins(0, 0, 0, 0);
    m_windowScaleSpin = new QDoubleSpinBox(m_windowScaleRow);
    m_windowScaleSpin->setRange(0.5, 4.0);
    m_windowScaleSpin->setSingleStep(0.25);
    m_windowScaleSpin->setValue(1.50);
    m_windowScaleSpin->setSuffix(QStringLiteral(" x"));
    connect(m_windowScaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onWindowScaleChanged);
    wsHBox->addWidget(m_windowScaleSpin);
    wsHBox->addStretch();
    modeLayout->addRow(QStringLiteral("窗口化倍率:"), m_windowScaleRow);
    m_windowScaleRow->setVisible(false);

    layout->addWidget(modeGroup);

    // 2. 画面裁剪 (Cropping)
    auto *cropGroup = new QGroupBox(QStringLiteral("✂️ 画面黑边裁剪 (Cropping)"), page);
    auto *cropLayout = new QGridLayout(cropGroup);
    cropLayout->setSpacing(10);

    cropLayout->addWidget(new QLabel(QStringLiteral("左裁 (Left):"), cropGroup), 0, 0);
    m_cropLeft = new QSpinBox(cropGroup);
    m_cropLeft->setRange(0, 800);
    m_cropLeft->setSuffix(QStringLiteral(" px"));
    connect(m_cropLeft, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onCropChanged);
    cropLayout->addWidget(m_cropLeft, 0, 1);

    cropLayout->addWidget(new QLabel(QStringLiteral("顶裁 (Top):"), cropGroup), 0, 2);
    m_cropTop = new QSpinBox(cropGroup);
    m_cropTop->setRange(0, 800);
    m_cropTop->setSuffix(QStringLiteral(" px"));
    connect(m_cropTop, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onCropChanged);
    cropLayout->addWidget(m_cropTop, 0, 3);

    cropLayout->addWidget(new QLabel(QStringLiteral("右裁 (Right):"), cropGroup), 1, 0);
    m_cropRight = new QSpinBox(cropGroup);
    m_cropRight->setRange(0, 800);
    m_cropRight->setSuffix(QStringLiteral(" px"));
    connect(m_cropRight, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onCropChanged);
    cropLayout->addWidget(m_cropRight, 1, 1);

    cropLayout->addWidget(new QLabel(QStringLiteral("底裁 (Bottom):"), cropGroup), 1, 2);
    m_cropBottom = new QSpinBox(cropGroup);
    m_cropBottom->setRange(0, 800);
    m_cropBottom->setSuffix(QStringLiteral(" px"));
    connect(m_cropBottom, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onCropChanged);
    cropLayout->addWidget(m_cropBottom, 1, 3);

    layout->addWidget(cropGroup);

    // 3. GPU 多算法串联流水线 (Effects Pipeline)
    auto *scalerGroup = new QGroupBox(QStringLiteral("✨ GPU 硬件加速多算法流水线 (Effects Pipeline)"), page);
    auto *scalerLayout = new QFormLayout(scalerGroup);
    scalerLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    scalerLayout->setSpacing(10);

    m_presetCombo = new QComboBox(scalerGroup);
    auto presets = ShaderPipeline::availablePresets();
    m_presetCombo->addItems(presets);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPresetChanged);
    scalerLayout->addRow(QStringLiteral("算法流水线预设:"), m_presetCombo);

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

    layout->addWidget(scalerGroup);
    layout->addStretch();

    return page;
}

// -------------------------------------------------------------
// Page 3: 设置 (SettingsPage)
// -------------------------------------------------------------
QWidget *MainWindow::createSettingsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    // 1. 置顶工具栏与性能看板
    auto *tbGroup = new QGroupBox(QStringLiteral("📌 全屏置顶悬浮工具栏 (Overlay Toolbar)"), page);
    auto *tbLayout = new QFormLayout(tbGroup);
    tbLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tbLayout->setSpacing(10);

    m_toolbarModeCombo = new QComboBox(tbGroup);
    m_toolbarModeCombo->addItem(QStringLiteral("自动隐藏 (AutoHide - 鼠标靠近屏幕顶部时滑出)"));
    m_toolbarModeCombo->addItem(QStringLiteral("图钉常驻 (Pinned - 始终锁定在屏幕顶部)"));
    connect(m_toolbarModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_overlay) m_overlay->setToolbarPinned(idx == 1);
    });
    tbLayout->addRow(QStringLiteral("工具栏模式:"), m_toolbarModeCombo);

    m_showProfilerCheck = new QCheckBox(QStringLiteral("默认展开实时性能开销看板 (Profiler: FPS / GPU耗时 / 分辨率映射)"), tbGroup);
    connect(m_showProfilerCheck, &QCheckBox::toggled, this, [this](bool c) {
        if (m_overlay) m_overlay->setShowProfiler(c);
    });
    tbLayout->addRow(QString(), m_showProfilerCheck);

    layout->addWidget(tbGroup);

    // 2. 无损截屏设置
    auto *shotGroup = new QGroupBox(QStringLiteral("📷 全屏无损截屏 (Take Screenshot)"), page);
    auto *shotLayout = new QFormLayout(shotGroup);
    shotLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    shotLayout->setSpacing(10);

    shotLayout->addRow(QStringLiteral("截屏快捷键:"), new QLabel(QStringLiteral("<b>F12</b> 或 <b>PrintScreen</b> (全屏中随时按下)"), shotGroup));

    auto *pathRow = new QWidget(shotGroup);
    auto *pathHBox = new QHBoxLayout(pathRow);
    pathHBox->setContentsMargins(0, 0, 0, 0);

    QString defaultShotDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/Magpie_Screenshots");
    m_screenshotDirEdit = new QLineEdit(defaultShotDir, pathRow);
    m_screenshotDirEdit->setReadOnly(true);

    m_browseScreenshotBtn = new QPushButton(QStringLiteral("浏览..."), pathRow);
    connect(m_browseScreenshotBtn, &QPushButton::clicked, this, &MainWindow::onBrowseScreenshotDir);

    pathHBox->addWidget(m_screenshotDirEdit);
    pathHBox->addWidget(m_browseScreenshotBtn);
    shotLayout->addRow(QStringLiteral("截图保存目录:"), pathRow);

    m_screenshotNotifyCheck = new QCheckBox(QStringLiteral("截屏成功后发送系统桌面通知"), shotGroup);
    m_screenshotNotifyCheck->setChecked(true);
    shotLayout->addRow(QString(), m_screenshotNotifyCheck);

    layout->addWidget(shotGroup);

    // 3. 输入与常规
    auto *inputGroup = new QGroupBox(QStringLiteral("👆 触控、鼠标与前台焦点保护"), page);
    auto *inLayout = new QVBoxLayout(inputGroup);
    inLayout->setSpacing(8);

    m_touchSupportCheck = new QCheckBox(QStringLiteral("启用触控屏 1:1 绝对对齐映射 (Touch Support)"), inputGroup);
    m_touchSupportCheck->setChecked(true);
    inLayout->addWidget(m_touchSupportCheck);

    m_stayOnTopCheck = new QCheckBox(QStringLiteral("锁定目标游戏图层并维持前台焦点 (防止游戏在后台冻结暂停，Live2D 永续)"), inputGroup);
    m_stayOnTopCheck->setChecked(true);
    inLayout->addWidget(m_stayOnTopCheck);

    m_minimizeToTrayCheck = new QCheckBox(QStringLiteral("点击关闭窗口时自动最小化缩进系统托盘后台常驻"), inputGroup);
    m_minimizeToTrayCheck->setChecked(true);
    inLayout->addWidget(m_minimizeToTrayCheck);

    layout->addWidget(inputGroup);
    layout->addStretch();

    return page;
}

// -------------------------------------------------------------
// Page 4: 关于 (AboutPage)
// -------------------------------------------------------------
QWidget *MainWindow::createAboutPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);

    auto *titleLabel = new QLabel(QStringLiteral("<h2>windowScaling for Linux (0.3.0)</h2>"), page);
    auto *descLabel = new QLabel(QStringLiteral("专为 Linux (KDE Plasma 6 / Wayland) 下的 Galgame / 视觉小说 / 二次元游戏打造的窗口超分辨率放大工具。<br>"
                                               "全面对标 Windows 原版 Magpie，具备 GPU 多算法顺序串联管线、触控绝对对齐与置顶工具栏。"), page);
    descLabel->setWordWrap(true);

    layout->addWidget(titleLabel);
    layout->addWidget(descLabel);

    auto *sysGroup = new QGroupBox(QStringLiteral("💻 运行环境与图形加速诊断"), page);
    auto *sysLayout = new QFormLayout(sysGroup);
    sysLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sysLayout->setSpacing(10);

    const char *glVendor = (const char*)glGetString(GL_RENDERER);
    QString gpuDesc = glVendor ? QString::fromUtf8(glVendor) : QStringLiteral("OpenGL Accelerated Device");
    sysLayout->addRow(QStringLiteral("GPU 设备:"), new QLabel(gpuDesc, sysGroup));
    sysLayout->addRow(QStringLiteral("捕获管线:"), new QLabel(QStringLiteral("XComposite + MIT-SHM (零拷贝共享显存)"), sysGroup));
    sysLayout->addRow(QStringLiteral("窗口系统:"), new QLabel(QStringLiteral("XWayland / Wayland (KDE Plasma 6)"), sysGroup));

    layout->addWidget(sysGroup);

    auto *creditsGroup = new QGroupBox(QStringLiteral("致敬与开源协议"), page);
    auto *crLayout = new QVBoxLayout(creditsGroup);
    crLayout->addWidget(new QLabel(QStringLiteral("本工具深度参考并全面对标 Windows 原版项目 <b>Blinue/Magpie</b>。<br>"
                                                 "着色器算法涵盖 Anime4K (bloc97), AMD FidelityFX (CAS & FSR 1.0), mpv-player deband 等。"), creditsGroup));
    layout->addWidget(creditsGroup);

    layout->addStretch();
    return page;
}

void MainWindow::setupTray()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows"), windowIcon()));
    m_trayIcon->setToolTip(QStringLiteral("windowScaling 超分辨率工具"));

    m_trayMenu = new QMenu(this);

    auto *scaleAction = m_trayMenu->addAction(QStringLiteral("⚡ 全屏放大当前焦点游戏 (Ctrl+Alt+M)"));
    connect(scaleAction, &QAction::triggered, this, [this]() {
        triggerScaling(0, false);
    });

    auto *winScaleAction = m_trayMenu->addAction(QStringLiteral("🔲 窗口化放大当前焦点游戏 (Ctrl+Alt+W)"));
    connect(winScaleAction, &QAction::triggered, this, [this]() {
        triggerScaling(0, true);
    });

    m_trayMenu->addSeparator();

    auto *shotAction = m_trayMenu->addAction(QStringLiteral("📷 一键全屏无损截屏 (F12)"));
    connect(shotAction, &QAction::triggered, this, [this]() {
        if (m_overlay) m_overlay->takeScreenshot();
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
    if (m_minimizeToTrayCheck && m_minimizeToTrayCheck->isChecked() && m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore(); // Minimize to tray instead of quitting
    } else {
        event->accept();
    }
}

void MainWindow::loadProfileToUI(const GameProfile &p)
{
    m_loadingProfile = true;

    int idx = m_profileCombo->findText(p.name);
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
    }

    if (m_homeProfileStatus) {
        m_homeProfileStatus->setText(QStringLiteral("当前匹配配置档: <b>%1</b>").arg(p.name));
    }

    m_ruleClassEdit->setText(p.windowClassPattern);
    m_ruleTitleEdit->setText(p.windowTitlePattern);

    // Match preset
    QString effStr = p.effectChain.join(" + ");
    for (int i = 0; i < m_presetCombo->count(); ++i) {
        if (m_presetCombo->itemText(i).contains(effStr)) {
            m_presetCombo->setCurrentIndex(i);
            break;
        }
    }

    m_modeCombo->setCurrentIndex(p.scalingMode);
    m_windowScaleRow->setVisible(p.scalingMode == 3);
    m_windowScaleSpin->setValue(p.windowScale);

    m_sharpnessSlider->setValue(std::lround(p.sharpness * 100));
    m_sharpnessLabel->setText(QString::number(p.sharpness, 'f', 2));

    m_animeSlider->setValue(std::lround(p.animeStrength * 100));
    m_animeLabel->setText(QString::number(p.animeStrength, 'f', 2));

    m_cropLeft->setValue(p.cropLeft);
    m_cropTop->setValue(p.cropTop);
    m_cropRight->setValue(p.cropRight);
    m_cropBottom->setValue(p.cropBottom);

    m_toolbarModeCombo->setCurrentIndex(p.toolbarPinned ? 1 : 0);
    m_showProfilerCheck->setChecked(p.showProfiler);

    if (m_overlay) {
        m_overlay->setCropMargins(QMargins(p.cropLeft, p.cropTop, p.cropRight, p.cropBottom));
        m_overlay->setWindowScale(p.windowScale);
        m_overlay->setToolbarPinned(p.toolbarPinned);
        m_overlay->setShowProfiler(p.showProfiler);
        if (m_overlay->shaderPipeline()) {
            m_overlay->shaderPipeline()->setEffectChain(p.effectChain);
            m_overlay->shaderPipeline()->setScalingMode((ScalingMode)p.scalingMode);
            m_overlay->shaderPipeline()->setSharpness(p.sharpness);
            m_overlay->shaderPipeline()->setAnimeStrength(p.animeStrength);
        }
    }

    m_loadingProfile = false;
}

GameProfile MainWindow::collectUIProfile() const
{
    GameProfile p;
    p.name = m_profileCombo->currentText();
    p.windowClassPattern = m_ruleClassEdit->text().trimmed();
    p.windowTitlePattern = m_ruleTitleEdit->text().trimmed();

    if (m_overlay && m_overlay->shaderPipeline()) {
        p.effectChain = m_overlay->shaderPipeline()->effectChain();
    } else {
        p.effectChain = {"Anime4K", "CAS"};
    }
    p.scalingMode = m_modeCombo->currentIndex();
    p.windowScale = (float)m_windowScaleSpin->value();
    p.sharpness = m_sharpnessSlider->value() / 100.0f;
    p.animeStrength = m_animeSlider->value() / 100.0f;

    p.cropLeft = m_cropLeft->value();
    p.cropTop = m_cropTop->value();
    p.cropRight = m_cropRight->value();
    p.cropBottom = m_cropBottom->value();

    p.toolbarPinned = (m_toolbarModeCombo->currentIndex() == 1);
    p.showProfiler = m_showProfilerCheck->isChecked();

    return p;
}

void MainWindow::onProfileSelected(int index)
{
    if (m_loadingProfile || index < 0) return;
    QString name = m_profileCombo->itemText(index);
    GameProfile p = ProfileManager::instance().getProfileByName(name);
    loadProfileToUI(p);
}

void MainWindow::onSaveProfileClicked()
{
    GameProfile p = collectUIProfile();
    ProfileManager::instance().saveProfile(p);
    QMessageBox::information(this, QStringLiteral("配置已保存"), QStringLiteral("配置档 [%1] 保存成功！").arg(p.name));
}

void MainWindow::onNewProfileClicked()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("新建配置档"), QStringLiteral("请输入配置档名称 (如游戏名称):"), QLineEdit::Normal, QStringLiteral(""), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        GameProfile p = collectUIProfile();
        p.name = name.trimmed();
        ProfileManager::instance().saveProfile(p);
        m_profileCombo->addItem(p.name);
        m_profileCombo->setCurrentText(p.name);
        loadProfileToUI(p);
    }
}

void MainWindow::onDeleteProfileClicked()
{
    QString name = m_profileCombo->currentText();
    if (name == QStringLiteral("默认 (Default)")) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("默认配置档不可删除。"));
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("删除确认"), QStringLiteral("确定删除配置档 [%1] 吗？").arg(name)) == QMessageBox::Yes) {
        ProfileManager::instance().deleteProfile(name);
        int idx = m_profileCombo->findText(name);
        if (idx >= 0) m_profileCombo->removeItem(idx);
        loadProfileToUI(ProfileManager::instance().currentProfile());
    }
}

void MainWindow::onFillRulesFromCurrentGame()
{
    int index = m_homeWindowCombo->currentIndex();
    if (index <= 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先在主页选择目标游戏窗口。"));
        return;
    }

    unsigned long winId = m_homeWindowCombo->itemData(index).toULongLong();
    QString winClass = m_capture.getWindowClass(winId);
    QString winTitle = m_capture.getWindowTitle(winId);

    if (!winClass.isEmpty()) m_ruleClassEdit->setText(winClass);
    if (!winTitle.isEmpty()) m_ruleTitleEdit->setText(winTitle);

    QMessageBox::information(this, QStringLiteral("填入成功"), QStringLiteral("已自动提取并填入游戏类名与标题规则。"));
}

void MainWindow::refreshWindowList()
{
    m_homeWindowCombo->blockSignals(true);
    m_homeWindowCombo->clear();
    m_homeWindowCombo->addItem(QStringLiteral("🎯 自动捕获当前焦点游戏 (首选)"), 0);

    auto list = m_capture.listWindows();
    for (const auto &w : list) {
        if (w.isViewable && w.width >= 400 && w.height >= 300) {
            QString title = w.title.isEmpty() ? QStringLiteral("[Wine / Game Window]") : w.title;
            QString text = QStringLiteral("0x%1 [%2x%3] %4")
                               .arg(QString::number(w.id, 16))
                               .arg(w.width)
                               .arg(w.height)
                               .arg(title);
            m_homeWindowCombo->addItem(text, (qulonglong)w.id);
        }
    }
    m_homeWindowCombo->blockSignals(false);
}

void MainWindow::onWindowSelected(int index)
{
    if (index <= 0) return;
    unsigned long winId = m_homeWindowCombo->itemData(index).toULongLong();
    QString winClass = m_capture.getWindowClass(winId);
    QString winTitle = m_capture.getWindowTitle(winId);

    // Auto load matched profile
    GameProfile p = ProfileManager::instance().getProfileForWindow(winClass, winTitle);
    loadProfileToUI(p);
}

void MainWindow::onPresetChanged(int index)
{
    if (m_loadingProfile) return;
    QString presetText = m_presetCombo->itemText(index);
    if (m_overlay && m_overlay->shaderPipeline()) {
        m_overlay->shaderPipeline()->applyPreset(presetText);
    }
}

void MainWindow::onModeChanged(int index)
{
    ScalingMode mode = (ScalingMode)m_modeCombo->itemData(index).toInt();
    m_windowScaleRow->setVisible(mode == ScalingMode::Windowed);
    if (m_overlay && m_overlay->shaderPipeline()) {
        m_overlay->shaderPipeline()->setScalingMode(mode);
    }
}

void MainWindow::onWindowScaleChanged(double val)
{
    if (m_overlay) {
        m_overlay->setWindowScale((float)val);
    }
}

void MainWindow::onCropChanged()
{
    QMargins margins(m_cropLeft->value(), m_cropTop->value(), m_cropRight->value(), m_cropBottom->value());
    if (m_overlay) {
        m_overlay->setCropMargins(margins);
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

void MainWindow::onBrowseScreenshotDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择截图保存相册目录"), m_screenshotDirEdit->text());
    if (!dir.isEmpty()) {
        m_screenshotDirEdit->setText(dir);
    }
}

void MainWindow::onStartScalingClicked()
{
    unsigned long targetId = m_homeWindowCombo->currentData().toULongLong();
    int delaySec = m_delaySlider->value();

    if (delaySec > 0) {
        m_scaleFullscreenBtn->setEnabled(false);
        m_scaleFullscreenBtn->setText(QStringLiteral("⏳ 请在 %1 秒内切回游戏窗口...").arg(delaySec));
        QTimer::singleShot(delaySec * 1000, this, [this, targetId]() {
            m_scaleFullscreenBtn->setEnabled(true);
            m_scaleFullscreenBtn->setText(QStringLiteral("🚀 立即全屏放大 (Scale Fullscreen)"));
            triggerScaling(targetId, false);
        });
    } else {
        triggerScaling(targetId, false);
    }
}

void MainWindow::onStartWindowedClicked()
{
    unsigned long targetId = m_homeWindowCombo->currentData().toULongLong();
    int delaySec = m_delaySlider->value();

    if (delaySec > 0) {
        m_scaleWindowedBtn->setEnabled(false);
        m_scaleWindowedBtn->setText(QStringLiteral("⏳ 请在 %1 秒内切回游戏窗口...").arg(delaySec));
        QTimer::singleShot(delaySec * 1000, this, [this, targetId]() {
            m_scaleWindowedBtn->setEnabled(true);
            m_scaleWindowedBtn->setText(QStringLiteral("🔲 立即窗口化放大 (Scale Windowed)"));
            triggerScaling(targetId, true);
        });
    } else {
        triggerScaling(targetId, true);
    }
}

void MainWindow::triggerScaling(unsigned long targetId, bool windowed)
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

    // Auto-match profile on trigger
    QString winClass = m_capture.getWindowClass(targetId);
    QString winTitle = m_capture.getWindowTitle(targetId);
    GameProfile p = ProfileManager::instance().getProfileForWindow(winClass, winTitle);
    loadProfileToUI(p);

    m_overlay->startScaling(targetId, windowed);
}