#include <QGuiApplication>
#include <QCommandLineParser>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QTimer>
#include <QDebug>
#include <iostream>
#include "capture.h"
#include "input.h"
#include "overlay_window.h"

static void sendNotification(const QString &summary, const QString &body)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify")
    );
    msg << QStringLiteral("Magpie Overlay");
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

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("magpie-overlay"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Magpie Independent Scaling Overlay for Linux Galgames"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption listOption(QStringList() << "l" << "list", QStringLiteral("List all detectable XWayland game windows."));
    QCommandLineOption winOption(QStringList() << "w" << "window", QStringLiteral("Target window ID (hex or dec) or title substring."), QStringLiteral("target"));
    QCommandLineOption delayOption(QStringList() << "d" << "delay", QStringLiteral("Delay in seconds before capturing focused window (default: 2)."), QStringLiteral("seconds"), QStringLiteral("2"));

    parser.addOption(listOption);
    parser.addOption(winOption);
    parser.addOption(delayOption);
    parser.process(app);

    X11CaptureEngine capture;
    if (!capture.open()) {
        std::cerr << "Failed to connect to X11/XWayland display (:0)" << std::endl;
        return 1;
    }

    if (parser.isSet(listOption)) {
        auto list = capture.listWindows();
        std::cout << "Available XWayland Windows (" << list.size() << " found):" << std::endl;
        for (const auto &item : list) {
            std::cout << "  0x" << std::hex << item.id << std::dec
                      << "  [" << item.width << "x" << item.height << " at (" << item.x << "," << item.y << ")]"
                      << "  " << (item.isViewable ? "[Visible]" : "[Hidden]")
                      << "  " << (item.title.isEmpty() ? std::string("[Wine / Game Window]") : item.title.toStdString()) << std::endl;
        }
        return 0;
    }

    X11InputForwarder input;
    if (!input.open()) {
        std::cerr << "Failed to connect to XTest input forwarder" << std::endl;
        return 1;
    }

    OverlayWindow overlay(capture, input);

    auto startOverlayWithId = [&](unsigned long id) {
        if (id == 0) {
            std::cerr << "No target window found to scale!" << std::endl;
            app.quit();
            return;
        }

        std::cout << "Starting Magpie Scaling on window: 0x" << std::hex << id << std::dec << std::endl;
        sendNotification(QStringLiteral("Magpie 独立全屏超分"), QStringLiteral("已全屏放大并启动触控/鼠标精确回传\n按 Esc 退出"));

        if (!overlay.startScaling(id)) {
            std::cerr << "Failed to start scaling window!" << std::endl;
            app.quit();
        }
    };

    if (parser.isSet(winOption)) {
        QString val = parser.value(winOption);
        bool ok = false;
        unsigned long winId = 0;
        if (val.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            winId = val.toULong(&ok, 16);
        } else {
            winId = val.toULong(&ok, 10);
        }
        if (!ok || winId == 0) {
            winId = capture.findWindowByTitle(val);
        }
        startOverlayWithId(winId);
    } else {
        int delaySec = parser.value(delayOption).toInt();
        if (delaySec < 0) delaySec = 0;
        std::cout << "Magpie Overlay will capture the focused window in " << delaySec << " seconds... Focus your game now!" << std::endl;

        QTimer::singleShot(delaySec * 1000, [&]() {
            unsigned long activeId = capture.getActiveWindow();
            if (activeId == 0) {
                // Pick the largest viewable window that is not a desktop/panel
                auto list = capture.listWindows();
                unsigned long bestId = 0;
                int maxArea = 0;
                for (const auto &w : list) {
                    if (w.isViewable && w.width >= 640 && w.height >= 400) {
                        int area = w.width * w.height;
                        if (area > maxArea) {
                            maxArea = area;
                            bestId = w.id;
                        }
                    }
                }
                activeId = bestId;
            }
            startOverlayWithId(activeId);
        });
    }

    QObject::connect(&overlay, &QWindow::visibilityChanged, [&](QWindow::Visibility v) {
        if (v == QWindow::Hidden) {
            sendNotification(QStringLiteral("Magpie 独立全屏超分"), QStringLiteral("已退出全屏"));
            app.quit();
        }
    });

    return app.exec();
}