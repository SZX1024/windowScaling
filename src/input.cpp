#include "input.h"

#include <QDebug>
#include <cstring>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

struct X11InputForwarder::Impl {
    Display *display = nullptr;
    Window root = None;
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

    d->root = DefaultRootWindow(d->display);
    return true;
}

void X11InputForwarder::close()
{
    releaseTarget();
    if (d->display) {
        XCloseDisplay(d->display);
        d->display = nullptr;
    }
}

void X11InputForwarder::setWindowBelow(unsigned long windowId, bool below)
{
    if (!d->display || windowId == None || d->root == None) {
        return;
    }

    Atom netWmState = XInternAtom(d->display, "_NET_WM_STATE", False);
    Atom netWmStateBelow = XInternAtom(d->display, "_NET_WM_STATE_BELOW", False);

    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = windowId;
    ev.xclient.message_type = netWmState;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = below ? 1 : 0; // 1 = _NET_WM_STATE_ADD, 0 = _NET_WM_STATE_REMOVE
    ev.xclient.data.l[1] = netWmStateBelow;
    ev.xclient.data.l[2] = 0;
    ev.xclient.data.l[3] = 1; // 1 = regular application

    XSendEvent(d->display, d->root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(d->display);
}

void X11InputForwarder::setTarget(unsigned long windowId, const QRect &targetRect)
{
    d->targetWindow = windowId;
    d->targetRect = targetRect;

    if (d->display && windowId != None) {
        // Pin target window below so it can never pop up over the overlay
        setWindowBelow(windowId, true);

        // Keep input focus on game window so animations never pause
        XSetInputFocus(d->display, windowId, RevertToPointerRoot, CurrentTime);
        XFlush(d->display);
    }
}

void X11InputForwarder::releaseTarget()
{
    if (d->display && d->targetWindow != None) {
        setWindowBelow(d->targetWindow, false);
        d->targetWindow = None;
    }
}

void X11InputForwarder::restackOverlayAbove(unsigned long overlayWinId)
{
    if (!d->display || overlayWinId == 0 || d->targetWindow == None) {
        return;
    }

    Window wins[2] = { (Window)overlayWinId, d->targetWindow };
    XRestackWindows(d->display, wins, 2);
    XFlush(d->display);
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

void X11InputForwarder::sendMotion(const QPoint &localPos)
{
    if (!d->display || d->targetWindow == None) return;

    XMotionEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = MotionNotify;
    ev.same_screen = True;
    ev.subwindow = None;
    ev.time = CurrentTime;
    ev.display = d->display;
    ev.window = d->targetWindow;
    ev.root = d->root;
    ev.x = localPos.x();
    ev.y = localPos.y();
    ev.x_root = d->targetRect.x() + localPos.x();
    ev.y_root = d->targetRect.y() + localPos.y();
    ev.state = 0;

    XSendEvent(d->display, d->targetWindow, False, PointerMotionMask, reinterpret_cast<XEvent*>(&ev));
    XFlush(d->display);
}

void X11InputForwarder::sendButton(Qt::MouseButton button, bool pressed, const QPoint &localPos)
{
    if (!d->display || d->targetWindow == None) return;

    // Send motion first to update target internal position
    sendMotion(localPos);

    XButtonEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = pressed ? ButtonPress : ButtonRelease;
    ev.button = qtButtonToX11(button);
    ev.same_screen = True;
    ev.subwindow = None;
    ev.time = CurrentTime;
    ev.display = d->display;
    ev.window = d->targetWindow;
    ev.root = d->root;
    ev.x = localPos.x();
    ev.y = localPos.y();
    ev.x_root = d->targetRect.x() + localPos.x();
    ev.y_root = d->targetRect.y() + localPos.y();
    ev.state = pressed ? 0 : (1 << (7 + ev.button));

    long mask = pressed ? ButtonPressMask : ButtonReleaseMask;
    XSendEvent(d->display, d->targetWindow, False, mask, reinterpret_cast<XEvent*>(&ev));
    XFlush(d->display);
}

void X11InputForwarder::sendClick(Qt::MouseButton button, const QPoint &localPos)
{
    sendButton(button, true, localPos);
    sendButton(button, false, localPos);
}

void X11InputForwarder::sendWheel(int deltaY, const QPoint &localPos)
{
    if (!d->display || d->targetWindow == None) return;

    sendMotion(localPos);

    unsigned int xbtn = (deltaY > 0) ? 4 : 5; // 4=Up, 5=Down
    int steps = std::max(1, std::abs(deltaY) / 120);

    for (int i = 0; i < steps; ++i) {
        XButtonEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.same_screen = True;
        ev.subwindow = None;
        ev.time = CurrentTime;
        ev.display = d->display;
        ev.window = d->targetWindow;
        ev.root = d->root;
        ev.x = localPos.x();
        ev.y = localPos.y();
        ev.x_root = d->targetRect.x() + localPos.x();
        ev.y_root = d->targetRect.y() + localPos.y();

        ev.type = ButtonPress;
        ev.button = xbtn;
        XSendEvent(d->display, d->targetWindow, False, ButtonPressMask, reinterpret_cast<XEvent*>(&ev));

        ev.type = ButtonRelease;
        XSendEvent(d->display, d->targetWindow, False, ButtonReleaseMask, reinterpret_cast<XEvent*>(&ev));
    }
    XFlush(d->display);
}

void X11InputForwarder::sendTouchDown(const QPoint &localPos)
{
    sendButton(Qt::LeftButton, true, localPos);
}

void X11InputForwarder::sendTouchMotion(const QPoint &localPos)
{
    sendMotion(localPos);
}

void X11InputForwarder::sendTouchUp(const QPoint &localPos)
{
    sendButton(Qt::LeftButton, false, localPos);
}
