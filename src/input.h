#pragma once

#include <QPoint>
#include <QRect>
#include <Qt>
#include <memory>

class X11InputForwarder {
public:
    X11InputForwarder();
    ~X11InputForwarder();

    bool open(const char *displayName = ":0");
    void close();

    void setTarget(unsigned long windowId, const QRect &targetRect);

    // Direct protocol injection via XSendEvent (Never warps hardware pointer!)
    void sendMotion(const QPoint &localPos);
    void sendButton(Qt::MouseButton button, bool pressed, const QPoint &localPos);
    void sendClick(Qt::MouseButton button, const QPoint &localPos);
    void sendWheel(int deltaY, const QPoint &localPos);

    // Touch input (mapped to direct button 1 press/release on target window)
    void sendTouchDown(const QPoint &localPos);
    void sendTouchMotion(const QPoint &localPos);
    void sendTouchUp(const QPoint &localPos);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
