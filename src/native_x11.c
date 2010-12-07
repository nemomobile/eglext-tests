/**
 * Native windowing implementation for X11
 * Copyright (C) 2010 Nokia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * \author Sami Kyöstilä <sami.kyostila@nokia.com>
 *
 */
#include <EGL/egl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>

#include "native.h"

EGLBoolean nativeCreateDisplay(EGLNativeDisplayType *pNativeDisplay)
{
    *pNativeDisplay = XOpenDisplay(NULL);

    if (!*pNativeDisplay)
    {
        fprintf(stderr, "XOpenDisplay failed\n");
        return EGL_FALSE;
    }

    return EGL_TRUE;
}

void nativeDestroyDisplay(EGLNativeDisplayType nativeDisplay)
{
    XCloseDisplay(nativeDisplay);
}

static int runningOnFremantle(void)
{
    /* Somewhat hacky way of detecting fremantle */
    FILE* f = popen("pgrep hildon-desktop", "r");
    int found = 0;

    while (!feof(f))
    {
        int pid;
        if (fscanf(f, "%d", &pid) == 1)
        {
            found = 1;
        }
    }
    pclose(f);
    return found;
}

static int lastError = 0;

int errorHandler(Display *dpy, XErrorEvent *event)
{
    (void)dpy;
    lastError = event->error_code;
    return 0;
}

int isWindowRedirected(Display *d, Window window)
{
    XErrorHandler prevHandler;

    /*
     *  Detect window composition by requesting the redirected pixmap name. If
     *  the window is not redirected, then this will trigger a BadAccess error.
     */
    XLockDisplay(d);
    lastError = 0;
    XSync(d, 0);
    prevHandler = XSetErrorHandler(errorHandler);
    (void)XCompositeNameWindowPixmap(d, window);
    XSync(d, 0);
    XSetErrorHandler(prevHandler);
    XUnlockDisplay(d);

    return (lastError == 0);
}

static Bool waitForNotify(Display *d, XEvent *e, char *arg)
{
    return (e->type == FocusIn) && (e->xfocus.window == (Window)arg);
}

static void waitUntilWindowIsFocused(Display *d, Window window)
{
    int timeout = 30;
    XEvent event;
    XSelectInput(d, window, FocusChangeMask);
    while (timeout-- &&
           !XCheckIfEvent(d, &event, waitForNotify, (char*) window))
    {
        usleep(100 * 1000);
    }
}

EGLBoolean nativeCreateWindow(EGLNativeDisplayType nativeDisplay, EGLDisplay dpy, EGLConfig config, 
                              const char *title, int width, int height, EGLNativeWindowType *nativeWindow)
{
    Window rootWindow = DefaultRootWindow(nativeDisplay);
    EGLint visualId;
    Window window;
    XVisualInfo* visual;
    XTextProperty windowTitle;
    XSetWindowAttributes winAttrs;
    XWindowAttributes rootAttrs;
    XVisualInfo visualInfo;
    int visualCount = 0;
    int fremantle = runningOnFremantle();

    /* Avoid window manager animations that can interfere with tests */
    Atom windowType = XInternAtom(nativeDisplay, "_NET_WM_WINDOW_TYPE", False);
    Atom windowTypeOverride = XInternAtom(nativeDisplay, "_KDE_NET_WM_WINDOW_TYPE_OVERRIDE", False);
    Atom windowTypeDialog = XInternAtom(nativeDisplay, "_NET_WM_WINDOW_TYPE_DIALOG", False);

    if (eglGetConfigAttrib(dpy, config, EGL_NATIVE_VISUAL_ID, &visualId) != EGL_TRUE)
    {
        fprintf(stderr, "eglGetConfigAttrib failed: %x\n", eglGetError());
        return EGL_FALSE;
    }

    visualInfo.visualid = visualId;
    visualInfo.screen = DefaultScreen(nativeDisplay);
    visual = XGetVisualInfo(nativeDisplay, VisualIDMask | VisualScreenMask, &visualInfo, &visualCount);

    if (!visualCount)
    {
        fprintf(stderr, "XGetVisualInfo failed\n");
        return EGL_FALSE;
    }

    XGetWindowAttributes(nativeDisplay, rootWindow, &rootAttrs);

    winAttrs.background_pixmap = None;
    winAttrs.border_pixel = 0;
    winAttrs.colormap = XCreateColormap(nativeDisplay, rootWindow, visual->visual, AllocNone);

    window = XCreateWindow(nativeDisplay, rootWindow, 0, 0,
                           width, height, 0, visual->depth,
                           InputOutput, visual->visual,
                           CWBackPixmap | CWBorderPixel | CWColormap,
                           &winAttrs);

    if (!window)
    {
        fprintf(stderr, "XCreateSimpleWindow failed\n");
        return EGL_FALSE;
    }

    /*
     * Harmattan needs the dialog and override always. Logic is only fallback
     * for fremantle to avoid changing properties there.
     */
    if (!fremantle || XVisualIDFromVisual(rootAttrs.visual) == visualId) {
        int mode = PropModeReplace;

        if (!fremantle) {
            XChangeProperty(nativeDisplay, window, windowType, XA_ATOM, 32, mode, (unsigned char*)&windowTypeDialog, 1);
            mode = PropModeAppend;
        }

        XChangeProperty(nativeDisplay, window, windowType, XA_ATOM, 32, mode, (unsigned char*)&windowTypeOverride, 1);
    }

    windowTitle.value    = (unsigned char*)title;
    windowTitle.encoding = XA_STRING;
    windowTitle.format   = 8;
    windowTitle.nitems   = strlen(title);
    XSetWMName(nativeDisplay, window, &windowTitle);

    /*
     * Harmattan WM reads all Atoms in MapRequest so it is best to set
     * atoms before mapping. But in Fremantle that doesn't work (according
     * to Tero's comments)
     */
    if (fremantle) {
        XMapWindow(nativeDisplay, window);
        XFlush(nativeDisplay);
    }

    /* Set window to fullscreen mode if it matches the screen size */
    if (rootAttrs.width == width && rootAttrs.height == height)
    {
        XEvent xev;
        Atom wmState = XInternAtom(nativeDisplay, "_NET_WM_STATE", False);
        Atom wmStateFullscreen = XInternAtom(nativeDisplay, "_NET_WM_STATE_FULLSCREEN", False);

        memset(&xev, 0, sizeof(XEvent));
        xev.type = ClientMessage;
        xev.xclient.window = window;
        xev.xclient.message_type = wmState;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 1;
        xev.xclient.data.l[1] = wmStateFullscreen;
        xev.xclient.data.l[2] = 0;
        XSendEvent(nativeDisplay, rootWindow, False, SubstructureNotifyMask, &xev);
    }

    if (!fremantle) {
        XMapWindow(nativeDisplay, window);
        XFlush(nativeDisplay);
    }

    waitUntilWindowIsFocused(nativeDisplay, window);

    *nativeWindow = window;
    nativeVerifyWindow(nativeDisplay, *nativeWindow);

    return EGL_TRUE;
}

void nativeDestroyWindow(EGLNativeDisplayType nativeDisplay, EGLNativeWindowType nativeWindow)
{
    XDestroyWindow(nativeDisplay, nativeWindow);
}

EGLBoolean nativeCreatePixmap(EGLNativeDisplayType nativeDisplay, int depth,
                              int width, int height, EGLNativePixmapType *nativePixmap)
{
    Window rootWindow = DefaultRootWindow(nativeDisplay);
    Pixmap pixmap;

    pixmap = XCreatePixmap(nativeDisplay, rootWindow, width, height, depth);

    if (!pixmap)
    {
        fprintf(stderr, "XCreatePixmap failed\n");
        return EGL_FALSE;
    }

    XFlush(nativeDisplay);

    *nativePixmap = pixmap;

    return EGL_TRUE;
}

void nativeDestroyPixmap(EGLNativeDisplayType nativeDisplay, EGLNativePixmapType nativePixmap)
{
    XFreePixmap(nativeDisplay, nativePixmap);
}

EGLBoolean nativeGetDisplayProperties(EGLNativeDisplayType nativeDisplay, int *width,
                                      int *height, int *depth)
{
    XWindowAttributes rootAttrs;
    Window rootWindow = DefaultRootWindow(nativeDisplay);
    XGetWindowAttributes(nativeDisplay, rootWindow, &rootAttrs);

    *width = rootAttrs.width;
    *height = rootAttrs.height;
    *depth = rootAttrs.depth;

    return EGL_TRUE;
}

EGLBoolean nativeVerifyWindow(EGLNativeDisplayType nativeDisplay,
                              EGLNativeWindowType nativeWindow)
{
    static int compWarningShown = 0;
    if (!compWarningShown && isWindowRedirected(nativeDisplay, (Window)nativeWindow))
    {
        printf("Warning: using a composited window; results may not be reliable\n");
        compWarningShown = 1;
        return EGL_FALSE;
    }
    return EGL_TRUE;
}
