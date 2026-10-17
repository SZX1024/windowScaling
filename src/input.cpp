#include "input.h"

#include <QDebug>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

struct X11InputForwarder::Impl {
    Display *display = nullptr;
    Window targetWindow = None;
    QRect targetRect;
};

X11InputForwarder::X11InputForwarder()
    : d(std::make_unique<Impl>())
{
}

X11InputForwarder::~X11InputForwarder()
{
    close();
}

bool X11InputForwarder::open(const char *displayName)
{
    if (d->display) {
        return true;
    }

    d->display = XOpenDisplay(displayName);
    if (!d->display) {
        qWarning() << "X11InputForwarder: Failed to open X11 display:" << displayName;
        return false;
    }

    int eventBase, errorBase, major, minor;
    if (!XTestQueryExtension(d->display, &eventBase, &errorBase, &major, &minor)) {
        qWarning() << "X11InputForwarder: XTest extension not supported!";
        close();
        return false;
    }

    return true;
}

void X11InputForwarder::close()
{
    if (d->display) {
        XCloseDisplay(d->display);
        d->display = nullptr;
    }
}

void X11InputForwarder::setTarget(unsigned long windowId, const QRect &targetRect)
{
    d->targetWindow = windowId;
    d->targetRect = targetRect;
}

static unsigned int qtButtonToX11(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton: return 1;
    case Qt::MiddleButton: return 2;
    case Qt::RightButton: return 3;
    default: return 1;
    }
}

void X11InputForwarder::sendMotion(const QPoint &globalPos)
{
    if (!d->display) return;
    XTestFakeMotionEvent(d->display, 0, globalPos.x(), globalPos.y(), CurrentTime);
    XFlush(d->display);
}

void X11InputForwarder::sendButton(Qt::MouseButton button, bool pressed, const QPoint &globalPos)
{
    if (!d->display) return;
    XTestFakeMotionEvent(d->display, 0, globalPos.x(), globalPos.y(), CurrentTime);
    unsigned int xbtn = qtButtonToX11(button);
    XTestFakeButtonEvent(d->display, xbtn, pressed ? True : False, CurrentTime);
    XFlush(d->display);
}

void X11InputForwarder::sendClick(Qt::MouseButton button, const QPoint &globalPos)
{
    if (!d->display) return;
    XTestFakeMotionEvent(d->display, 0, globalPos.x(), globalPos.y(), CurrentTime);
    unsigned int xbtn = qtButtonToX11(button);
    XTestFakeButtonEvent(d->display, xbtn, True, CurrentTime);
    XTestFakeButtonEvent(d->display, xbtn, False, CurrentTime);
    XFlush(d->display);
}

void X11InputForwarder::sendWheel(int deltaY, const QPoint &globalPos)
{
    if (!d->display) return;
    XTestFakeMotionEvent(d->display, 0, globalPos.x(), globalPos.y(), CurrentTime);
    // Button 4 is Wheel Up, Button 5 is Wheel Down
    unsigned int xbtn = (deltaY > 0) ? 4 : 5;
    int steps = std::max(1, std::abs(deltaY) / 120);
    for (int i = 0; i < steps; ++i) {
        XTestFakeButtonEvent(d->display, xbtn, True, CurrentTime);
        XTestFakeButtonEvent(d->display, xbtn, False, CurrentTime);
    }
    XFlush(d->display);
}

void X11InputForwarder::sendTouchDown(const QPoint &globalPos)
{
    if (!d->display) return;
    XTestFakeMotionEvent(d->display, 0, globalPos.x(), globalPos.y(), CurrentTime);
    XTestFakeButtonEvent(d->display, 1, True, CurrentTime); // Left click press
    XFlush(d->display);
}

void X11InputForwarder::sendTouchMotion(const QPoint &globalPos)
{
    if (!d->display) return;
    XTestFakeMotionEvent(d->display, 0, globalPos.x(), globalPos.y(), CurrentTime);
    XFlush(d->display);
}

void X11InputForwarder::sendTouchUp(const QPoint &globalPos)
{
    if (!d->display) return;
    XTestFakeMotionEvent(d->display, 0, globalPos.x(), globalPos.y(), CurrentTime);
    XTestFakeButtonEvent(d->display, 1, False, CurrentTime); // Left click release
    XFlush(d->display);
}
