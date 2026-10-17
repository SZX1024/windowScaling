#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <vector>
#include <memory>

struct XWindowItem {
    unsigned long id{0};
    QString title;
    int x{0}, y{0}, width{0}, height{0};
    bool isViewable{false};
};

class X11CaptureEngine {
public:
    X11CaptureEngine();
    ~X11CaptureEngine();

    bool open(const char *displayName = ":0");
    void close();

    std::vector<XWindowItem> listWindows();
    unsigned long getActiveWindow();
    unsigned long findWindowByTitle(const QString &titleSubstring);

    bool setTargetWindow(unsigned long windowId);
    void releaseTarget();

    QImage captureFrame();
    bool isTargetAlive();
    QRect targetRect() const;
    unsigned long targetId() const { return m_target; }

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    unsigned long m_target{0};
};