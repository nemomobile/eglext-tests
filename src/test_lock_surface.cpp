/**
 * EGL_lock_surface2 extension test
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "ext.h"
#include "native.h"
#include "util.h"
#include "testutil.h"

typedef EGLBoolean (EGLAPIENTRYP PFNEGLLOCKSURFACEKHRPROC) (EGLDisplay display, EGLSurface surface, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLUNLOCKSURFACEKHRPROC) (EGLDisplay display, EGLSurface surface);

// Lock surface functions
static PFNEGLLOCKSURFACEKHRPROC eglLockSurfaceKHR;
static PFNEGLUNLOCKSURFACEKHRPROC eglUnlockSurfaceKHR;

// EGLImage functions
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

int winWidth = 864;
int winHeight = 480;
int winDepth = 16;
static EGLDisplay dpy;
static EGLNativeDisplayType nativeDisplay;

/**
 * Verify that the needed extensions are present
 */
void testExtensionPresence()
{
    // Lock surface
    ASSERT(util::isEGLExtensionSupported("EGL_KHR_lock_surface"));
    ASSERT(util::isEGLExtensionSupported("EGL_KHR_lock_surface2"));

    eglLockSurfaceKHR =
        (PFNEGLLOCKSURFACEKHRPROC)eglGetProcAddress("eglLockSurfaceKHR");
    eglUnlockSurfaceKHR =
        (PFNEGLUNLOCKSURFACEKHRPROC)eglGetProcAddress("eglUnlockSurfaceKHR");

    ASSERT(eglLockSurfaceKHR);
    ASSERT(eglUnlockSurfaceKHR);

    // EGL image
    ASSERT(util::isEGLExtensionSupported("EGL_KHR_image_base"));
    ASSERT(util::isEGLExtensionSupported("EGL_KHR_image_pixmap"));

    eglCreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR =
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    ASSERT(eglCreateImageKHR);
    ASSERT(eglDestroyImageKHR);
    ASSERT(glEGLImageTargetTexture2DOES);
}

void fillSurface(EGLDisplay dpy, EGLSurface surface)
{
    EGLint width, height;
    EGLint redSize, greenSize, blueSize, alphaSize;
    EGLint redShift, greenShift, blueShift, alphaShift;
    EGLConfig config;
    void* pixels = 0;
    EGLint pitch;
    EGLint origin;
    EGLint pixelSize;

    eglQuerySurface(dpy, surface, EGL_CONFIG_ID, (EGLint*)&config); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_WIDTH, &width); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &height); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_BITMAP_POINTER_KHR, (EGLint*)&pixels); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_BITMAP_PITCH_KHR, &pitch); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_BITMAP_PIXEL_RED_OFFSET_KHR, &redShift); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_BITMAP_PIXEL_GREEN_OFFSET_KHR, &greenShift); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_BITMAP_PIXEL_BLUE_OFFSET_KHR, &blueShift); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_BITMAP_PIXEL_ALPHA_OFFSET_KHR, &alphaShift); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_BITMAP_ORIGIN_KHR, &origin); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_BITMAP_PIXEL_SIZE_KHR, &pixelSize); ASSERT_EGL();

    eglGetConfigAttrib(dpy, config, EGL_RED_SIZE, &redSize); ASSERT_EGL();
    eglGetConfigAttrib(dpy, config, EGL_GREEN_SIZE, &greenSize); ASSERT_EGL();
    eglGetConfigAttrib(dpy, config, EGL_BLUE_SIZE, &blueSize); ASSERT_EGL();
    eglGetConfigAttrib(dpy, config, EGL_ALPHA_SIZE, &alphaSize); ASSERT_EGL();

    bool originAtTop = (origin == EGL_UPPER_LEFT_KHR);

    switch (pixelSize)
    {
    case 16:
        test::drawTestPattern<uint16_t>(reinterpret_cast<uint16_t*>(pixels),
                width, height, pitch, redSize, greenSize, blueSize, alphaSize,
                redShift, greenShift, blueShift, alphaShift, originAtTop);
       break;
    case 32:
        test::drawTestPattern<uint32_t>(reinterpret_cast<uint32_t*>(pixels),
                width, height, pitch, redSize, greenSize, blueSize, alphaSize,
                redShift, greenShift, blueShift, alphaShift, originAtTop);
        break;
    default:
        test::fail("Unsupported color depth %d\n", pixelSize);
    }
}

void checkSurface(EGLDisplay dpy, EGLSurface surface)
{
    test::scoped<EGLContext> context(boost::bind(eglDestroySurface, dpy, _1));
    EGLint width, height;
    EGLConfig config;
    uint8_t pixel[4];

    const EGLint contextAttrs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    eglQuerySurface(dpy, surface, EGL_CONFIG_ID, (EGLint*)&config); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_WIDTH, &width); ASSERT_EGL();
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &height); ASSERT_EGL();

    context = eglCreateContext(dpy, config, EGL_NO_CONTEXT, contextAttrs);
    ASSERT_EGL();

    if (!context)
    {
        test::fail("Unable to create a context\n");
    }

    eglMakeCurrent(dpy, surface, surface, context);
    ASSERT_EGL();
    ASSERT_GL();

    for (int y = 0; y < height; y += height / 2 + 1)
    {
        for (int x = 0; x < width; x += width / 4 + 1)
        {
            int px = x + width / 8;
            int py = y + height / 4;
            glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            ASSERT_GL();

            int r = 0, g = 0, b = 0, a = 0xff;
            switch (4 * px / width)
            {
            case 0:
                r = g = b = 0xff;
                break;
            case 1:
                r = 0xff;
                break;
            case 2:
                g = 0xff;
                break;
            case 3:
                b = 0xff;
                break;
            }

            // OpenGL's origin is at the bottom left
            if (py < height / 2)
            {
                r >>= 1;
                g >>= 1;
                b >>= 1;
            }

            int t = 8;
            if (abs(r - pixel[0]) > t ||
                abs(g - pixel[1]) > t ||
                abs(b - pixel[2]) > t ||
                abs(a - pixel[3]) > t)
            {
                test::fail("Image comparison failed at (%d, %d). "
                           "Expecting (%d,%d,%d,%d), got (%d,%d,%d,%d)\n",
                           px, py, r, g, b, a,
                           pixel[0], pixel[1], pixel[2], pixel[3]);
            }
        }
    }
}

void checkFrontBuffer(EGLNativeWindowType win)
{
    int width, height;
    test::scoped<NativeFrontBuffer> fb(boost::bind(nativeUnmapFrontBuffer,
                util::ctx.nativeDisplay, _1));
    int fbStride, fbBits;
    uint8_t* fbPixels = 0;

    eglWaitClient();
    eglWaitNative(EGL_CORE_NATIVE_ENGINE);

    if (!nativeMapFrontBuffer(util::ctx.nativeDisplay, win,
                              NATIVE_FRONTBUFFER_READ_BIT,
                              &fbPixels, &width, &height, &fbBits,
                              &fbStride, &fb))
    {
        test::fail("Unable to read front buffer");
    }

    for (int y = 0; y < height; y += height / 2 + 1)
    {
        for (int x = 0; x < width; x += width / 4 + 1)
        {
            int px = x + width / 8;
            int py = y + height / 4;

            int r1 = 0, g1 = 0, b1 = 0, a1 = 0xff;
            switch (4 * px / width)
            {
            case 0:
                r1 = g1 = b1 = 0xff;
                break;
            case 1:
                r1 = 0xff;
                break;
            case 2:
                g1 = 0xff;
                break;
            case 3:
                b1 = 0xff;
                break;
            }

            // OpenGL's origin is at the bottom left
            if (py < height / 2)
            {
                r1 >>= 1;
                g1 >>= 1;
                b1 >>= 1;
            }

            int r2, g2, b2, a2 = 0xff;
            int t = 32; // Large threshold due to possible dithering
            if (fbBits == 32)
            {
                uint32_t color = *reinterpret_cast<uint32_t*>
                    (&fbPixels[(height - y - 1) * fbStride + x * 4]);
                b2 = ((color & 0x000000ff) >>  0);
                g2 = ((color & 0x0000ff00) >>  8);
                r2 = ((color & 0x00ff0000) >> 16);
            }
            else
            {
                ASSERT(fbBits == 16);
                uint16_t color = *reinterpret_cast<uint16_t*>
                    (&fbPixels[(height - y - 1) * fbStride + x * 2]);
                r2 = ((color & 0xf800) >> 11) << 3;
                g2 = ((color & 0x07e0) >> 5) << 2;
                b2 = (color & 0x001f) << 3;
                r2 |= r2 >> 5;
                g2 |= g2 >> 6;
                b2 |= b2 >> 5;
            }

            if (abs(r1 - r2) > t ||
                abs(g1 - g2) > t ||
                abs(b1 - b2) > t ||
                abs(a1 - a2) > t)
            {
                test::fail("Image comparison failed at (%d, %d). "
                           "Expecting (%d,%d,%d,%d), got (%d,%d,%d,%d)\n",
                           px, py, r1, g1, b1, a1,
                           r2,  g2, b2, a2);
            }
        }
    }
}

/**
 *  Test locking window surfaces.
 *
 *  For each window surface config:
 *
 *  1. Create a window, a window surface and a rendering context.
 *  2. Lock the window surface.
 *  3. Render test pattern into surface
 *  4. Unlock window surface.
 *  5. Read pixels back from window surface and verify test pattern.
 */
void testWindowSurfaces()
{
    EGLConfig configs[256];
    EGLint configCount;
    bool result = true;

    EGLint configAttrs[] =
    {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_LOCK_SURFACE_BIT_KHR,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLint winAttrs[] =
    {
        EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED,
        EGL_NONE
    };

    EGLint lockAttrs[] =
    {
        EGL_LOCK_USAGE_HINT_KHR, EGL_WRITE_SURFACE_BIT_KHR,
        EGL_NONE
    };

    eglChooseConfig(dpy, configAttrs, &configs[0],
                    sizeof(configs) / sizeof(configs[0]), &configCount);
    ASSERT_EGL();

    if (!configCount)
    {
        test::fail("Lockable window surface configs not found\n");
    }

    for (int i = 0; i < configCount; i++)
    {
        EGLNativeWindowType win;
        EGLSurface surface;

        test::printHeader("Testing window surface config %d", configs[i]);

        try
        {
            if (!nativeCreateWindow(nativeDisplay, dpy, configs[i], __FILE__,
                                    winWidth, winHeight, &win))
            {
                test::fail("Unable to create a window\n");
            }

            surface = eglCreateWindowSurface(dpy, configs[i], win, winAttrs);
            ASSERT_EGL();
            if (!surface)
            {
                test::fail("Unable to create a surface\n");
            }

            // Render test pattern to window
            eglLockSurfaceKHR(dpy, surface, lockAttrs);
            ASSERT_EGL();

            fillSurface(dpy, surface);

            eglUnlockSurfaceKHR(dpy, surface);
            ASSERT_EGL();

            eglWaitClient();
            eglSwapBuffers(dpy, surface);
            ASSERT_EGL();
            sleep(1);

            // Verify test pattern
            checkFrontBuffer(win);

            eglDestroySurface(dpy, surface);
            nativeDestroyWindow(nativeDisplay, win);
            test::printResult(true);
        }
        catch (std::runtime_error e)
        {
            result = false;
            test::printResult(e);
        }
    }
    ASSERT(result);
}

/**
 *  Test locking pixmap surfaces.
 *
 *  For each pixmap surface config:
 *
 *  1. Create a pixmap and a pixmap surface.
 *  2. Lock the pixmap surface.
 *  3. Render test pattern into surface
 *  4. Unlock pixmap surface.
 *  5. Create EGLImage from pixmap and bind it to a texture.
 *  6. Blit texture onto a window surface.
 *  7. Read back pixels and verify test pattern.
 */
void testPixmapSurfaces()
{
    EGLConfig configs[256];
    EGLint configCount;
    bool result = true;

    EGLint configAttrs[] =
    {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_LOCK_SURFACE_BIT_KHR,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLint pixmapAttrs[] =
    {
        EGL_NONE
    };

    EGLint lockAttrs[] =
    {
        EGL_LOCK_USAGE_HINT_KHR, EGL_WRITE_SURFACE_BIT_KHR,
        EGL_NONE
    };

    EGLint imageAttributes[] =
    {
        EGL_NONE
    };

    eglChooseConfig(dpy, configAttrs, &configs[0],
                    sizeof(configs) / sizeof(configs[0]), &configCount);
    ASSERT_EGL();

    if (!configCount)
    {
        test::fail("Lockable pixmap surface configs not found\n");
    }

    for (int i = 0; i < configCount; i++)
    {
        EGLNativePixmapType pixmap;
        EGLSurface surface;
        EGLint depth;
        GLuint texture;

        test::printHeader("Testing pixmap surface config %d", configs[i]);

        try
        {
            eglGetConfigAttrib(dpy, configs[i], EGL_BUFFER_SIZE, &depth);
            ASSERT_EGL();

            if (!nativeCreatePixmap(nativeDisplay, depth, winWidth, winHeight,
                                    &pixmap))
            {
                test::fail("Unable to create a pixmap\n");
            }

            surface = eglCreatePixmapSurface(dpy, configs[i], pixmap, pixmapAttrs);
            ASSERT_EGL();
            if (!surface)
            {
                test::fail("Unable to create a surface\n");
            }

            // Render test pattern to pixmap
            eglLockSurfaceKHR(dpy, surface, lockAttrs);
            ASSERT_EGL();

            fillSurface(dpy, surface);

            eglUnlockSurfaceKHR(dpy, surface);
            ASSERT_EGL();

            // Create an EGL image from the pixmap
            EGLImageKHR image = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT,
                                                  EGL_NATIVE_PIXMAP_KHR,
                                                  (EGLClientBuffer)pixmap,
                                                  imageAttributes);
            ASSERT_EGL();

            // Bind the image to a texture
            eglMakeCurrent(util::ctx.dpy, util::ctx.surface, util::ctx.surface,
                    util::ctx.context);
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
            ASSERT_GL();

            // Render the texture onto the screen
            glClear(GL_COLOR_BUFFER_BIT);
            test::drawQuad(0, 0, winWidth, winHeight);
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &texture);
            eglMakeCurrent(util::ctx.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                    EGL_NO_CONTEXT);
            eglDestroyImageKHR(util::ctx.dpy, image);
            ASSERT_EGL();

            checkSurface(dpy, surface);

            test::swapBuffers();
            ASSERT_EGL();

            eglDestroySurface(dpy, surface);
            nativeDestroyPixmap(nativeDisplay, pixmap);

            test::printResult(true);
        }
        catch (std::runtime_error e)
        {
            test::printResult(e);
            result = false;
        }
    }
    ASSERT(result);
}

int main(int argc, char** argv)
{
    bool result;

    nativeCreateDisplay(&nativeDisplay);
    nativeGetDisplayProperties(nativeDisplay, &winWidth, &winHeight, &winDepth);

    const EGLint configAttrs[] =
    {
        EGL_BUFFER_SIZE,     winDepth,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    const EGLint contextAttrs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    dpy = eglGetDisplay(nativeDisplay);
    ASSERT_EGL();

    eglInitialize(dpy, NULL, NULL);

    result = util::createWindow(winWidth, winHeight, configAttrs, contextAttrs);
    ASSERT(result);
    ASSERT_EGL();

    GLint program = util::createProgram(test::vertSource, test::fragSource);
    glUseProgram(program);

    test::printHeader("Testing extension presence");
    result = test::verifyResult(testExtensionPresence);

    if (!result)
    {
        goto out;
    }

    result &= test::verify(testWindowSurfaces);
    result &= test::verify(testPixmapSurfaces);
out:
    glDeleteProgram(program);
    util::destroyWindow();
    eglTerminate(dpy);
    nativeDestroyDisplay(nativeDisplay);

    printf("================================================\n");
    printf("Result: ");
    test::printResult(result);

    return result ? 0 : 1;
}
