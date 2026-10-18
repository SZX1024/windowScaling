#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
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

    // Scaling & effects slots
    void onPresetChanged(int index);
    void onModeChanged(int index);
    void onSharpnessChanged(int val);
    void onAnimeChanged(int val);
    void onCropChanged();
    void onWindowScaleChanged(double val);

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

    // --- Page 2: Scaling Modes & Effects ---
    QComboBox *m_modeCombo{nullptr};
    QWidget *m_windowScaleRow{nullptr};
    QDoubleSpinBox *m_windowScaleSpin{nullptr};
    QSpinBox *m_cropLeft{nullptr};
    QSpinBox *m_cropTop{nullptr};
    QSpinBox *m_cropRight{nullptr};
    QSpinBox *m_cropBottom{nullptr};
    QComboBox *m_presetCombo{nullptr};
    QSlider *m_sharpnessSlider{nullptr};
    QLabel *m_sharpnessLabel{nullptr};
    QSlider *m_animeSlider{nullptr};
    QLabel *m_animeLabel{nullptr};

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
