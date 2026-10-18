#include <QApplication>
#include <QCommandLineParser>
#include <QAbstractNativeEventFilter>
#include <QDebug>
#include <iostream>

#include "capture.h"
#include "input.h"
#include "main_window.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>

// X11 Global Hotkey Filter for Ctrl+Alt+M
class X11GlobalHotkeyFilter : public QAbstractNativeEventFilter
{
public:
    explicit X11GlobalHotkeyFilter(Display *display, MainWindow *win)
        : m_display(display)
        , m_window(win)
    {
        if (m_display) {
            m_root = DefaultRootWindow(m_display);
            m_keyFullscreen = XKeysymToKeycode(m_display, XK_m);
            m_keyWindowed = XKeysymToKeycode(m_display, XK_w);

            unsigned int modifiers[] = {
                ControlMask | Mod1Mask,
                ControlMask | Mod1Mask | Mod2Mask,
                ControlMask | Mod1Mask | LockMask,
                ControlMask | Mod1Mask | Mod2Mask | LockMask
            };

            for (unsigned int mod : modifiers) {
                if (m_keyFullscreen != 0) {
                    XGrabKey(m_display, m_keyFullscreen, mod, m_root, True, GrabModeAsync, GrabModeAsync);
                }
                if (m_keyWindowed != 0) {
                    XGrabKey(m_display, m_keyWindowed, mod, m_root, True, GrabModeAsync, GrabModeAsync);
                }
            }
            XFlush(m_display);
        }
    }

    ~X11GlobalHotkeyFilter() override
    {
        if (m_display) {
            if (m_keyFullscreen != 0) XUngrabKey(m_display, m_keyFullscreen, AnyModifier, m_root);
            if (m_keyWindowed != 0) XUngrabKey(m_display, m_keyWindowed, AnyModifier, m_root);
            XFlush(m_display);
        }
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override
    {
        Q_UNUSED(result);
        if (eventType == "xcb_generic_event_t" || eventType == "X11") {
            auto *ev = reinterpret_cast<XEvent*>(message);
            if (ev && ev->type == KeyPress) {
                if (ev->xkey.keycode == m_keyFullscreen && m_keyFullscreen != 0) {
                    if (m_window) {
                        m_window->triggerScaling(0, false);
                    }
                    return true;
                } else if (ev->xkey.keycode == m_keyWindowed && m_keyWindowed != 0) {
                    if (m_window) {
                        m_window->triggerScaling(0, true);
                    }
                    return true;
                }
            }
        }
        return false;
    }

private:
    Display *m_display{nullptr};
    Window m_root{None};
    KeyCode m_keyFullscreen{0};
    KeyCode m_keyWindowed{0};
    MainWindow *m_window{nullptr};
};

int main(int argc, char **argv)
{
    // Auto-detect XAUTHORITY if needed
    if (!getenv("XAUTHORITY")) {
        FILE *fp = popen("ls -t /run/user/1000/xauth_* 2>/dev/null | head -n 1", "r");
        if (fp) {
            char path[512] = {0};
            if (fgets(path, sizeof(path) - 1, fp)) {
                char *nl = strchr(path, '\n');
                if (nl) *nl = 0;
                if (path[0]) setenv("XAUTHORITY", path, 1);
            }
            pclose(fp);
        }
    }

    if (!getenv("DISPLAY")) {
        setenv("DISPLAY", ":0", 1);
    }

    if (!getenv("QT_QPA_PLATFORM")) {
        setenv("QT_QPA_PLATFORM", "xcb", 1);
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("windowScaling"));
    app.setApplicationVersion(QStringLiteral("0.3.0"));
    app.setQuitOnLastWindowClosed(false); // Enable running minimized in system tray

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("windowScaling - Linux 窗口超分辨率放大器 (Galgame 专属)"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption listOption(QStringList() << "l" << "list", QStringLiteral("列出当前所有可检测的 XWayland 游戏窗口"));
    QCommandLineOption winOption(QStringList() << "w" << "window", QStringLiteral("指定游戏窗口 ID 或标题片段直接放大"), QStringLiteral("target"));
    QCommandLineOption trayOption(QStringList() << "t" << "tray", QStringLiteral("启动后直接最小化缩进系统托盘后台常驻"));

    parser.addOption(listOption);
    parser.addOption(winOption);
    parser.addOption(trayOption);
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
                      << "  " << (item.title.isEmpty() ? "[Wine / Game Window]" : item.title.toStdString()) << std::endl;
        }
        return 0;
    }

    X11InputForwarder input;
    if (!input.open()) {
        std::cerr << "Failed to connect to XTest input forwarder" << std::endl;
        return 1;
    }

    MainWindow mainWindow(capture, input);

    // Install X11 system-wide hotkey listener for Ctrl+Alt+M
    Display *rawDisplay = XOpenDisplay(":0");
    std::unique_ptr<X11GlobalHotkeyFilter> hotkeyFilter;
    if (rawDisplay) {
        hotkeyFilter = std::make_unique<X11GlobalHotkeyFilter>(rawDisplay, &mainWindow);
        app.installNativeEventFilter(hotkeyFilter.get());
    }

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
        mainWindow.triggerScaling(winId);
    } else if (parser.isSet(trayOption)) {
        // Start minimized in tray
        mainWindow.hide();
    } else {
        // Show main GUI configuration panel
        mainWindow.show();
    }

    int ret = app.exec();

    hotkeyFilter.reset();
    if (rawDisplay) {
        XCloseDisplay(rawDisplay);
    }

    return ret;
}