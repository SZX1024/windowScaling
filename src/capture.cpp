
#include "capture.h"

#include <QDebug>
#include <cstdlib>
#include <cstring>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/XShm.h>
#include <X11/Xutil.h>

static int silentX11ErrorHandler(Display *disp, XErrorEvent *err)
{
    Q_UNUSED(disp);
    Q_UNUSED(err);
    return 0;
}

struct X11CaptureEngine::Impl {
    Display *display = nullptr;
    Window root = None;
    Window target = None;
    Pixmap pixmap = None;
    QRect targetGeometry;

    XShmSegmentInfo shmInfo{};
    XImage *shmImage = nullptr;
    bool shmAttached = false;

    void cleanupShm() {
        if (shmAttached && display) {
            XShmDetach(display, &shmInfo);
            shmAttached = false;
        }
        if (shmImage) {
            shmImage->f.destroy_image(shmImage);
            shmImage = nullptr;
        }
        if (shmInfo.shmaddr) {
            shmdt(shmInfo.shmaddr);
            shmInfo.shmaddr = nullptr;
        }
        if (shmInfo.shmid > 0) {
            shmctl(shmInfo.shmid, IPC_RMID, nullptr);
            shmInfo.shmid = -1;
        }
    }
};

X11CaptureEngine::X11CaptureEngine()
    : d(std::make_unique<Impl>())
{
}

X11CaptureEngine::~X11CaptureEngine()
{
    close();
}

bool X11CaptureEngine::open(const char *displayName)
{
    if (d->display) {
        return true;
    }

    // Ensure XAUTHORITY is set from /run/user/1000 if not present
    if (!getenv("XAUTHORITY")) {
        // Try finding xauth file in /run/user/1000/
        FILE *fp = popen("ls -t /run/user/1000/xauth_* 2>/dev/null | head -n 1", "r");
        if (fp) {
            char path[512] = {0};
            if (fgets(path, sizeof(path) - 1, fp)) {
                // strip newline
                char *nl = strchr(path, '\n');
                if (nl) *nl = 0;
                if (path[0]) {
                    setenv("XAUTHORITY", path, 1);
                }
            }
            pclose(fp);
        }
    }

    XSetErrorHandler(silentX11ErrorHandler);
    d->display = XOpenDisplay(displayName);
    if (!d->display) {
        qWarning() << "X11CaptureEngine: Failed to open X11 display:" << displayName;
        return false;
    }

    d->root = DefaultRootWindow(d->display);

    int eventBase, errorBase;
    if (!XCompositeQueryExtension(d->display, &eventBase, &errorBase)) {
        qWarning() << "X11CaptureEngine: XComposite extension not supported!";
        close();
        return false;
    }

    if (!XShmQueryExtension(d->display)) {
        qWarning() << "X11CaptureEngine: MIT-SHM extension not supported!";
        close();
        return false;
    }

    return true;
}

void X11CaptureEngine::close()
{
    releaseTarget();
    if (d->display) {
        XCloseDisplay(d->display);
        d->display = nullptr;
        d->root = None;
    }
}

std::vector<XWindowItem> X11CaptureEngine::listWindows()
{
    std::vector<XWindowItem> result;
    if (!d->display || d->root == None) {
        return result;
    }

    Atom netClientList = XInternAtom(d->display, "_NET_CLIENT_LIST", True);
    if (netClientList == None) {
        return result;
    }

    Atom actualType;
    int actualFormat;
    unsigned long nItems = 0, bytesAfter = 0;
    unsigned char *prop = nullptr;

    if (XGetWindowProperty(d->display, d->root, netClientList, 0, 4096, False, XA_WINDOW,
                           &actualType, &actualFormat, &nItems, &bytesAfter, &prop) == Success && prop) {
        Window *windows = reinterpret_cast<Window*>(prop);
        for (unsigned long i = 0; i < nItems; ++i) {
            Window win = windows[i];
            char *name = nullptr;
            XFetchName(d->display, win, &name);

            XWindowAttributes attrs;
            if (XGetWindowAttributes(d->display, win, &attrs)) {
                XWindowItem item;
                item.id = win;
                item.title = name ? QString::fromUtf8(name) : QString();
                item.winClass = getWindowClass(win);
                item.x = attrs.x;
                item.y = attrs.y;
                item.width = attrs.width;
                item.height = attrs.height;
                item.isViewable = (attrs.map_state == IsViewable);
                result.push_back(item);
            }
            if (name) XFree(name);
        }
        XFree(prop);
    }
    return result;
}

QString X11CaptureEngine::getWindowClass(unsigned long windowId)
{
    if (!d->display || windowId == 0) return QString();
    XClassHint hint;
    QString result;
    if (XGetClassHint(d->display, (Window)windowId, &hint)) {
        if (hint.res_class) {
            result = QString::fromUtf8(hint.res_class);
            XFree(hint.res_class);
        }
        if (hint.res_name) {
            XFree(hint.res_name);
        }
    }
    return result;
}

QString X11CaptureEngine::getWindowTitle(unsigned long windowId)
{
    if (!d->display || windowId == 0) return QString();
    char *name = nullptr;
    XFetchName(d->display, (Window)windowId, &name);
    QString result = name ? QString::fromUtf8(name) : QString();
    if (name) XFree(name);
    return result;
}

unsigned long X11CaptureEngine::getActiveWindow()
{
    if (!d->display || d->root == None) {
        return 0;
    }

    Atom netActiveWindow = XInternAtom(d->display, "_NET_ACTIVE_WINDOW", True);
    unsigned long activeWin = 0;

    if (netActiveWindow != None) {
        Atom actualType;
        int actualFormat;
        unsigned long nItems = 0, bytesAfter = 0;
        unsigned char *prop = nullptr;

        if (XGetWindowProperty(d->display, d->root, netActiveWindow, 0, 1, False, XA_WINDOW,
                               &actualType, &actualFormat, &nItems, &bytesAfter, &prop) == Success && prop) {
            if (nItems > 0) {
                activeWin = *reinterpret_cast<Window*>(prop);
            }
            XFree(prop);
        }
    }

    // Fallback: XGetInputFocus
    if (activeWin == 0) {
        Window focusWin = None;
        int revertTo = 0;
        XGetInputFocus(d->display, &focusWin, &revertTo);
        if (focusWin != None && focusWin != d->root && focusWin != PointerRoot) {
            activeWin = focusWin;
        }
    }

    return activeWin;
}

unsigned long X11CaptureEngine::findWindowByTitle(const QString &titleSubstring)
{
    auto list = listWindows();
    for (const auto &item : list) {
        if (item.isViewable && item.title.contains(titleSubstring, Qt::CaseInsensitive)) {
            return item.id;
        }
    }
    return 0;
}

bool X11CaptureEngine::setTargetWindow(unsigned long windowId)
{
    if (!d->display || windowId == 0) {
        return false;
    }

    releaseTarget();

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(d->display, windowId, &attrs)) {
        qWarning() << "X11CaptureEngine: Cannot get attributes for window" << Qt::hex << windowId;
        return false;
    }

    d->target = windowId;
    m_target = windowId;
    int rootX = attrs.x, rootY = attrs.y;
    Window childWin;
    if (d->display && d->root != None) {
        XTranslateCoordinates(d->display, d->target, d->root, 0, 0, &rootX, &rootY, &childWin);
    }
    d->targetGeometry = QRect(rootX, rootY, attrs.width, attrs.height);

    // Redirect window offscreen
    XCompositeRedirectWindow(d->display, d->target, CompositeRedirectAutomatic);
    d->pixmap = XCompositeNameWindowPixmap(d->display, d->target);

    // If top-level has no pixmap, check children (common in Wine/Proton games)
    if (d->pixmap == None) {
        Window rootRet, parentRet;
        Window *children = nullptr;
        unsigned int nchildren = 0;
        if (XQueryTree(d->display, d->target, &rootRet, &parentRet, &children, &nchildren) && children) {
            for (unsigned int i = 0; i < nchildren; ++i) {
                XWindowAttributes childAttrs;
                if (XGetWindowAttributes(d->display, children[i], &childAttrs) && childAttrs.map_state == IsViewable) {
                    XCompositeRedirectWindow(d->display, children[i], CompositeRedirectAutomatic);
                    Pixmap p = XCompositeNameWindowPixmap(d->display, children[i]);
                    if (p != None) {
                        d->target = children[i];
                        m_target = children[i];
                        d->pixmap = p;
                        d->targetGeometry = QRect(childAttrs.x, childAttrs.y, childAttrs.width, childAttrs.height);
                        attrs = childAttrs;
                        break;
                    }
                }
            }
            XFree(children);
        }
    }

    if (d->pixmap == None) {
        qWarning() << "X11CaptureEngine: XCompositeNameWindowPixmap failed for window" << Qt::hex << windowId;
        releaseTarget();
        return false;
    }

    if (attrs.width <= 0 || attrs.height <= 0) {
        releaseTarget();
        return false;
    }

    // Allocate MIT-SHM image
    d->shmImage = XShmCreateImage(d->display, attrs.visual, attrs.depth, ZPixmap, nullptr, &d->shmInfo, attrs.width, attrs.height);
    if (!d->shmImage) {
        qWarning() << "X11CaptureEngine: XShmCreateImage failed";
        releaseTarget();
        return false;
    }

    d->shmInfo.shmid = shmget(IPC_PRIVATE, d->shmImage->bytes_per_line * d->shmImage->height, IPC_CREAT | 0777);
    if (d->shmInfo.shmid < 0) {
        qWarning() << "X11CaptureEngine: shmget failed";
        releaseTarget();
        return false;
    }

    d->shmInfo.shmaddr = d->shmImage->data = (char*)shmat(d->shmInfo.shmid, nullptr, 0);
    d->shmInfo.readOnly = False;
    if (!XShmAttach(d->display, &d->shmInfo)) {
        qWarning() << "X11CaptureEngine: XShmAttach failed";
        releaseTarget();
        return false;
    }
    d->shmAttached = true;

    return true;
}

void X11CaptureEngine::releaseTarget()
{
    d->cleanupShm();
    if (d->pixmap != None && d->display) {
        XFreePixmap(d->display, d->pixmap);
        d->pixmap = None;
    }
    if (d->target != None && d->display) {
        XCompositeUnredirectWindow(d->display, d->target, CompositeRedirectAutomatic);
        d->target = None;
    }
    m_target = 0;
    d->targetGeometry = QRect();
}

QImage X11CaptureEngine::captureFrame()
{
    if (!d->display || d->pixmap == None || !d->shmImage || !d->shmAttached) {
        return QImage();
    }

    // Update geometry if needed
    XWindowAttributes attrs;
    if (XGetWindowAttributes(d->display, d->target, &attrs)) {
        int rootX = attrs.x, rootY = attrs.y;
        Window childWin;
        if (d->display && d->root != None) {
            XTranslateCoordinates(d->display, d->target, d->root, 0, 0, &rootX, &rootY, &childWin);
        }
        d->targetGeometry = QRect(rootX, rootY, attrs.width, attrs.height);
        // If size changed, reallocate SHM
        if (attrs.width != d->shmImage->width || attrs.height != d->shmImage->height) {
            setTargetWindow(d->target);
            if (!d->shmImage) return QImage();
        }
    }

    // Fast zero-copy shared memory pull
    if (!XShmGetImage(d->display, d->pixmap, d->shmImage, 0, 0, AllPlanes)) {
        return QImage();
    }

    // Wrap in QImage without copying pixels
    return QImage(reinterpret_cast<const uchar*>(d->shmImage->data),
                  d->shmImage->width,
                  d->shmImage->height,
                  d->shmImage->bytes_per_line,
                  QImage::Format_RGB32);
}

QRect X11CaptureEngine::targetRect() const
{
    return d->targetGeometry;
}
bool X11CaptureEngine::isTargetAlive()
{
    if (!d->display || d->target == None) {
        return false;
    }

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(d->display, d->target, &attrs)) {
        return false;
    }

    return (attrs.map_state == IsViewable && attrs.width > 0 && attrs.height > 0);
}