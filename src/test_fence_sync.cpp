/**
 * EGL_KHR_fence_sync test
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
#include <sys/types.h>
#include <sys/socket.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <boost/scoped_array.hpp>

#include <string.h>
#include <malloc.h>
#include <fcntl.h>

#include "ext.h"
#include "native.h"
#include "util.h"
#include "testutil.h"

// Sync functions
static PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
static PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
static PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
static PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR;
//static PFNEGLSIGNALSYNCKHRPROC eglSignalSyncKHR;

// Lock surface functions
static PFNEGLLOCKSURFACEKHRPROC eglLockSurfaceKHR;
static PFNEGLUNLOCKSURFACEKHRPROC eglUnlockSurfaceKHR;

// EGLImage functions
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

/**
 * Verify that the needed extensions are present
 */
void testExtensionPresence()
{
    ASSERT(util::isEGLExtensionSupported("EGL_KHR_fence_sync"));
    ASSERT(util::isGLExtensionSupported("GL_OES_EGL_sync"));

    // EGL sync
    eglCreateSyncKHR =
      (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    eglDestroySyncKHR =
      (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
    eglClientWaitSyncKHR =
      (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
    eglGetSyncAttribKHR =
      (PFNEGLGETSYNCATTRIBKHRPROC)eglGetProcAddress("eglGetSyncAttribKHR");
    //eglSignalSyncKHR =
    //  (PFNEGLSIGNALSYNCKHRPROC)eglGetProcAddress("eglSignalSyncKHR");

    ASSERT(eglCreateSyncKHR);
    ASSERT(eglDestroySyncKHR);
    ASSERT(eglClientWaitSyncKHR);
    ASSERT(eglGetSyncAttribKHR);
    //ASSERT(eglSignalSyncKHR);

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

/**
 *  Make sure fences can be used to synchronize access to a software surface.
 *
 *  1. Lock pixmap surface, draw solid color into it, unlock.
 *  2. Bind the pixmap surface as a pixmap and render it on screen.
 *  3. Place a fence and wait for it.
 *  4. Lock pixmap again, fill with different color and unlock.
 *  5. Check that second color is not visible on screen.
 *  6. Repeat.
 */
void testExclusion()
{
    unsigned int width = 512;
    unsigned int height = 384;
    unsigned int depth = 16;
    boost::scoped_array<uint16_t> screenPixels(new uint16_t[width * height]);
    uint16_t* pixels;
    EGLint pitch;
    GLuint texture;
    EGLSurface surface;
    EGLConfig config;
    EGLint configCount;

    EGLint lockAttrs[] =
    {
        EGL_LOCK_USAGE_HINT_KHR, EGL_WRITE_SURFACE_BIT_KHR,
        EGL_NONE
    };

    /* Create a pixmap */
    test::scoped<Pixmap> pixmap(
            boost::bind(nativeDestroyPixmap, util::ctx.nativeDisplay, _1));
    nativeCreatePixmap(util::ctx.nativeDisplay, depth, width, height, &pixmap);

    /* Create a pixmap surface */
    EGLint configAttrs[] =
    {
        EGL_SURFACE_TYPE, EGL_PIXMAP_BIT | EGL_LOCK_SURFACE_BIT_KHR,
        EGL_MATCH_FORMAT_KHR, EGL_FORMAT_RGB_565_EXACT_KHR,
        EGL_NONE
    };

    eglChooseConfig(util::ctx.dpy, configAttrs, &config, 1, &configCount);
    ASSERT(configCount > 0);

    surface = eglCreatePixmapSurface(util::ctx.dpy, config, pixmap, NULL);
    ASSERT_EGL();

    /* Create an image pointing to the pixmap */
    EGLImageKHR image = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT,
                                          EGL_NATIVE_PIXMAP_KHR,
                                          (EGLClientBuffer)(intptr_t)pixmap,
                                          NULL);
    ASSERT_EGL();

    /* Bind the image to a texture */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    ASSERT_GL();

    glClearColor(.2, .4, .6, 1.0);

    for (int step = 0; step < 128; step++)
    {
        uint16_t color1 = (1 | (1 << 6) | (1 << 11)) * step;
        uint16_t color2 = 0x0000;

        /* Fill the pixmap with color 1 */
        eglLockSurfaceKHR(util::ctx.dpy, surface, lockAttrs);
        eglQuerySurface(util::ctx.dpy, surface, EGL_BITMAP_POINTER_KHR, (EGLint*)&pixels); ASSERT_EGL();
        eglQuerySurface(util::ctx.dpy, surface, EGL_BITMAP_PITCH_KHR, &pitch); ASSERT_EGL();
        for (unsigned int i = 0; i < height * pitch / sizeof(*pixels); i++)
        {
            pixels[i] = color1;
        }
        eglUnlockSurfaceKHR(util::ctx.dpy, surface);
        ASSERT_EGL();

        /* Render the texture to the screen */
        glClear(GL_COLOR_BUFFER_BIT);
        test::drawQuad(0, 0, width, height);
        ASSERT_GL();

        /* Place fence */
        EGLSyncKHR sync = eglCreateSyncKHR(util::ctx.dpy, EGL_SYNC_FENCE_KHR, NULL);
        ASSERT_EGL();

        /* Wait for fence */
        eglClientWaitSyncKHR(util::ctx.dpy, sync,
                EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR);
        eglDestroySyncKHR(util::ctx.dpy, sync);
        ASSERT_EGL();

        /* Fill the pixmap with color 2 */
        eglLockSurfaceKHR(util::ctx.dpy, surface, lockAttrs);
        eglQuerySurface(util::ctx.dpy, surface, EGL_BITMAP_POINTER_KHR, (EGLint*)&pixels); ASSERT_EGL();
        eglQuerySurface(util::ctx.dpy, surface, EGL_BITMAP_PITCH_KHR, &pitch); ASSERT_EGL();
        for (unsigned int i = 0; i < height * pitch / sizeof(*pixels); i++)
        {
            pixels[i] = color2;
        }
        eglUnlockSurfaceKHR(util::ctx.dpy, surface);
        ASSERT_EGL();

        /* Make we don't see color 1 anywhere */
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &screenPixels[0]);
        for (unsigned int y = 0; y < height; y++)
        {
            for (unsigned int x = 0; x < width; x++)
            {
                if (!test::compareRGB565(screenPixels[y * width + x], color1))
                {
                    test::fail("Color comparison failed at (%d, %d). Expecting %04x, got %04x\n",
                               x, y, color1, screenPixels[y * width + x]);
                }
            }
        }
        test::swapBuffers();
        ASSERT_EGL();
    }

    /* Cleanup */
    glDeleteTextures(1, &texture);
    eglDestroyImageKHR(util::ctx.dpy, image);
    eglDestroySurface(util::ctx.dpy, surface);
}

/**
 *  Test a sync object without any rendering.
 */
void testNopSync()
{
    /* Sync without any rendering */
    EGLSyncKHR sync = eglCreateSyncKHR(util::ctx.dpy, EGL_SYNC_FENCE_KHR, NULL);
    ASSERT_EGL();

    /* Wait for fence */
    EGLint ret = eglClientWaitSyncKHR(util::ctx.dpy, sync,
            EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 1000ull * 1000 * 1000);
    ASSERT_EGL();
    ASSERT(ret == EGL_CONDITION_SATISFIED_KHR);

    eglDestroySyncKHR(util::ctx.dpy, sync);
    ASSERT_EGL();
}

/**
 *  Test a sync object with flushing by swapping the buffer.
 */
void testSyncWithSwap()
{
    /* Sync with swapbuffers */
    glClearColor(.8, .2, .1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    EGLSyncKHR sync = eglCreateSyncKHR(util::ctx.dpy, EGL_SYNC_FENCE_KHR, NULL);
    ASSERT_EGL();

    test::swapBuffers();
    ASSERT_EGL();

    /* Wait for fence */
    EGLint ret = eglClientWaitSyncKHR(util::ctx.dpy, sync, 0, 1000ull * 1000 * 1000);
    ASSERT_EGL();
    ASSERT(ret == EGL_CONDITION_SATISFIED_KHR);

    eglDestroySyncKHR(util::ctx.dpy, sync);
    ASSERT_EGL();
}

/**
 *  Test a sync object without swapping the buffer.
 */
void testSyncWithoutFlush()
{
    /* Sync without swapbuffers */
    glClearColor(.2, .8, .1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    EGLSyncKHR sync = eglCreateSyncKHR(util::ctx.dpy, EGL_SYNC_FENCE_KHR, NULL);
    ASSERT_EGL();

    /* Wait for fence */
    EGLint ret = eglClientWaitSyncKHR(util::ctx.dpy, sync,
            EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 1000ull * 1000 * 1000);
    ASSERT_EGL();
    ASSERT(ret == EGL_CONDITION_SATISFIED_KHR);

    eglDestroySyncKHR(util::ctx.dpy, sync);
    ASSERT_EGL();
}

/**
 *  Test several syncs in a row with or without frame swaps in between and in
 *  or out of order waiting.
 */
void testSyncQueue(bool inOrder, bool acrossFrames)
{
    /* Several syncs in a row waited on in different orders */
    const int queueSize = 64;
    EGLSyncKHR sync[queueSize];

    /* Render and place fences */
    for (int i = 0; i < queueSize; i++)
    {
        glClearColor(0.0, 0.0, float(i + 1) / queueSize, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        sync[i] = eglCreateSyncKHR(util::ctx.dpy, EGL_SYNC_FENCE_KHR, NULL);
        ASSERT_EGL();
        if (acrossFrames)
        {
            test::swapBuffers();
            ASSERT_EGL();
        }
    }

    /* Wait for fences */
    for (int i = 0; i < queueSize; i++)
    {
        int j = i;

        if (!inOrder)
        {
            j = ((queueSize - 1) ^ i) ^ ((i << 2) % queueSize);
        }

        EGLint ret = eglClientWaitSyncKHR(util::ctx.dpy, sync[j],
                EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 1000ull * 1000 * 1000);
        ASSERT_EGL();
        ASSERT(ret == EGL_CONDITION_SATISFIED_KHR);

        eglDestroySyncKHR(util::ctx.dpy, sync[j]);
        ASSERT_EGL();
    }
}

/**
 *  Measure various delays when using sync objects
 */
void testLatency()
{
    int cycles = 64;
    int64_t start, totalRendering = 0, totalCreate = 0,
            totalWait = 0, totalDestroy = 0;

    glClearColor(.8, .1, .6, 1.0);

    for (int i = 0; i < cycles; i++)
    {
        /* Render */
        start = util::getTime();
        glClear(GL_COLOR_BUFFER_BIT);
        totalRendering += util::getTime() - start;

        /* Place fence */
        start = util::getTime();
        EGLSyncKHR sync = eglCreateSyncKHR(util::ctx.dpy, EGL_SYNC_FENCE_KHR, NULL);
        totalCreate += util::getTime() - start;

        /* Wait for fence */
        start = util::getTime();
        eglClientWaitSyncKHR(util::ctx.dpy, sync,
                             EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 1000ull * 1000 * 1000);
        totalWait += util::getTime() - start;

        /* Destroy */
        start = util::getTime();
        eglDestroySyncKHR(util::ctx.dpy, sync);
        totalDestroy += util::getTime() - start;

        /* Reset */
        test::swapBuffers();
    }

    printf("%lld us / %lld us / %lld us / %lld us : ",
            totalRendering / cycles / 1000UL,
            totalCreate / cycles / 1000UL,
            totalWait / cycles / 1000UL,
            totalDestroy / cycles / 1000UL);
}

int main(int argc, char** argv)
{
    bool result;
    int winWidth = 864;
    int winHeight = 480;
    int winDepth = 16;

    EGLNativeDisplayType dpy;
    nativeCreateDisplay(&dpy);
    nativeGetDisplayProperties(dpy, &winWidth, &winHeight, &winDepth);
    nativeDestroyDisplay(dpy);

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

    result = util::createWindow(winWidth, winHeight, configAttrs, contextAttrs);
    ASSERT(result);
    ASSERT_EGL();

    GLint program = util::createProgram(test::vertSource, test::fragSource);
    glUseProgram(program);
    ASSERT_GL();

    test::printHeader("Testing extension presence");
    result &= test::verifyResult(testExtensionPresence);

    if (!result)
    {
        goto out;
    }

    test::printHeader("Testing exclusion");
    result &= test::verifyResult(testExclusion);
    test::printHeader("Testing sync w/o rendering");
    result &= test::verifyResult(testNopSync);
    test::printHeader("Testing sync w/swap");
    result &= test::verifyResult(testSyncWithSwap);
    test::printHeader("Testing sync queue in-order");
    result &= test::verifyResult(boost::bind(testSyncQueue, true, false));
    test::printHeader("Testing sync queue in-order w/swaps");
    result &= test::verifyResult(boost::bind(testSyncQueue, true, true));
    test::printHeader("Testing sync queue out-of-order");
    result &= test::verifyResult(boost::bind(testSyncQueue, false, false));
    test::printHeader("Testing sync queue out-of-order w/swaps");
    result &= test::verifyResult(boost::bind(testSyncQueue, false, true));
    test::printHeader("Testing sync latency");
    result &= test::verifyResult(testLatency);

out:
    glDeleteProgram(program);
    util::destroyWindow();

    printf("================================================\n");
    printf("Result: ");
    test::printResult(result);

    return result ? 0 : 1;
}
