#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCloseEvent>
#include <memory>
#include "capture.h"
#include "input.h"
#include "overlay_window.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(X11CaptureEngine &capture, X11InputForwarder &input, QWidget *parent = nullptr);
    ~MainWindow() override;

    void triggerScaling(unsigned long targetId = 0);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void refreshWindowList();
    void onStartScalingClicked();
    void onScalerChanged(int index);
    void onModeChanged(int index);
    void onSharpnessChanged(int val);
    void onAnimeChanged(int val);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUI();
    void setupTray();

    X11CaptureEngine &m_capture;
    X11InputForwarder &m_input;
    std::unique_ptr<OverlayWindow> m_overlay;

    // UI Elements
    QComboBox *m_windowCombo{nullptr};
    QPushButton *m_refreshBtn{nullptr};
    QComboBox *m_scalerCombo{nullptr};
    QComboBox *m_modeCombo{nullptr};
    QSlider *m_sharpnessSlider{nullptr};
    QLabel *m_sharpnessLabel{nullptr};
    QSlider *m_animeSlider{nullptr};
    QLabel *m_animeLabel{nullptr};
    QSlider *m_delaySlider{nullptr};
    QLabel *m_delayLabel{nullptr};
    QPushButton *m_scaleBtn{nullptr};

    // System Tray
    QSystemTrayIcon *m_trayIcon{nullptr};
    QMenu *m_trayMenu{nullptr};
};
