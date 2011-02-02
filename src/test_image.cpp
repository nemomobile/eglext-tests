/**
 * EGL_KHR_image_base and EGL_KHR_image_pixmap test
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

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <boost/scoped_array.hpp>

#include "ext.h"
#include "native.h"
#include "util.h"
#include "testutil.h"

static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
static PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES;

/**
 * Verify that the needed extensions are present
 */
void testExtensionPresence()
{
    ASSERT(util::isEGLExtensionSupported("EGL_KHR_image_base"));
    ASSERT(util::isEGLExtensionSupported("EGL_KHR_image_pixmap"));

    eglCreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR =
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    glEGLImageTargetRenderbufferStorageOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");

    ASSERT(eglCreateImageKHR);
    ASSERT(eglDestroyImageKHR);
    ASSERT(glEGLImageTargetTexture2DOES);
    ASSERT(glEGLImageTargetRenderbufferStorageOES);
}

/**
 * Test known invalid inputs.
 *
 * 1. Invalid pixmap handle.
 */
void testFailureCases()
{
    /* Invalid pixmap */
    EGLImageKHR image = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT,
                                          EGL_NATIVE_PIXMAP_KHR, 0, NULL);
    ASSERT(image == (EGLImageKHR)0);
    ASSERT(eglGetError() != EGL_SUCCESS);
}

static unsigned int colorPattern = 0;
static const unsigned int colorPatternCount = 9;

uint32_t colorAt(int width, int height, int x, int y)
{
    uint8_t r, g, b, a = 0xff;

    switch (colorPattern)
    {
        case 0:
            r = g = b = 0xff;
            break;
        case 1:
            r = g = b = 0;
            break;
        case 2:
            r = 0xff;
            g = 0;
            b = 0;
            break;
        case 3:
            r = 0;
            g = 0xff;
            b = 0;
            break;
        case 4:
            r = 0;
            g = 0;
            b = 0xff;
            break;
        case 5:
            r = 0xff;
            g = 0xff;
            b = 0;
            break;
        case 6:
            r = 0;
            g = 0xff;
            b = 0xff;
            break;
        case 7:
            r = 0xff;
            g = 0;
            b = 0xff;
            break;
        case 8:
            r = (x * 256) / width;
            g = (y * 256) / height;
            b = 0xff - ((x + y) * 256) / (width + height);
            break;
    }
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static void fillPixmap(Pixmap pixmap, int width, int height, int depth)
{
    XImage* img = XGetImage(util::ctx.nativeDisplay, pixmap,
                            0, 0, width, height, -1, ZPixmap);
    ASSERT(img);
    ASSERT(img->data);

    uint8_t* data = (uint8_t*)img->data;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint32_t color = colorAt(width, height, x, y);

            switch (depth)
            {
                case 24:
                case 32:
                {
                    ((uint32_t*)data)[x] = color;
                    break;
                }
                case 16:
                {
                    uint8_t b = ((color & 0x000000ff) >>  0);
                    uint8_t g = ((color & 0x0000ff00) >>  8);
                    uint8_t r = ((color & 0x00ff0000) >> 16);
                    r >>= 3;
                    g >>= 2;
                    b >>= 3;
                    color = (r << 11) | (g << 5) | b;
                    ((uint16_t*)data)[x] = (uint16_t)color;
                    break;
                }
            }
        }
        data += img->bytes_per_line / sizeof(*data);
    }

    XGCValues gcValues;
    GC gc = XCreateGC(util::ctx.nativeDisplay, pixmap, 0, &gcValues);
    XPutImage(util::ctx.nativeDisplay, pixmap, gc, img, 0, 0, 0, 0, width, height);
    XFreeGC(util::ctx.nativeDisplay, gc);
    XDestroyImage(img);

    eglWaitNative(EGL_CORE_NATIVE_ENGINE);
}

/**
 *  Test pixmap usage as texture via EGLImage.
 *
 *  1. Create @width x @height x @depth pixmap.
 *  2. Fill pixmap with test pattern.
 *  3. Create EGLimage referring to pixmap with EGL_NATIVE_PIXMAP_KHR.
 *  4. Bind EGLImage to texture with glEGLImageTargetTexture2DOES.
 *  5. Draw quad using the texture.
 *  6. Verify that destination surface contains the test pattern.
 */
void testTextures(int width, int height, int depth)
{
    test::scoped<Pixmap> pixmap(
            boost::bind(nativeDestroyPixmap, util::ctx.nativeDisplay, _1));
    boost::scoped_array<uint32_t> pixels(new uint32_t[width * height]);
    EGLImageKHR image;
    GLuint texture;

    const EGLint imageAttributes[] =
    {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };

    /* Create and fill the pixmap with the test pattern */
    ASSERT(nativeCreatePixmap(util::ctx.nativeDisplay, depth, width, height, &pixmap));
    fillPixmap(pixmap, width, height, depth);

    /* Create an EGL image from the pixmap */
    image = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                              (EGLClientBuffer)(intptr_t)pixmap, imageAttributes);
    ASSERT_EGL();

    /* Bind the image to a texture */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    ASSERT_GL();

    test::drawQuad(0, 0, width, height);

    /* Check the results */
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
    ASSERT_GL();

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint32_t p1 = colorAt(width, height, x, height - y - 1);
            uint32_t p2 = pixels[y * width + x];
            uint8_t b1 = (p1 & 0x000000ff);
            uint8_t g1 = (p1 & 0x0000ff00) >> 8;
            uint8_t r1 = (p1 & 0x00ff0000) >> 16;
            uint8_t a1 = (p1 & 0xff000000) >> 24;
            uint8_t r2 = (p2 & 0x000000ff);
            uint8_t g2 = (p2 & 0x0000ff00) >> 8;
            uint8_t b2 = (p2 & 0x00ff0000) >> 16;
            uint8_t a2 = (p2 & 0xff000000) >> 24;
            int t = 8;

            if (abs(r1 - r2) > t ||
                abs(g1 - g2) > t ||
                abs(b1 - b2) > t ||
                abs(a1 - a2) > t)
            {
                test::fail("Image comparison failed at (%d, %d), size (%d, %d), expected %02x%02x%02x%02x, "
                           "got %02x%02x%02x%02x\n", x, y, width, height, r1, g1, b1, a1,
                           r2, g2, b2, a2);
            }
        }
    }

    /* Clean up */
    glDeleteTextures(1, &texture);
    eglDestroyImageKHR(util::ctx.dpy, image);
}

/**
 *  Test pixmap usage as framebuffer via EGLImage.
 *
 *  1. Create @width x @height x @depth pixmap.
 *  2. Create EGLimage referring to pixmap with EGL_NATIVE_PIXMAP_KHR.
 *  3. Bind EGLImage to renderbuffer with
 *     glEGLImageTargetRenderbufferStorageOES.
 *  4. Draw quad using a test pattern texture to the rendebuffer.
 *  5. Finish rendering.
 *  6. Verify that pixmap contains the test pattern.
 */
void testFramebuffers(int width, int height, int depth)
{
    test::scoped<Pixmap> pixmap(
            boost::bind(nativeDestroyPixmap, util::ctx.nativeDisplay, _1));
    EGLImageKHR image;
    GLuint texture;
    GLuint framebuffer;
    GLuint renderbuffer;
    boost::scoped_array<uint32_t> pixels(new uint32_t[width * height]);

    /* Create a pixmap */
    ASSERT(nativeCreatePixmap(util::ctx.nativeDisplay, depth, width, height, &pixmap));

    /* Create an EGL image from the pixmap */
    image = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                              (EGLClientBuffer)(intptr_t)pixmap, NULL);
    ASSERT_EGL();

    /* Bind the image to a renderbuffer */
    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, image);
    ASSERT_GL();

    /* Bind the renderbuffer to a framebuffer */
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, renderbuffer);
    ASSERT_GL();

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    ASSERT(status == GL_FRAMEBUFFER_COMPLETE);

    /* Create a texture with the test pattern */
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            pixels[y * width + x] = colorAt(width, height, x, y);
        }
    }
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, &pixels[0]);
    ASSERT_GL();

    test::drawQuad(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFinish();

    /* Check the results */
    XImage* img = XGetImage(util::ctx.nativeDisplay, pixmap,
                            0, 0, width, height, -1, ZPixmap);
    ASSERT(img);
    ASSERT(img->data);

    uint8_t* data = (uint8_t*)img->data;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint32_t color = colorAt(width, height, x, height - 1 - y);
            uint8_t b1 = (color & 0x000000ff);
            uint8_t g1 = (color & 0x0000ff00) >> 8;
            uint8_t r1 = (color & 0x00ff0000) >> 16;
            uint8_t a1 = (color & 0xff000000) >> 24;
            uint8_t r2 = 0;
            uint8_t g2 = 0;
            uint8_t b2 = 0;
            uint8_t a2 = 0;
            int t = 8;

            switch (depth)
            {
                case 24:
                case 32:
                {
                    uint32_t p = ((uint32_t*)data)[x];
                    r2 = (p & 0x000000ff);
                    g2 = (p & 0x0000ff00) >> 8;
                    b2 = (p & 0x00ff0000) >> 16;
                    a2 = (p & 0xff000000) >> 24;
                    break;
                }
                case 16:
                {
                    uint16_t p = ((uint16_t*)data)[x];
                    b2 = ((p & 0xf800) >> 11) << 3;
                    g2 = ((p & 0x07e0) >> 5) << 2;
                    r2 = (p & 0x001f) << 3;
                    a2 = 0xff;

                    r2 |= r2 >> 5;
                    g2 |= g2 >> 6;
                    b2 |= b2 >> 5;
                    break;
                }
                default:
                {
                    ASSERT(0);
                    break;
                }
            }

            if (abs(r1 - r2) > t ||
                abs(g1 - g2) > t ||
                abs(b1 - b2) > t ||
                abs(a1 - a2) > t)
            {
                test::fail("Image comparison failed at (%d, %d), size (%d, %d), expected %02x%02x%02x%02x, "
                           "got %02x%02x%02x%02x\n", x, y, width, height, r1, g1, b1, a1,
                           r2, g2, b2, a2);
            }
        }
        data += img->bytes_per_line / sizeof(*data);
    }
    XDestroyImage(img);
    ASSERT_GL();

    /* Clean up */
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteRenderbuffers(1, &framebuffer);
    glDeleteTextures(1, &texture);
    eglDestroyImageKHR(util::ctx.dpy, image);
}

/**
 *  Measure how long it takes to bind an EGLImage to a texture.
 *
 *  Initialization:
 *
 *  1. Create @width x @height pixmap.
 *  2. Create a texture object.
 *
 *  Test loop:
 *
 *  1. Create an EGLImage from the pixmap using EGL_NATIVE_PIXMAP_KHR.
 *  2. Bind the EGLImage to the texture.
 *  3. Draw a 1x1 quad using the texture and read a pixel to force rendering to
 *     complete.
 *
 *  The test result is the average time for each step in the test loop.
 */
void testMappingLatency(int width, int height)
{
    test::scoped<Pixmap> pixmap(
            boost::bind(nativeDestroyPixmap, util::ctx.nativeDisplay, _1));
    EGLImageKHR image;
    GLuint targetTexture;

    int cycles = 64;
    int64_t start, totalImageCreation = 0,
            totalImageBinding = 0, totalRendering = 0;
    uint8_t color[4];

    const EGLint imageAttributes[] =
    {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };

    /* Create the pixmap */
    ASSERT(nativeCreatePixmap(util::ctx.nativeDisplay, 32, width, height, &pixmap));
    fillPixmap(pixmap, width, height, 32);

    /* Measure creation and binding times */
    glGenTextures(1, &targetTexture);
    glBindTexture(GL_TEXTURE_2D, targetTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    for (int i = 0; i < cycles; i++)
    {
        /* Bind the pixmap to an image */
        start = util::getTime();
        image = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT,
                                  EGL_NATIVE_PIXMAP_KHR,
                                  (EGLClientBuffer)(intptr_t)pixmap,
                                  imageAttributes);
        totalImageCreation += util::getTime() - start;

        /* Bind the image to a texture */
        start = util::getTime();
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
        totalImageBinding += util::getTime() - start;

        /* Draw and read back a single pixel to make sure the texture is fully
         * prepared
         */
        start = util::getTime();
        glClear(GL_COLOR_BUFFER_BIT);
        test::drawQuad(0, 0, 1, 1);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);
        totalRendering += util::getTime() - start;

        /* Prepare for next round */
        eglDestroyImageKHR(util::ctx.dpy, image);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, color);
        test::swapBuffers();
        ASSERT_GL();
        ASSERT_EGL();
    }

    printf("%lld us / %lld us / %lld us : ",
            totalImageCreation / cycles / 1000UL,
            totalImageBinding / cycles / 1000UL,
            totalRendering / cycles / 1000UL);

    /* Clean up */
    eglDestroyImageKHR(util::ctx.dpy, image);
    glDeleteTextures(1, &targetTexture);
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

    struct
    {
        int width;
        int height;
        int depth;
    } entries[] =
    {
        {64,  64, 16},
        {64,  64, 32},
        {113, 47, 16},
        {113, 47, 32},
    };

    const int sizes[] = {
        16,
        64,
        128,
        256,
        512,
        1024
    };

    GLint program = util::createProgram(test::vertSource, test::fragSource);
    glUseProgram(program);

    test::printHeader("Testing extension presence");
    result = test::verifyResult(testExtensionPresence);

    if (!result)
    {
        goto out;
    }

    test::printHeader("Testing failure cases");
    result &= test::verifyResult(testFailureCases);

    for (colorPattern = 0; colorPattern < colorPatternCount; colorPattern++)
    {
        for (unsigned int i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
        {
            glClear(GL_COLOR_BUFFER_BIT);

            test::printHeader("Testing %dx%d %dbpp texture, p%d",
                    entries[i].width, entries[i].height, entries[i].depth,
                    colorPattern);

            result &= test::verifyResult(
                    boost::bind(testTextures, entries[i].width,
                                entries[i].height, entries[i].depth));

            test::printHeader("Testing %dx%d %dbpp framebuffer, p%d",
                    entries[i].width, entries[i].height, entries[i].depth,
                    colorPattern);

            result &= test::verifyResult(
                    boost::bind(testFramebuffers, entries[i].width,
                                entries[i].height, entries[i].depth));
            test::swapBuffers();
        }
    }

    for (unsigned int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        test::printHeader("Testing binding latency (%dx%d 32bpp)",
                 sizes[i], sizes[i]);

        result &= test::verifyResult(
                boost::bind(testMappingLatency, sizes[i], sizes[i]));
    }

out:
    glDeleteProgram(program);
    util::destroyWindow();

    printf("================================================\n");
    printf("Result: ");
    test::printResult(result);

    return result ? 0 : 1;
}
