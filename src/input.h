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

    // Mouse input injection via XTest
    void sendMotion(const QPoint &globalPos);
    void sendButton(Qt::MouseButton button, bool pressed, const QPoint &globalPos);
    void sendClick(Qt::MouseButton button, const QPoint &globalPos);
    void sendWheel(int deltaY, const QPoint &globalPos);

    // Touch input injection via XTest
    void sendTouchDown(const QPoint &globalPos);
    void sendTouchMotion(const QPoint &globalPos);
    void sendTouchUp(const QPoint &globalPos);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
