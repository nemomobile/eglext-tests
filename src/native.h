/**
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
 * Native windowing
 */
#include <EGL/egl.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 *  Create a native display
 *
 *  @param[out] nativeDisplay           Native display handle
 */
EGLBoolean nativeCreateDisplay(EGLNativeDisplayType *pNativeDisplay);

/**
 *  Get properties of a display
 *
 *  @param nativeDisplay                Native display handle
 *  @param[out] width                   Display width
 *  @param[out] height                  Display height
 *  @param[out] depth                   Display bits per pixel
 */
EGLBoolean nativeGetDisplayProperties(EGLNativeDisplayType nativeDisplay, int *width,
                                      int *height, int *depth);


/**
 *  Destroy a native display
 */
void nativeDestroyDisplay(EGLNativeDisplayType nativeDisplay);

/**
 *  Create a native window
 *
 *  @param nativeDisplay                Native display handle
 *  @param dpy                          EGL display handle
 *  @param config                       Configuration to be used with the window
 *  @param title                        Window title
 *  @param width                        Window width in pixels
 *  @param height                       Window height in pixels
 *  @param[out] nativeWindow            New window handle
 */
EGLBoolean nativeCreateWindow(EGLNativeDisplayType nativeDisplay, EGLDisplay dpy,
                              EGLConfig config, const char *title, int width,
                              int height, EGLNativeWindowType *nativeWindow);

/**
 *  Destroy a native window
 *
 *  @param nativeDisplay                Native display handle
 *  @param nativeWindow                 Window to destroy
 */
void nativeDestroyWindow(EGLNativeDisplayType nativeDisplay, EGLNativeWindowType nativeWindow);

/**
 *  Check that a native window is suitable for performance measurement
 *  purposes. For example, a window which is drawn through composition may have
 *  a performance overhead compared to a direct rendered window.
 *
 *  @param nativeDisplay                Native display handle
 *  @param nativeWindow                 Window to verify
 */
EGLBoolean nativeVerifyWindow(EGLNativeDisplayType nativeDisplay,
                              EGLNativeWindowType nativeWindow);

/**
 *  Create a native pixmap
 *
 *  @param nativeDisplay                Native display handle
 *  @param depth                        Color depth
 *  @param width                        Pixmap width in pixels
 *  @param height                       Pixmap height in pixels
 *  @param[out] nativePixmap            New pixmap handle
 */
EGLBoolean nativeCreatePixmap(EGLNativeDisplayType nativeDisplay,
                              int depth,
                              int width, int height, EGLNativePixmapType *nativePixmap);

/**
 *  Destroy a native pixmap
 */
void nativeDestroyPixmap(EGLNativeDisplayType nativeDisplay, EGLNativePixmapType nativePixmap);

/** Opaque native front buffer handle */
typedef void* NativeFrontBuffer;

#define NATIVE_FRONTBUFFER_READ_BIT     0x0001  /** Read access to front buffer */
#define NATIVE_FRONTBUFFER_WRITE_BIT    0x0002  /** Write access to front buffer */

/**
 *  Map the front buffer of a native window for CPU access
 *
 *  @param nativeDisplay                Native display handle
 *  @param nativeWindow                 Native window owning the front buffer
 *  @param flags                        Bitmask of requested access type (read,
 *                                      write or both)
 *  @param[out] pixels                  Pointer to pixel data
 *  @param[out] width                   Front buffer width
 *  @param[out] height                  Front buffer height
 *  @param[out] bitsPerPixel            Front buffer depth
 *  @param[out] stride                  Front buffer stride in bytes
 *  @param[out] fb                      Front buffer handle
 *
 */
EGLBoolean nativeMapFrontBuffer(EGLNativeDisplayType nativeDisplay,
                                EGLNativeWindowType nativeWindow,
                                int flags,
                                uint8_t **pixels, int *width, int *height,
                                int *bitsPerPixel, int *stride,
                                NativeFrontBuffer *fb);

/**
 *  Unmap a previously mapped front buffer
 *
 *  @param nativeDisplay                Native display handle
 *  @param fb                           Front buffer handle
 */
void nativeUnmapFrontBuffer(EGLNativeDisplayType nativeDisplay,
                            NativeFrontBuffer fb);

#if defined(__cplusplus)
}
#endif
