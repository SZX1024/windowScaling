#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QGroupBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QMenu>
#include <QCloseEvent>
#include <memory>
#include "capture.h"
#include "input.h"
#include "overlay_window.h"
#include "profile_manager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(X11CaptureEngine &capture, X11InputForwarder &input, QWidget *parent = nullptr);
    ~MainWindow() override;

    void triggerScaling(unsigned long targetId = 0, bool windowed = false);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNavChanged(int row);
    void refreshWindowList();
    void onWindowSelected(int index);
    void onStartScalingClicked();
    void onStartWindowedClicked();

    // Profiles page slots
    void onProfileSelected(int index);
    void onNewProfileClicked();
    void onSaveProfileClicked();
    void onDeleteProfileClicked();
    void onFillRulesFromCurrentGame();

    // Scaling mode & crop slots
    void onModeChanged(int index);
    void onWindowScaleChanged(double val);
    void onCropChanged();

    // Custom Effects Pipeline Editor slots (Magpie style)
    void onPresetChanged(int index);
    void onPipelineRowChanged(int row);
    void onAddEffectClicked();
    void onRemoveEffectClicked();
    void onMoveUpEffectClicked();
    void onMoveDownEffectClicked();
    void onSharpnessChanged(int val);
    void onAnimeChanged(int val);
    void onDebandThresholdChanged(int val);
    void onDebandRangeChanged(int val);
    void onCRTScanlineChanged(int val);

    // Settings page slots
    void onBrowseScreenshotDir();

    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUI();
    void setupTray();
    QWidget *createHomePage();
    QWidget *createProfilesPage();
    QWidget *createScalingModesPage();
    QWidget *createSettingsPage();
    QWidget *createAboutPage();

    void loadProfileToUI(const GameProfile &p);
    GameProfile collectUIProfile() const;
    void updatePipelineListUI();
    void updateParamControlsForCurrentEffect();

    X11CaptureEngine &m_capture;
    X11InputForwarder &m_input;
    std::unique_ptr<OverlayWindow> m_overlay;

    // Navigation & Stack
    QListWidget *m_navList{nullptr};
    QStackedWidget *m_pageStack{nullptr};

    // --- Page 0: Home ---
    QComboBox *m_homeWindowCombo{nullptr};
    QPushButton *m_homeRefreshBtn{nullptr};
    QLabel *m_homeProfileStatus{nullptr};
    QSlider *m_delaySlider{nullptr};
    QLabel *m_delayLabel{nullptr};
    QPushButton *m_scaleFullscreenBtn{nullptr};
    QPushButton *m_scaleWindowedBtn{nullptr};

    // --- Page 1: Profiles ---
    QComboBox *m_profileCombo{nullptr};
    QPushButton *m_newProfileBtn{nullptr};
    QPushButton *m_saveProfileBtn{nullptr};
    QPushButton *m_deleteProfileBtn{nullptr};
    QLineEdit *m_ruleClassEdit{nullptr};
    QLineEdit *m_ruleTitleEdit{nullptr};
    QPushButton *m_fillRuleBtn{nullptr};

    // --- Page 2: Scaling Modes & Custom Effects Pipeline Editor ---
    QComboBox *m_modeCombo{nullptr};
    QWidget *m_windowScaleRow{nullptr};
    QDoubleSpinBox *m_windowScaleSpin{nullptr};
    QSpinBox *m_cropLeft{nullptr};
    QSpinBox *m_cropTop{nullptr};
    QSpinBox *m_cropRight{nullptr};
    QSpinBox *m_cropBottom{nullptr};

    // Effects Pipeline Editor
    QComboBox *m_presetCombo{nullptr};
    QListWidget *m_pipelineList{nullptr};
    QPushButton *m_addEffectBtn{nullptr};
    QPushButton *m_removeEffectBtn{nullptr};
    QPushButton *m_moveUpEffectBtn{nullptr};
    QPushButton *m_moveDownEffectBtn{nullptr};
    QMenu *m_addEffectMenu{nullptr};

    // Dynamic Parameter Controls
    QGroupBox *m_paramBox{nullptr};
    QLabel *m_paramTitleLabel{nullptr};

    QWidget *m_sharpnessRow{nullptr};
    QSlider *m_sharpnessSlider{nullptr};
    QLabel *m_sharpnessLabel{nullptr};

    QWidget *m_animeRow{nullptr};
    QSlider *m_animeSlider{nullptr};
    QLabel *m_animeLabel{nullptr};

    QWidget *m_debandThresholdRow{nullptr};
    QSlider *m_debandThresholdSlider{nullptr};
    QLabel *m_debandThresholdLabel{nullptr};

    QWidget *m_debandRangeRow{nullptr};
    QSlider *m_debandRangeSlider{nullptr};
    QLabel *m_debandRangeLabel{nullptr};

    QWidget *m_crtRow{nullptr};
    QSlider *m_crtScanlineSlider{nullptr};
    QLabel *m_crtScanlineLabel{nullptr};

    // --- Page 3: Settings ---
    QComboBox *m_toolbarModeCombo{nullptr};
    QCheckBox *m_showProfilerCheck{nullptr};
    QLineEdit *m_screenshotDirEdit{nullptr};
    QPushButton *m_browseScreenshotBtn{nullptr};
    QCheckBox *m_screenshotNotifyCheck{nullptr};
    QCheckBox *m_touchSupportCheck{nullptr};
    QCheckBox *m_stayOnTopCheck{nullptr};
    QCheckBox *m_minimizeToTrayCheck{nullptr};

    // --- Page 4: About ---
    QLabel *m_gpuInfoLabel{nullptr};

    // System Tray
    QSystemTrayIcon *m_trayIcon{nullptr};
    QMenu *m_trayMenu{nullptr};

    bool m_loadingProfile{false};
};