/**
 * Extension tokens and function definitions
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
 */
#ifndef EXT_H
#define EXT_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_KHR_lock_surface
#define EGL_KHR_lock_surface 1
#define EGL_READ_SURFACE_BIT_KHR                0x0001  /* EGL_LOCK_USAGE_HINT_KHR bitfield */
#define EGL_WRITE_SURFACE_BIT_KHR               0x0002  /* EGL_LOCK_USAGE_HINT_KHR bitfield */
#define EGL_LOCK_SURFACE_BIT_KHR                0x0080  /* EGL_SURFACE_TYPE bitfield */
#define EGL_OPTIMAL_FORMAT_BIT_KHR              0x0100  /* EGL_SURFACE_TYPE bitfield */
#define EGL_MATCH_FORMAT_KHR                    0x3043  /* EGLConfig attribute */
#define EGL_FORMAT_RGB_565_EXACT_KHR            0x30C0  /* EGL_MATCH_FORMAT_KHR value */
#define EGL_FORMAT_RGB_565_KHR                  0x30C1  /* EGL_MATCH_FORMAT_KHR value */
#define EGL_FORMAT_RGBA_8888_EXACT_KHR          0x30C2  /* EGL_MATCH_FORMAT_KHR value */
#define EGL_FORMAT_RGBA_8888_KHR                0x30C3  /* EGL_MATCH_FORMAT_KHR value */
#define EGL_MAP_PRESERVE_PIXELS_KHR             0x30C4  /* eglLockSurfaceKHR attribute */
#define EGL_LOCK_USAGE_HINT_KHR                 0x30C5  /* eglLockSurfaceKHR attribute */
#define EGL_BITMAP_POINTER_KHR                  0x30C6  /* eglQuerySurface attribute */
#define EGL_BITMAP_PITCH_KHR                    0x30C7  /* eglQuerySurface attribute */
#define EGL_BITMAP_ORIGIN_KHR                   0x30C8  /* eglQuerySurface attribute */
#define EGL_BITMAP_PIXEL_RED_OFFSET_KHR         0x30C9  /* eglQuerySurface attribute */
#define EGL_BITMAP_PIXEL_GREEN_OFFSET_KHR       0x30CA  /* eglQuerySurface attribute */
#define EGL_BITMAP_PIXEL_BLUE_OFFSET_KHR        0x30CB  /* eglQuerySurface attribute */
#define EGL_BITMAP_PIXEL_ALPHA_OFFSET_KHR       0x30CC  /* eglQuerySurface attribute */
#define EGL_BITMAP_PIXEL_LUMINANCE_OFFSET_KHR   0x30CD  /* eglQuerySurface attribute */
#define EGL_LOWER_LEFT_KHR                      0x30CE  /* EGL_BITMAP_ORIGIN_KHR value */
#define EGL_UPPER_LEFT_KHR                      0x30CF  /* EGL_BITMAP_ORIGIN_KHR value */

typedef EGLBoolean (EGLAPIENTRYP PFNEGLLOCKSURFACEKHRPROC) (EGLDisplay display, EGLSurface surface, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLUNLOCKSURFACEKHRPROC) (EGLDisplay display, EGLSurface surface);
#endif

#ifndef EGL_KHR_lock_surface2
#define EGL_KHR_lock_surface2 1
#define EGL_BITMAP_PIXEL_SIZE_KHR               0x3110
#endif

#ifndef EGL_NOK_image_yuv
#define EGL_NOK_image_yuv 1
#define EGL_IMAGE_FORMAT_FOURCC_NOK             0x30DE
#define EGL_YUV_CONVERSION_TYPE_NOK             0x30DF
#define EGL_YUV_CONVERSION_BT601_NOK            0x3150
#define EGL_YUV_CONVERSION_BT601_FULL_NOK       0x3151
#define EGL_YUV_CONVERSION_BT709_NOK            0x3152
#define EGL_YUV_CONVERSION_BT709_FULL_NOK       0x3153
#endif

#ifndef EGL_NOK_image_shared
#define EGL_NOK_image_shared 1
typedef XID EGLNativeSharedImageTypeNOK;
#define EGL_SHARED_IMAGE_NOK                    0x30DA
typedef EGLNativeSharedImageTypeNOK (EGLAPIENTRYP PFNEGLCREATESHAREDIMAGENOKPROC) (EGLDisplay dpy, EGLImageKHR image, const EGLint *attr_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYSHAREDIMAGENOKPROC) (EGLDisplay dpy, EGLNativeSharedImageTypeNOK image);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYIMAGENOKPROC) (EGLDisplay dpy, EGLImageKHR image, EGLint attribute, EGLint* value);
#endif

#ifndef EGL_NOK_swap_region2
#define EGL_NOK_swap_region2
typedef EGLBoolean (*PFNEGLSWAPBUFFERSREGION2NOKPROC)(EGLDisplay, EGLSurface, EGLint, const EGLint*);
#endif

#ifndef EGL_NOK_image_framebuffer
#define EGL_NOK_image_framebuffer 1
#define EGL_FRAMEBUFFER_DEVICE_NOK              0x3154
#define EGL_FRAMEBUFFER_X_OFFSET_NOK            0x3155
#define EGL_FRAMEBUFFER_Y_OFFSET_NOK            0x3156
#define EGL_FRAMEBUFFER_WIDTH_NOK               0x3157
#define EGL_FRAMEBUFFER_HEIGHT_NOK              0x3158
#endif

#ifndef EGL_KHR_reusable_sync
#define EGL_KHR_reusable_sync 1

typedef void* EGLSyncKHR;
typedef khronos_utime_nanoseconds_t EGLTimeKHR;

#define EGL_SYNC_STATUS_KHR                     0x30F1
#define EGL_SIGNALED_KHR                        0x30F2
#define EGL_UNSIGNALED_KHR                      0x30F3
#define EGL_TIMEOUT_EXPIRED_KHR                 0x30F5
#define EGL_CONDITION_SATISFIED_KHR             0x30F6
#define EGL_SYNC_TYPE_KHR                       0x30F7
#define EGL_SYNC_REUSABLE_KHR                   0x30FA
#define EGL_SYNC_FLUSH_COMMANDS_BIT_KHR         0x0001  /* eglClientWaitSyncKHR <flags> bitfield */
#define EGL_FOREVER_KHR                         0xFFFFFFFFFFFFFFFFull
#define EGL_NO_SYNC_KHR                         ((EGLSyncKHR)0)
typedef EGLSyncKHR (EGLAPIENTRYP PFNEGLCREATESYNCKHRPROC) (EGLDisplay dpy, EGLenum type, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync);
typedef EGLint (EGLAPIENTRYP PFNEGLCLIENTWAITSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSIGNALSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETSYNCATTRIBKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value);
#endif

#ifndef EGL_NOK_surface_scaling
#define EGL_NOK_surface_scaling 1
#define EGL_SURFACE_SCALING_NOK         0x30DD

#define EGL_FIXED_WIDTH_NOK	            0x30DB
#define EGL_FIXED_HEIGHT_NOK            0x30DC
#define EGL_TARGET_EXTENT_OFFSET_X_NOK  0x3079
#define EGL_TARGET_EXTENT_OFFSET_Y_NOK  0x307A
#define EGL_TARGET_EXTENT_WIDTH_NOK     0x307B
#define EGL_TARGET_EXTENT_HEIGHT_NOK    0x307C
#define EGL_BORDER_COLOR_RED_NOK        0x307D
#define EGL_BORDER_COLOR_GREEN_NOK      0x307E
#define EGL_BORDER_COLOR_BLUE_NOK       0x30D8

#define EGL_NOT_SUPPORTED_NOK           0
#define EGL_SUPPORTED_NOK               1
#define EGL_SLOW_NOK                    3

typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSURFACESCALINGCAPABILITYNOKPROC) (EGLDisplay display, EGLConfig config,
                    EGLint surface_width, EGLint surface_height, EGLint target_width, EGLint target_height, EGLint *value);

typedef EGLBoolean (EGLAPIENTRYP PFNEGLSETSURFACESCALINGNOKPROC) (EGLDisplay display, EGLSurface surface,
                    EGLint target_offset_x, EGLint target_offset_y, EGLint target_width, EGLint target_height);
#endif

#ifndef EGL_KHR_fence_sync
#define EGL_KHR_fence_sync 1
#define EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR    0x30F0
#define EGL_SYNC_CONDITION_KHR                  0x30F8
#define EGL_SYNC_FENCE_KHR                      0x30F9
#endif

#endif // EXT_H
