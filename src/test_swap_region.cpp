/**
 * EGL_NOK_swap_region2 conformance test
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
#include <math.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <boost/scoped_array.hpp>

#include "ext.h"
#include "native.h"
#include "util.h"
#include "testutil.h"

static PFNEGLSWAPBUFFERSREGION2NOKPROC eglSwapBuffersRegion2NOK;

/**
 * Verify that the needed extensions are present
 */
void testExtensionPresence()
{
    ASSERT(util::isEGLExtensionSupported("EGL_NOK_swap_region2"));

    eglSwapBuffersRegion2NOK =
        (PFNEGLSWAPBUFFERSREGION2NOKPROC)eglGetProcAddress("eglSwapBuffersRegion2NOK");

    ASSERT(eglSwapBuffersRegion2NOK);
}

/**
 *  Smoke test to ensure that the extension is working.
 */
void testSmoke()
{
    eglSwapBuffersRegion2NOK(util::ctx.dpy, util::ctx.surface, 0, NULL);
    ASSERT_EGL();

    const EGLint rects[] = {
        50, 50, 10, 10,
    };
    eglSwapBuffersRegion2NOK(util::ctx.dpy, util::ctx.surface, 1, rects);
    ASSERT_EGL();
}

/**
 *  Test known invalid inputs.
 *
 *  1. NULL rectangle list.
 *  2. Negative rectangle count.
 */
void testFailureCases()
{
    // NULL list of rectangles
    eglSwapBuffersRegion2NOK(util::ctx.dpy, util::ctx.surface, 1, NULL);
    ASSERT(eglGetError() == EGL_BAD_PARAMETER);

    // Negative rectangle count
    const EGLint rects[] = {
        50, 50, 10, 10,
    };
    eglSwapBuffersRegion2NOK(util::ctx.dpy, util::ctx.surface, -1, rects);
    ASSERT(eglGetError() == EGL_BAD_PARAMETER);
}

static void createFramebuffer(GLuint& texture, GLuint& framebuffer,
                              int surfaceWidth, int surfaceHeight)
{
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surfaceWidth, surfaceHeight, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    ASSERT_GL();

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Framebuffer incomplete\n");
        ASSERT(0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ASSERT_GL();
}

static void compareFramebufferWithDisplay(GLuint framebuffer, int width, int height)
{
    boost::scoped_array<uint8_t> texPixels(new uint8_t[width * height * 2]);
    uint8_t* fbPixels = 0;
    int fbFd, fbSize, fbStride, fbOffset, fbBits;

    eglWaitClient();
    eglWaitNative(EGL_CORE_NATIVE_ENGINE);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glReadPixels(0, 0, width, height, GL_RGB,
                 GL_UNSIGNED_SHORT_5_6_5, &texPixels[0]);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#if 0
    FILE* f = fopen("/opt/tex.raw", "wb");
    fwrite(texPixels, width * height * 2, 1, f);
    fclose(f);
#endif

    if (!test::mapFramebuffer(fbPixels, fbFd, fbSize, fbBits, fbStride, fbOffset))
    {
        test::fail("Unable to map framebuffer");
    }

    test::scoped<int> fb(
            fbFd, boost::bind(test::unmapFramebuffer, fbPixels, _1, fbSize));

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            // Texture data is stored upside down compared to the system
            // framebuffer
            uint16_t p1 = *reinterpret_cast<uint16_t*>(&texPixels[(y * width + x) * 2]);
            uint16_t p2 = 0;

            if (fbBits == 16)
            {
                p2 = *reinterpret_cast<uint16_t*>
                    (&fbPixels[fbOffset + (height - y - 1) * fbStride + x * 2]);
            }
            else
            {
                ASSERT(fbBits == 32);
                uint32_t color = *reinterpret_cast<uint32_t*>
                    (&fbPixels[fbOffset + (height - y - 1) * fbStride + x * 4]);
                uint8_t b = ((color & 0x000000ff) >>  0);
                uint8_t g = ((color & 0x0000ff00) >>  8);
                uint8_t r = ((color & 0x00ff0000) >> 16);
                r >>= 3;
                g >>= 2;
                b >>= 3;
                p2 = (r << 11) | (g << 5) | b;
            }

            if (!test::compareRGB565(p1,  p2))
            {
                test::fail("Framebuffer comparison failed at (%d, %d): "
                           "expected %04x, got %04x\n",
                           x, y, p1, p2);
            }
        }
    }
}

/**
 *  Basic partial updates test.
 *
 *  Initialization:
 *
 *  1. Clear screen to known color.
 *
 *  Test loop:
 *
 *  1. Clear entire screen to known "invalid" color.
 *  2. Render @maxRects rectangles onto the screen by using glScissor and
 *     glClear.
 *  3. Render same rectangles to a texture.
 *  4. Do a partial swap covering the area of the rectangles.
 *  5. Animate rectangles.
 *
 *  Verification:
 *
 *  1. Verify that display contents match those of the texture. The screen
 *     should show a number of rectangles with colorful trails.
 */
void testPartialUpdates(int maxRects = 0)
{
    int cycles = 100;
    struct
    {
        EGLint x, y, w, h;
    } rects[] =
    {
        {10,  20,   54,  37},
        {30,  10,   64,  64},
        {10,  60,  117,  47},
        {534, 212,  42, 160},
        {353, 123, 234,  24},
        {251, 400,  36,  64},
        {244, 333,  46,  61},
        {125, 95,   25,  53},
    };
    struct
    {
        EGLint x, y;
    } vel[] =
    {
        { 1, 2},
        {-3, 7},
        { 5,-3},
        {-3, 7},
        {-5, 3},
        { 1,-2},
        {-3,-7},
        {-5, 3},
    };
    GLubyte colors[][4] =
    {
        {0xff, 0x00, 0x00, 0xff},
        {0x00, 0xff, 0x00, 0xff},
        {0x00, 0x00, 0xff, 0xff},
        {0xff, 0xff, 0x00, 0xff},
        {0xff, 0x00, 0xff, 0xff},
        {0x00, 0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff, 0xff},
        {0x7f, 0x7f, 0x7f, 0x00},
    };
    int numRects = maxRects ? maxRects : (sizeof(rects) / sizeof(rects[0]));
    int numColors = sizeof(colors) / sizeof(colors[0]);
    EGLint surfaceWidth, surfaceHeight;
    GLuint texture, framebuffer;

    eglQuerySurface(util::ctx.dpy, util::ctx.surface, EGL_WIDTH, &surfaceWidth);
    eglQuerySurface(util::ctx.dpy, util::ctx.surface, EGL_HEIGHT, &surfaceHeight);

    // Render the same output into an offscreen buffer
    createFramebuffer(texture, framebuffer, surfaceWidth, surfaceHeight);

    // Clear background with a known solid color
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    test::swapBuffers();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    for (int frame = 0; frame < cycles; frame++)
    {
        // Move the rectangles
        for (int i = 0; i < numRects; i++)
        {
            if (rects[i].x + rects[i].w + vel[i].x > surfaceWidth ||
                rects[i].x + vel[i].x < 0)
            {
                vel[i].x = -vel[i].x;
            }
            if (rects[i].y + rects[i].h + vel[i].y > surfaceHeight ||
                rects[i].y + vel[i].y < 0)
            {
                vel[i].y = -vel[i].y;
            }
            rects[i].x += vel[i].x;
            rects[i].y += vel[i].y;
        }

        // "Invalidate" frame buffer
        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render rectangles
        glEnable(GL_SCISSOR_TEST);
        for (int pass = 0; pass < 2; pass++)
        {
            if (pass == 0)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
            else
            {
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            }
            for (int i = 0; i < numRects; i++)
            {
                glScissor(rects[i].x, surfaceHeight - rects[i].y - rects[i].h,
                          rects[i].w, rects[i].h);
                glClearColor(
                    (colors[i % numColors][0] * (0.5f + 0.25f * sin(frame * 0.1f))) / 255.f,
                    (colors[i % numColors][1] * (0.5f + 0.25f * sin(frame * 0.2f))) / 255.f,
                    (colors[i % numColors][2] * (0.5f + 0.25f * sin(frame * 0.3f))) / 255.f,
                    (colors[i % numColors][3] * (0.5f + 0.25f * sin(frame * 0.4f))) / 255.f),
                glClear(GL_COLOR_BUFFER_BIT);
                ASSERT_GL();
            }
        }
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        ASSERT_GL();

        // Do the partial swap
        eglSwapBuffersRegion2NOK(util::ctx.dpy, util::ctx.surface, numRects,
                               reinterpret_cast<EGLint*>(rects));
        ASSERT_EGL();
    }

    // Check the results
    compareFramebufferWithDisplay(framebuffer, surfaceWidth, surfaceHeight);

    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
}

/**
 *  Test partial updates synchronization with rendering.
 *
 *  Test loop:
 *
 *  1. Choose a previously unused tile region from the screen.
 *  2. Clear the tile with a solid color.
 *  3. Clear the same tile in a texture.
 *  4. Do a partial swap with the area of the tile.
 *
 *  Verification:
 *
 *  1. Verify that display contents match those of the texture. The screen
 *     should show a grid of colorful tiles.
 */
void testSynchronization()
{
    int cycles = 91;
    GLubyte colors[][4] =
    {
        {0xff, 0x00, 0x00, 0xff},
        {0x00, 0xff, 0x00, 0xff},
        {0x00, 0x00, 0xff, 0xff},
        {0xff, 0xff, 0x00, 0xff},
        {0xff, 0x00, 0xff, 0xff},
        {0x00, 0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff, 0xff},
        {0x7f, 0x7f, 0x7f, 0x00},
    };
    int numColors = sizeof(colors) / sizeof(colors[0]);
    EGLint surfaceWidth, surfaceHeight;
    GLuint texture, framebuffer;
    int tileSize = 64;
    int tileX = 0, tileY = 0;

    eglQuerySurface(util::ctx.dpy, util::ctx.surface, EGL_WIDTH, &surfaceWidth);
    eglQuerySurface(util::ctx.dpy, util::ctx.surface, EGL_HEIGHT, &surfaceHeight);

    // Render the same output into an offscreen buffer
    createFramebuffer(texture, framebuffer, surfaceWidth, surfaceHeight);

    // Clear background with a known solid color
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    test::swapBuffers();
    ASSERT_GL();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ASSERT_GL();

    for (int frame = 0; frame < cycles; frame++)
    {
        glClearColor(
            (colors[frame % numColors][0]) / 255.f,
            (colors[frame % numColors][1]) / 255.f,
            (colors[frame % numColors][2]) / 255.f,
            (colors[frame % numColors][3]) / 255.f);

        // Clear the entire framebuffer with a specific color
        glClear(GL_COLOR_BUFFER_BIT);

        // Update just a single tile on the screen
        EGLint rect[] =
        {
            tileX, tileY,
            tileSize, tileSize
        };
        eglSwapBuffersRegion2NOK(util::ctx.dpy, util::ctx.surface, 1, rect);
        ASSERT_EGL();

        tileX += tileSize;
        if (tileX + tileSize > surfaceWidth)
        {
            tileX = 0;
            tileY += tileSize;
        }
    }
    ASSERT_GL();

    // Do the same for the FBO
    tileX = tileY = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glEnable(GL_SCISSOR_TEST);
    ASSERT_GL();
    for (int frame = 0; frame < cycles; frame++)
    {
        glClearColor(
            (colors[frame % numColors][0]) / 255.f,
            (colors[frame % numColors][1]) / 255.f,
            (colors[frame % numColors][2]) / 255.f,
            (colors[frame % numColors][3]) / 255.f);

        // Clear the tile with a specific color
        glScissor(tileX, surfaceHeight - tileSize - tileY, tileSize, tileSize);
        glClear(GL_COLOR_BUFFER_BIT);
        ASSERT_GL();

        tileX += tileSize;
        if (tileX + tileSize > surfaceWidth)
        {
            tileX = 0;
            tileY += tileSize;
        }
    }
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ASSERT_GL();

    // Check the results
    compareFramebufferWithDisplay(framebuffer, surfaceWidth, surfaceHeight);

    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
}

/**
 *  Test a single partial update.
 *
 *  1. Clear the screen with a known solid color.
 *  2. Also clear a texture with the same color.
 *  3. Clear the entire screen with a second color.
 *  4. Do a partial update that covers a rectangle on the screen.
 *  5. Change the color of an identical region in the texture using scissoring.
 *  6. Verify that screen and texture contents match. The screen should contain
 *     a single green rectangle on a gray background.
 */
void testSingleUpdate()
{
    EGLint surfaceWidth, surfaceHeight;
    GLuint texture, framebuffer;

    eglQuerySurface(util::ctx.dpy, util::ctx.surface, EGL_WIDTH, &surfaceWidth);
    eglQuerySurface(util::ctx.dpy, util::ctx.surface, EGL_HEIGHT, &surfaceHeight);

    // Render the same output into an offscreen buffer
    createFramebuffer(texture, framebuffer, surfaceWidth, surfaceHeight);

    // Clear background with a known solid color
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    test::swapBuffers();
    ASSERT_GL();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ASSERT_GL();

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);

    // Clear the entire framebuffer with a specific color
    glClear(GL_COLOR_BUFFER_BIT);

    // Update just a single tile on the screen
    EGLint rect[] =
    {
        64, 64,
        128, 128,
    };
    eglSwapBuffersRegion2NOK(util::ctx.dpy, util::ctx.surface, 1, rect);
    ASSERT_EGL();

    // Do the same for the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glEnable(GL_SCISSOR_TEST);
    ASSERT_GL();

    // Clear the tile with a specific color
    glScissor(rect[0], surfaceHeight - rect[3] - rect[1], rect[2], rect[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ASSERT_GL();

    // Check the results
    compareFramebufferWithDisplay(framebuffer, surfaceWidth, surfaceHeight);

    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
}

/**
 *  Measure the performance of a simple partial update.
 *
 *  Full update loop:
 *
 *  1. Clear entire screen.
 *  2. Clear a single subrectangle of the screen with another color.
 *  3. Swap the buffer.
 *
 *  Partial update loop:
 *
 *  1. Clear entire screen.
 *  2. Do a partial swap covering the update region.
 *
 *  The test results include average timings from the two loops above.
 */
void testSimplePerformance(int width, int height)
{
    int frames = 256;
    int i;
    int64_t start, end;
    EGLint surfaceHeight;

    eglQuerySurface(util::ctx.dpy, util::ctx.surface, EGL_HEIGHT, &surfaceHeight);

    // 1. Fullscreen updates
    glScissor(0, 0, width, height);
    start = util::getTime();
    for (i = 0; i < frames; i++)
    {
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        glClearColor((float)i / frames, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        test::swapBuffers();
    }
    end = util::getTime();
    glDisable(GL_SCISSOR_TEST);
    ASSERT_GL();
    printf("full %lld us, ", (end - start) / frames / 1000UL);

    // 2. Partial updates
    EGLint rect[] =
    {
        0, surfaceHeight - height, width, height
    };

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    start = util::getTime();
    for (i = 0; i < frames; i++)
    {
        glClearColor(0.0f, (float)i / frames, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffersRegion2NOK(util::ctx.dpy, util::ctx.surface, 1, rect);
    }
    end = util::getTime();
    glDisable(GL_SCISSOR_TEST);
    ASSERT_GL();
    printf("partial %lld us : ", (end - start) / frames / 1000UL);
}

/**
 *  Measure the performance of a complex partial update.
 *
 *  For number of rectangles in range 0..8:
 *
 *  1. Clear entire screen.
 *  2. Do a partial swap covering the region of the rectangles.
 *
 *  The test results include average timings from the loop above using a
 *  different number of rectangles.
 */
void testComplexPerformance(int width, int height)
{
    int frames = 256;
    int i;
    int64_t start, end;
    EGLint surfaceWidth;
    EGLint surfaceHeight;
    int maxRects = 8;
    int margin = 4;
    EGLint rects[maxRects * 4];

    eglQuerySurface(util::ctx.dpy, util::ctx.surface, EGL_WIDTH, &surfaceWidth);
    eglQuerySurface(util::ctx.dpy, util::ctx.surface, EGL_HEIGHT, &surfaceHeight);

    int x = 0;
    int y = surfaceHeight - height;
    for (i = 0; i < maxRects; i++)
    {
        rects[i * 4 + 0] = x;
        rects[i * 4 + 1] = y;
        rects[i * 4 + 2] = width;
        rects[i * 4 + 3] = height;
        x += width + margin;
        if (x + width >= surfaceWidth)
        {
            x = 0;
            y -= height + margin;
            if (y < 0)
            {
                maxRects = i + 1;
                break;
            }
        }
    }

    for (int numRects = 1; numRects < maxRects; numRects++)
    {
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        test::swapBuffers();

        start = util::getTime();
        for (i = 0; i < frames; i++)
        {
            glClearColor(0.0f, 0.f, (float)i / frames, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            eglSwapBuffersRegion2NOK(util::ctx.dpy, util::ctx.surface, numRects, rects);
        }
        end = util::getTime();
        printf("%dx %lld us", numRects, (end - start) / frames / 1000UL);
        printf((numRects == maxRects - 1) ? " : " : ", ");
    }
    ASSERT_GL();
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

    const EGLint simpleSizes[] =
    {
        32, 32,
        64, 64,
        256, 256,
        512, 384,
        864, 480
    };

    const EGLint complexSizes[] =
    {
        32, 32,
        64, 64,
        256, 256,
    };

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

    test::printHeader("Testing basic functionality");
    result &= test::verifyResult(testSmoke);

    test::printHeader("Testing failure cases");
    result &= test::verifyResult(testFailureCases);

    test::printHeader("Testing single update");
    result &= test::verifyResult(testSingleUpdate);

    test::printHeader("Testing simple partial updates");
    result &= test::verifyResult(boost::bind(testPartialUpdates, 3));

    test::printHeader("Testing complex partial updates");
    result &= test::verifyResult(boost::bind(testPartialUpdates, 0));

    test::printHeader("Testing synchronization");
    result &= test::verifyResult(testSynchronization);

    for (unsigned int i = 0; i < sizeof(simpleSizes) / sizeof(simpleSizes[0]); i += 2)
    {
        test::printHeader("Testing %dx%d simple update performance",
                          simpleSizes[i], simpleSizes[i + 1]);
        result &= test::verifyResult(boost::bind(testSimplePerformance,
                    simpleSizes[i], simpleSizes[i + 1]));
    }

    for (unsigned int i = 0; i < sizeof(complexSizes) / sizeof(complexSizes[0]); i += 2)
    {
        test::printHeader("Testing %dx%d complex update performance",
                        complexSizes[i], complexSizes[i + 1]);
        result &= test::verifyResult(boost::bind(testComplexPerformance,
                    complexSizes[i], complexSizes[i + 1]));
    }

out:
    glDeleteProgram(program);
    util::destroyWindow();

    printf("================================================\n");
    printf("Result: ");
    test::printResult(result);

    return result ? 0 : 1;
}
