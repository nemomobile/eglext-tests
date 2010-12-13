/**
 * EGL_NOK_surface_scaling extension test
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
 * \author Tero Tiittanen <tero.tiittanen@nokia.com>
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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

/**
 * These tests do not rely on any particular scaling technology. This
 * means that there are no assumptions how the scaled surface content
 * is delivered onto the display.
 *
 * A consequency is that the tests cannot rely on the possibility to
 * read the data back from some point of the pipeline and check that
 * it has scaled appropriately. The tests thus concentrate on testing
 * the EGL logic.
 *
 * There is one readback test which works opposite to what is said
 * above: it tests that the rendered data has *not* scaled when it is
 * read back on GLES level but still appears for the application as
 * having the size of the fixed surface.
 *
 * Tests assume that 1:1 scaling for whole screen area is always
 * valid.
 *
 * Native window and EGL intialization is implemented locally because
 * the tests do part of the initialization (mainly surface creation)
 * themselves and thus the initialization is left halfway on purpose.
 */

PFNEGLSETSURFACESCALINGNOKPROC eglSetSurfaceScalingNOK;
PFNEGLQUERYSURFACESCALINGCAPABILITYNOKPROC eglQuerySurfaceScalingCapabilityNOK;

int winWidth = 864;
int winHeight = 480;
int winDepth = 16;

const int maxConfigs = 100;
const int maxScalingLimitSearchFactor = 256;

/**
 * Get scaling limits for a given configuration assuming that the
 * window surface is of the size of the screen.
 *
 * A helper method. Implements a simple linear search (capability
 * query method is fast). Returns 0 if the limit was not found.
 */
void getScalingLimits(EGLConfig *config, int *minWidth, int *maxWidth,
                      int *minHeight, int *maxHeight)
{
    int w, h;
    EGLint value;
    EGLBoolean ret;

    h = winHeight;

    for(w = winWidth - 1; w > 0; --w)
    {
        ret = eglQuerySurfaceScalingCapabilityNOK(util::ctx.dpy, *config,
                                                  winWidth, winHeight,
                                                  w, h,
                                                  &value);

        test::assert(ret == EGL_TRUE,
                     "Failed to query capability (%dx%d->%dx%d)",
                     winWidth, winHeight, w, h);
        ASSERT_EGL();

        if(value == EGL_NOT_SUPPORTED_NOK)
        {
            *minWidth = w + 1;
            break;
        }
    }
    if(w == 0)
    {
        *minWidth = 0;
    }

    for(w = winWidth + 1; w < maxScalingLimitSearchFactor * winWidth; ++w)
    {
        ret = eglQuerySurfaceScalingCapabilityNOK(util::ctx.dpy, *config,
                                                  winWidth, winHeight,
                                                  w, h,
                                                  &value);

        test::assert(ret == EGL_TRUE,
                     "Failed to query capability (%dx%d->%dx%d)",
                     winWidth, winHeight, w, h);
        ASSERT_EGL();

        if(value == EGL_NOT_SUPPORTED_NOK)
        {
            *maxWidth = w - 1;
            break;
        }
    }
    if(w == maxScalingLimitSearchFactor * winWidth)
    {
        *maxWidth = 0;
    }

    w = winWidth;

    for(h = winHeight - 1; h > 0; --h)
    {
        ret = eglQuerySurfaceScalingCapabilityNOK(util::ctx.dpy, *config,
                                                  winWidth, winHeight,
                                                  w, h,
                                                  &value);

        test::assert(ret == EGL_TRUE,
                     "Failed to query capability (%dx%d->%dx%d)",
                     winWidth, winHeight, w, h);
        ASSERT_EGL();

        if(value == EGL_NOT_SUPPORTED_NOK)
        {
            *minHeight = h + 1;
            break;
        }
    }
    if(h == 0)
    {
        *minHeight = 0;
    }

    for(h = winHeight + 1; h < maxScalingLimitSearchFactor * winHeight; ++h)
    {
        ret = eglQuerySurfaceScalingCapabilityNOK(util::ctx.dpy, *config,
                                                  winWidth, winHeight,
                                                  w, h,
                                                  &value);

        test::assert(ret == EGL_TRUE,
                     "Failed to query capability (%dx%d->%dx%d)",
                     winWidth, winHeight, w, h);
        ASSERT_EGL();

        if(value == EGL_NOT_SUPPORTED_NOK)
        {
            *maxHeight = h - 1;
            break;
        }
    }
    if(h == maxScalingLimitSearchFactor * winHeight)
    {
        *maxHeight = 0;
    }
}

/**
 * Verify that the needed extensions are present
 */
void testExtensionPresence()
{
    ASSERT(util::isEGLExtensionSupported("EGL_NOK_surface_scaling"));

    eglSetSurfaceScalingNOK =
        (PFNEGLSETSURFACESCALINGNOKPROC)eglGetProcAddress("eglSetSurfaceScalingNOK");
    eglQuerySurfaceScalingCapabilityNOK =
        (PFNEGLQUERYSURFACESCALINGCAPABILITYNOKPROC)eglGetProcAddress("eglQuerySurfaceScalingCapabilityNOK");

    ASSERT(eglSetSurfaceScalingNOK);
    ASSERT(eglQuerySurfaceScalingCapabilityNOK);
}

/**
 * Test configuration choosing and scaling attribute.
 *
 * Request configurations with and without scaling. Check that
 * eglGetConfigAttrib returns consistent values.
 */
void testConfigChoosing(bool scaling)
{
    EGLConfig configs[maxConfigs];
    int i;
    EGLint value;
    EGLint configCount = 0;

    EGLint configAttrs[] =
        {
            EGL_SURFACE_SCALING_NOK, 0,
            EGL_NONE
        };

    configAttrs[1] = scaling ? EGL_TRUE : EGL_FALSE;

    eglChooseConfig(util::ctx.dpy, (EGLint const *)configAttrs, configs, maxConfigs, &configCount);
    ASSERT_EGL();

    for(i = 0; i < configCount; i++)
    {
        eglGetConfigAttrib(util::ctx.dpy, configs[i], EGL_SURFACE_SCALING_NOK, &value);
        if(value != configAttrs[1])
        {
            test::fail("Config attribute not consistent\n");
        }
    }
}

/**
 * Test scaling capability query.
 *
 * Access configurations with eglQuerySurfaceScalingCapabilityNOK.
 * Test that non-scaling ones give EGL_BAD_MATCH. Test that the result
 * value does not change if returns EGL_FALSE.  For scaling
 * configuration, test that gets EGL_TRUE. Test that <=0 w/h gives
 * EGL_BAD_PARAMETER.
 *
 * This test does not check the scaling limits. That would not result
 * for extra information in this context. They are tested later when
 * the surfaces are tried to be initialized based on the found limits.
 */
void testCapabilityQuery(bool scaling)
{
    EGLConfig configs[maxConfigs];
    int i, j;
    EGLint value, valueIn;
    EGLint configCount = 0;
    EGLBoolean ret;

    EGLint const configAttrsScaling[] =
        {
            EGL_SURFACE_SCALING_NOK, EGL_TRUE,
            EGL_NONE
        };

    EGLint const configAttrsNonScaling[] =
        {
            EGL_SURFACE_SCALING_NOK, EGL_FALSE,
            EGL_NONE
        };

    if(scaling)
    {
        eglChooseConfig(util::ctx.dpy, configAttrsScaling, configs, maxConfigs, &configCount);
        ASSERT_EGL();

        for(i = 0; i < configCount; i++)
        {
            ret = eglQuerySurfaceScalingCapabilityNOK(util::ctx.dpy, configs[i],
                                                      winWidth, winHeight,
                                                      winWidth, winHeight,
                                                      &value);

            test::assert(ret == EGL_TRUE,
                         "Failed to query capability (%dx%d->%dx%d)",
                         winWidth, winHeight, winWidth, winHeight);

            /* Test -1 and 0 separately for window surface/extent width/height. */
            for(j = 0; j < 4 * 2; j++)
            {
                EGLint ww = (((j >> 1) & 3) == 0) ? (j & 1) - 1 : winWidth;
                EGLint wh = (((j >> 1) & 3) == 1) ? (j & 1) - 1 : winHeight;
                EGLint ew = (((j >> 1) & 3) == 2) ? (j & 1) - 1 : winWidth;
                EGLint eh = (((j >> 1) & 3) == 3) ? (j & 1) - 1 : winHeight;

                ASSERT_EGL();
                ret = eglQuerySurfaceScalingCapabilityNOK(util::ctx.dpy, configs[i],
                                                          ww, wh, ew, eh,
                                                          &value);

                test::assert(ret == EGL_FALSE,
                             "Querying illegal w/h does not fail correctly (%dx%d->%dx%d)",
                             ww, wh, ew, eh);
                test::assert(eglGetError() == EGL_BAD_PARAMETER,
                             "Querying illegal w/h does not fail correctly (%dx%d->%dx%d)",
                             ww, wh, ew, eh);
            }
        }
    }
    else
    {
        eglChooseConfig(util::ctx.dpy, configAttrsNonScaling, configs, maxConfigs, &configCount);
        ASSERT_EGL();

        for(i = 0; i < configCount; i++)
        {
            value = valueIn = rand();

            ASSERT_EGL();
            ret = eglQuerySurfaceScalingCapabilityNOK(util::ctx.dpy, configs[i],
                                                      winWidth, winHeight,
                                                      winWidth, winHeight,
                                                      &value);

            test::assert(ret == EGL_FALSE,
                         "Querying for non-scaling config does not fail correctly");
            test::assert(eglGetError() == EGL_BAD_MATCH,
                         "Querying for non-scaling config does not fail correctly");
            test::assert(value == valueIn,
                         "Querying for non-scaling config changes the value illegally");
        }
    }
}

/**
 * Test the creation of surfaces in relation to scaling.
 *
 * Create scaling surfaces. Test that creation NACKs, if RGB is
 * out-of-bounds, w/h <= 0 is given or if only some of scaling
 * attributes are given.
 */
void testSurfaceCreation()
{
    test::scoped<EGLSurface> surface(boost::bind(eglDestroySurface, util::ctx.dpy, _1));
    int i;

    EGLint surfaceAttrs[19] =
        {
            EGL_FIXED_WIDTH_NOK,            winWidth,
            EGL_FIXED_HEIGHT_NOK,           winHeight,
            EGL_TARGET_EXTENT_OFFSET_X_NOK, 0,
            EGL_TARGET_EXTENT_OFFSET_Y_NOK, 0,
            EGL_TARGET_EXTENT_WIDTH_NOK,    winWidth,
            EGL_TARGET_EXTENT_HEIGHT_NOK,   winHeight,
            EGL_BORDER_COLOR_RED_NOK,       0,
            EGL_BORDER_COLOR_GREEN_NOK,     0,
            EGL_BORDER_COLOR_BLUE_NOK,      0,
            EGL_NONE
        };

    EGLint surfaceAttrsTemp[19];

    EGLint colorValue[4] = {0, -1, 255, 256};

    /* Test -1, 0, 255 and 256 separately for R, G and B. */
    for(i = 0; i < 3 * 4; i++)
    {
        memcpy(surfaceAttrsTemp, surfaceAttrs, 19 * sizeof(EGLint));
        surfaceAttrsTemp[13 + 2 * ((i >> 2) & 3)] = colorValue[i & 3];
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrsTemp);

        if(i & 1)
        {
            test::assert(eglGetError() == EGL_BAD_ATTRIBUTE,
                         "Out-of-bound color does not fail correctly (component = %d)",
                         colorValue[i & 3]);
            test::assert(surface == EGL_NO_SURFACE,
                         "Out-of-bound color does not fail correctly (component = %d)",
                         colorValue[i & 3]);
        }
        else
        {
            ASSERT(eglGetError() == EGL_SUCCESS);
            ASSERT(surface != EGL_NO_SURFACE);

            eglDestroySurface(util::ctx.dpy, surface);
            surface = 0;
        }
    }

    /* Test -1 and 0 separately for window surface/extent width/height. */
    for(i = 0; i < 4 * 2; i++)
    {
        memcpy(surfaceAttrsTemp, surfaceAttrs, 19 * sizeof(EGLint));
        surfaceAttrsTemp[1]  = (((i >> 1) & 3) == 0) ? (i & 1) - 1 : winWidth;
        surfaceAttrsTemp[3]  = (((i >> 1) & 3) == 1) ? (i & 1) - 1 : winHeight;
        surfaceAttrsTemp[9]  = (((i >> 1) & 3) == 2) ? (i & 1) - 1 : winWidth;
        surfaceAttrsTemp[11] = (((i >> 1) & 3) == 3) ? (i & 1) - 1 : winHeight;
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrsTemp);

        test::assert(eglGetError() == EGL_BAD_ATTRIBUTE,
                     "Illegal w/h does not fail correctly (%dx%d->%dx%d)",
                     surfaceAttrsTemp[1], surfaceAttrsTemp[3], surfaceAttrsTemp[9], surfaceAttrsTemp[11]);
        test::assert(surface == EGL_NO_SURFACE,
                     "Illegal w/h does not fail correctly (%dx%d->%dx%d)",
                     surfaceAttrsTemp[1], surfaceAttrsTemp[3], surfaceAttrsTemp[9], surfaceAttrsTemp[11]);
    }

    /* Test incomplete surface attribute lists. */
    for(i = 2; i <= 10; i+=2)
    {
        memcpy(surfaceAttrsTemp, surfaceAttrs, 19 * sizeof(EGLint));
        surfaceAttrsTemp[i] = EGL_NONE;
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrsTemp);

        test::assert(eglGetError() == EGL_BAD_ATTRIBUTE,
                     "Incomplete attribute list does not fail correctly");
        test::assert(surface == EGL_NO_SURFACE,
                     "Incomplete attribute list does not fail correctly");
    }
}

/**
 * Check surface creation against the scaling limits.
 *
 * Search scaling limits with getScalingLimits. If found, try to
 * create surfaces which should / should not work and test for
 * consistency.
 */
void testSurfaceCreationAgainstLimits()
{
    int minWidth, maxWidth, minHeight, maxHeight;
    test::scoped<EGLSurface> surface(boost::bind(eglDestroySurface, util::ctx.dpy, _1));

    EGLint surfaceAttrs[19] =
        {
            EGL_FIXED_WIDTH_NOK,            winWidth,
            EGL_FIXED_HEIGHT_NOK,           winHeight,
            EGL_TARGET_EXTENT_OFFSET_X_NOK, 0,
            EGL_TARGET_EXTENT_OFFSET_Y_NOK, 0,
            EGL_TARGET_EXTENT_WIDTH_NOK,    0,
            EGL_TARGET_EXTENT_HEIGHT_NOK,   0,
            EGL_BORDER_COLOR_RED_NOK,       0,
            EGL_BORDER_COLOR_GREEN_NOK,     0,
            EGL_BORDER_COLOR_BLUE_NOK,      0,
            EGL_NONE
        };

    getScalingLimits(&util::ctx.config, &minWidth, &maxWidth, &minHeight, &maxHeight);

    printf("%dx%d -> (%d..%d)x(%d..%d) ", winWidth, winHeight, minWidth, maxWidth, minHeight, maxHeight);

    if(minWidth > 1)
    {
        surfaceAttrs[9]  = minWidth - 1;
        surfaceAttrs[11] = winHeight;
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

        test::assert(eglGetError() == EGL_BAD_ATTRIBUTE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);
        test::assert(surface == EGL_NO_SURFACE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);

        surfaceAttrs[9]  = minWidth;
        surfaceAttrs[11] = winHeight;
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

        test::assert(eglGetError() == EGL_SUCCESS,
                     "Ok target extent fails (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);
        test::assert(surface != EGL_NO_SURFACE,
                     "Ok target extent fails (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);

        eglDestroySurface(util::ctx.dpy, surface);
        surface = 0;
    }

    if(maxWidth > 0)
    {
        surfaceAttrs[9]  = maxWidth + 1;
        surfaceAttrs[11] = winHeight;
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

        test::assert(eglGetError() == EGL_BAD_ATTRIBUTE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);
        test::assert(surface == EGL_NO_SURFACE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);

        surfaceAttrs[9]  = maxWidth;
        surfaceAttrs[11] = winHeight;
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

        test::assert(eglGetError() == EGL_SUCCESS,
                     "Ok target extent fails (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);
        test::assert(surface != EGL_NO_SURFACE,
                     "Ok target extent fails (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);

        eglDestroySurface(util::ctx.dpy, surface);
        surface = 0;
    }

    if(minHeight > 1)
    {
        surfaceAttrs[9]  = winWidth;
        surfaceAttrs[11] = minHeight - 1;
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

        test::assert(eglGetError() == EGL_BAD_ATTRIBUTE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);
        test::assert(surface == EGL_NO_SURFACE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);

        surfaceAttrs[9]  = winWidth;
        surfaceAttrs[11] = minHeight;
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

        test::assert(eglGetError() == EGL_SUCCESS,
                     "Ok target extent fails (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);
        test::assert(surface != EGL_NO_SURFACE,
                     "Ok target extent fails (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);

        eglDestroySurface(util::ctx.dpy, surface);
        surface = 0;
    }

    if(maxHeight > 0)
    {
        surfaceAttrs[9]  = winWidth;
        surfaceAttrs[11] = maxHeight + 1;
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

        test::assert(eglGetError() == EGL_BAD_ATTRIBUTE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);
        test::assert(surface == EGL_NO_SURFACE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);

        surfaceAttrs[9]  = winWidth;
        surfaceAttrs[11] = maxHeight;
        surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

        test::assert(eglGetError() == EGL_SUCCESS,
                     "Ok target extent fails (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);
        test::assert(surface != EGL_NO_SURFACE,
                     "Ok target extent fails (%dx%d)",
                     surfaceAttrs[9], surfaceAttrs[11]);

        eglDestroySurface(util::ctx.dpy, surface);
        surface = 0;
    }
}

/**
 * Test resizing the target extent.
 *
 * Create a valid surface and try to resize it with
 * eglSetSurfaceScalingNOK with valid and invalid sizes, check for
 * return value. When scaling is inside limits, read extent back and
 * check.
 */
void testResizingExtent()
{
    int minWidth, maxWidth, minHeight, maxHeight;
    test::scoped<EGLSurface> surface(boost::bind(eglDestroySurface, util::ctx.dpy, _1));
    EGLBoolean ret;

    EGLint surfaceAttrs[19] =
        {
            EGL_FIXED_WIDTH_NOK,            winWidth,
            EGL_FIXED_HEIGHT_NOK,           winHeight,
            EGL_TARGET_EXTENT_OFFSET_X_NOK, 0,
            EGL_TARGET_EXTENT_OFFSET_Y_NOK, 0,
            EGL_TARGET_EXTENT_WIDTH_NOK,    winWidth,
            EGL_TARGET_EXTENT_HEIGHT_NOK,   winHeight,
            EGL_BORDER_COLOR_RED_NOK,       0,
            EGL_BORDER_COLOR_GREEN_NOK,     0,
            EGL_BORDER_COLOR_BLUE_NOK,      0,
            EGL_NONE
        };

    surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

    ASSERT(eglGetError() == EGL_SUCCESS);
    ASSERT(surface != EGL_NO_SURFACE);

    getScalingLimits(&util::ctx.config, &minWidth, &maxWidth, &minHeight, &maxHeight);

    if(minWidth > 1)
    {
        ret = eglSetSurfaceScalingNOK(util::ctx.dpy, surface, 0, 0, minWidth - 1, winHeight);
        test::assert(eglGetError() == EGL_BAD_PARAMETER,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     minWidth - 1, winHeight);
        test::assert(ret == EGL_FALSE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     minWidth - 1, winHeight);

        ret = eglSetSurfaceScalingNOK(util::ctx.dpy, surface, 0, 0, minWidth, winHeight);
        test::assert(eglGetError() == EGL_SUCCESS,
                     "Ok target extent fails (%dx%d)",
                     minWidth, winHeight);
        test::assert(ret == EGL_TRUE,
                     "Ok target extent fails (%dx%d)",
                     minWidth, winHeight);
    }

    if(maxWidth > 0)
    {
        ret = eglSetSurfaceScalingNOK(util::ctx.dpy, surface, 0, 0, maxWidth + 1, winHeight);
        test::assert(eglGetError() == EGL_BAD_PARAMETER,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     maxWidth + 1, winHeight);
        test::assert(ret == EGL_FALSE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     maxWidth + 1, winHeight);

        ret = eglSetSurfaceScalingNOK(util::ctx.dpy, surface, 0, 0, maxWidth, winHeight);
        test::assert(eglGetError() == EGL_SUCCESS,
                     "Ok target extent fails (%dx%d)",
                     maxWidth, winHeight);
        test::assert(ret == EGL_TRUE,
                     "Ok target extent fails (%dx%d)",
                     maxWidth, winHeight);
    }

    if(minHeight > 1)
    {
        ret = eglSetSurfaceScalingNOK(util::ctx.dpy, surface, 0, 0, winWidth, minHeight - 1);
        test::assert(eglGetError() == EGL_BAD_PARAMETER,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     winWidth, minHeight - 1);
        test::assert(ret == EGL_FALSE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     winWidth, minHeight - 1);

        ret = eglSetSurfaceScalingNOK(util::ctx.dpy, surface, 0, 0, winWidth, minHeight);
        test::assert(eglGetError() == EGL_SUCCESS,
                     "Ok target extent fails (%dx%d)",
                     winWidth, minHeight);
        test::assert(ret == EGL_TRUE,
                     "Ok target extent fails (%dx%d)",
                     winWidth, minHeight);
    }

    if(maxHeight > 0)
    {
        ret = eglSetSurfaceScalingNOK(util::ctx.dpy, surface, 0, 0, winWidth, maxHeight + 1);
        test::assert(eglGetError() == EGL_BAD_PARAMETER,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     winWidth, maxHeight + 1);
        test::assert(ret == EGL_FALSE,
                     "Illegal target extent does not fail correctly (%dx%d)",
                     winWidth, maxHeight + 1);

        ret = eglSetSurfaceScalingNOK(util::ctx.dpy, surface, 0, 0, winWidth, maxHeight);
        test::assert(eglGetError() == EGL_SUCCESS,
                     "Ok target extent fails (%dx%d)",
                     winWidth, maxHeight);
        test::assert(ret == EGL_TRUE,
                     "Ok target extent fails (%dx%d)",
                     winWidth, maxHeight);
    }

    // Swapping for sanity checking
    eglMakeCurrent(util::ctx.dpy, surface, surface, util::ctx.context);
    ASSERT_EGL();
    eglSwapBuffers(util::ctx.dpy, surface);
    ASSERT_EGL();
    eglMakeCurrent(util::ctx.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    eglDestroySurface(util::ctx.dpy, surface);
    surface = 0;
}

/**
 * Test changing the border color.
 *
 * Change border color for a scaling surface and read it back. Test
 * that out-of-bound NACKs correctly.
 */
void testChangingBorderColor()
{
    test::scoped<EGLSurface> surface(boost::bind(eglDestroySurface, util::ctx.dpy, _1));
    EGLBoolean ret;
    int i;

    EGLint surfaceAttrs[19] =
        {
            EGL_FIXED_WIDTH_NOK,            winWidth,
            EGL_FIXED_HEIGHT_NOK,           winHeight,
            EGL_TARGET_EXTENT_OFFSET_X_NOK, 0,
            EGL_TARGET_EXTENT_OFFSET_Y_NOK, 0,
            EGL_TARGET_EXTENT_WIDTH_NOK,    winWidth,
            EGL_TARGET_EXTENT_HEIGHT_NOK,   winHeight,
            EGL_BORDER_COLOR_RED_NOK,       0,
            EGL_BORDER_COLOR_GREEN_NOK,     0,
            EGL_BORDER_COLOR_BLUE_NOK,      0,
            EGL_NONE
        };

    EGLint colorValue[4] = {0, -1, 255, 256};
    EGLint attrib;
    EGLint value;

    surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

    ASSERT(eglGetError() == EGL_SUCCESS);
    ASSERT(surface != EGL_NO_SURFACE);

    for(i = 0; i < 3 * 4; i++)
    {
        if(((i >> 2) & 3) == 0)
        {
            attrib = EGL_BORDER_COLOR_RED_NOK;
        }
        else if(((i >> 2) & 3) == 1)
        {
            attrib = EGL_BORDER_COLOR_GREEN_NOK;
        }
        else
        {
            attrib = EGL_BORDER_COLOR_BLUE_NOK;
        }

        ret = eglSurfaceAttrib(util::ctx.dpy, surface, attrib, 128);
        test::assert(ret == EGL_TRUE,
                     "Failed to set color attribute with legal value 128");

        ret = eglSurfaceAttrib(util::ctx.dpy, surface, attrib, colorValue[i & 3]);
        if(i & 1)
        {
            test::assert(eglGetError() == EGL_BAD_PARAMETER,
                         "Out-of-bound color does not fail correctly (component = %d)",
                         colorValue[i & 3]);
            test::assert(ret == EGL_FALSE,
                         "Out-of-bound color does not fail correctly (component = %d)",
                         colorValue[i & 3]);
        }
        else
        {
            test::assert(eglGetError() == EGL_SUCCESS,
                         "Setting valid color fails (component = %d)",
                         colorValue[i & 3]);
            test::assert(ret == EGL_TRUE,
                         "Setting valid color fails (component = %d)",
                         colorValue[i & 3]);
        }
        eglQuerySurface(util::ctx.dpy, surface, attrib, &value);
        if(i & 1)
        {
            test::assert(value == 128,
                         "Setting out-of-bound color changes the target value illegally");
        }
        else
        {
            test::assert(value == colorValue[i & 3],
                         "Setting ok color does not change the target value");
        }
    }

    // Swapping for sanity checking
    eglMakeCurrent(util::ctx.dpy, surface, surface, util::ctx.context);
    ASSERT_EGL();
    eglSwapBuffers(util::ctx.dpy, surface);
    ASSERT_EGL();
    eglMakeCurrent(util::ctx.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    eglDestroySurface(util::ctx.dpy, surface);
    surface = 0;
}

/**
 * Test clipping of target extent.
 *
 * Move the target extent to clipping positions, swap and check that
 * everything is ok.
 */
void testClipping()
{
    test::scoped<EGLSurface> surface(boost::bind(eglDestroySurface, util::ctx.dpy, _1));
    EGLBoolean ret;
    int x, y;

    EGLint surfaceAttrs[19] =
        {
            EGL_FIXED_WIDTH_NOK,            winWidth,
            EGL_FIXED_HEIGHT_NOK,           winHeight,
            EGL_TARGET_EXTENT_OFFSET_X_NOK, 0,
            EGL_TARGET_EXTENT_OFFSET_Y_NOK, 0,
            EGL_TARGET_EXTENT_WIDTH_NOK,    winWidth,
            EGL_TARGET_EXTENT_HEIGHT_NOK,   winHeight,
            EGL_BORDER_COLOR_RED_NOK,       50,
            EGL_BORDER_COLOR_GREEN_NOK,     150,
            EGL_BORDER_COLOR_BLUE_NOK,      250,
            EGL_NONE
        };

    surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

    ASSERT(eglGetError() == EGL_SUCCESS);
    ASSERT(surface != EGL_NO_SURFACE);

    eglMakeCurrent(util::ctx.dpy, surface, surface, util::ctx.context);
    ASSERT_EGL();

    for(x = -3 * winWidth / 2; x <= 3 * winWidth / 2; x += 6 * winWidth / 10)
    {
        for(y = -3 * winHeight / 2; y <= 3 * winHeight / 2; y += 6 * winHeight / 10)
        {
            ret = eglSetSurfaceScalingNOK(util::ctx.dpy, surface, x, y, winWidth, winHeight);

            test::assert(eglGetError() == EGL_SUCCESS,
                         "Ok target extent fails (%dx%d) at (%d,%d)",
                         winWidth, winHeight, x, y);
            test::assert(ret == EGL_TRUE,
                         "Ok target extent fails (%dx%d) at (%d,%d)",
                         winWidth, winHeight, x, y);

            // Swapping for sanity checking
            eglSwapBuffers(util::ctx.dpy, surface);
            ASSERT_EGL();
        }
    }

    eglMakeCurrent(util::ctx.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(util::ctx.dpy, surface);
    surface = 0;
}

/**
 * Test rendering to scaling surface.
 *
 * Render and check that it is rendered 1:1 no matter which scaling
 * factor.
 */
void testRendering()
{
    test::scoped<EGLSurface> surface(boost::bind(eglDestroySurface, util::ctx.dpy, _1));
    int x, y, i, j;
    unsigned int pixel;

    EGLint surfaceAttrs[19] =
        {
            EGL_FIXED_WIDTH_NOK,            winWidth,
            EGL_FIXED_HEIGHT_NOK,           winHeight,
            EGL_TARGET_EXTENT_OFFSET_X_NOK, 0,
            EGL_TARGET_EXTENT_OFFSET_Y_NOK, 0,
            EGL_TARGET_EXTENT_WIDTH_NOK,    2 * winWidth / 3,
            EGL_TARGET_EXTENT_HEIGHT_NOK,   2 * winHeight / 3,
            EGL_BORDER_COLOR_RED_NOK,       50,
            EGL_BORDER_COLOR_GREEN_NOK,     150,
            EGL_BORDER_COLOR_BLUE_NOK,      250,
            EGL_NONE
        };

    surface = eglCreateWindowSurface(util::ctx.dpy, util::ctx.config, util::ctx.win, surfaceAttrs);

    ASSERT(eglGetError() == EGL_SUCCESS);
    ASSERT(surface != EGL_NO_SURFACE);

    eglMakeCurrent(util::ctx.dpy, surface, surface, util::ctx.context);
    ASSERT_EGL();

    GLint program = util::createProgram(test::color::vertSource, test::color::fragSource);
    glUseProgram(program);

    for(i = 0 ; i < winWidth / 4; ++i)
    {
        GLfloat r = (i & 1) ? 1.0f : 0.0f;
        GLfloat g = (i & 1) ? 1.0f : 0.0f;
        GLfloat b = (i & 1) ? 1.0f : 0.0f;
        test::color::drawQuad(i, i, 1, 1, r, g, b);
        test::color::drawQuad(winWidth - 1 - i, i, 1, 1, r, g, b);
        test::color::drawQuad(i, winHeight - 1 - i, 1, 1, r, g, b);
        test::color::drawQuad(winWidth - 1 - i, winHeight - 1 - i, 1, 1, r, g, b);
    }

    for(i = 0 ; i < winWidth / 4; ++i)
    {
        for(j = 0; j < 4; j++)
        {
            switch(j)
            {
            case 0:
                x = i; y = i; break;
            case 1:
                x = i; y = winHeight - 1 - i; break;
            case 2:
                x = winWidth - 1 - i; y = i; break;
            default:
                x = winWidth - 1 - i; y = winHeight - 1 - i; break;
            }
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

            unsigned char r1 = 255 * (i & 1);
            unsigned char g1 = 255 * (i & 1);
            unsigned char b1 = 255 * (i & 1);
            unsigned char a1 = 255;
            unsigned char b2 = (pixel & 0x000000ff);
            unsigned char g2 = (pixel & 0x0000ff00) >> 8;
            unsigned char r2 = (pixel & 0x00ff0000) >> 16;
            unsigned char a2 = (pixel & 0xff000000) >> 24;
            int t = 8;

            if (abs(r1 - r2) > t ||
                abs(g1 - g2) > t ||
                abs(b1 - b2) > t ||
                abs(a1 - a2) > t)
            {
                test::fail("Image comparison failed at (%d, %d), expected %02x%02x%02x%02x, "
                           "got %02x%02x%02x%02x\n", x, y, r1, g1, b1, a1, r2, g2, b2, a2);
            }
        }
    }

    eglSwapBuffers(util::ctx.dpy, surface);
    ASSERT_EGL();

    glDeleteProgram(program);
    eglMakeCurrent(util::ctx.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(util::ctx.dpy, surface);
    surface = 0;
}

/**
 * Auxiliary method.
 *
 * Inits native window and EGL up to eglCreateContext.
 */
bool initWindow()
{
    EGLint configCount = 0;

    const EGLint configAttrs[] =
        {
            EGL_BUFFER_SIZE,         winDepth,
            EGL_SURFACE_TYPE,        EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE,     EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_SCALING_NOK, EGL_TRUE,
            EGL_NONE
        };

    const EGLint contextAttrs[] =
        {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

    nativeCreateDisplay(&util::ctx.nativeDisplay);

    util::ctx.dpy = eglGetDisplay(util::ctx.nativeDisplay);
    ASSERT_EGL();

    eglInitialize(util::ctx.dpy, NULL, NULL);
    eglChooseConfig(util::ctx.dpy, configAttrs, &util::ctx.config, 1, &configCount);
    ASSERT_EGL();

    if (!configCount)
    {
        printf("Config not found\n");
        goto out_error;
    }

    if (!nativeCreateWindow(util::ctx.nativeDisplay, util::ctx.dpy, util::ctx.config, __FILE__,
                            winWidth, winHeight, &util::ctx.win))
    {
        printf("Unable to create a window\n");
        goto out_error;
    }

    util::ctx.context = eglCreateContext(util::ctx.dpy, util::ctx.config, EGL_NO_CONTEXT, contextAttrs);
    ASSERT_EGL();
    if (!util::ctx.context)
    {
        printf("Unable to create a context\n");
        goto out_error;
    }
    return true;

  out_error:
    eglDestroyContext(util::ctx.dpy, util::ctx.context);
    eglTerminate(util::ctx.dpy);
    nativeDestroyWindow(util::ctx.nativeDisplay, util::ctx.win);
    nativeDestroyDisplay(util::ctx.nativeDisplay);
    return false;
}

/**
 * Auxiliary method.
 */
void deinitWindow()
{
    eglDestroyContext(util::ctx.dpy, util::ctx.context);
    eglTerminate(util::ctx.dpy);
    nativeDestroyWindow(util::ctx.nativeDisplay, util::ctx.win);
    nativeDestroyDisplay(util::ctx.nativeDisplay);
}

int main(int argc, char** argv)
{
    EGLNativeDisplayType dpy;
    nativeCreateDisplay(&dpy);
    nativeGetDisplayProperties(dpy, &winWidth, &winHeight, &winDepth);
    nativeDestroyDisplay(dpy);

    bool res, result;

    res = initWindow();
    ASSERT(res);
    ASSERT_EGL();

    test::printHeader("Testing extension presence");
    result = test::verifyResult(testExtensionPresence);

    if (!result)
    {
        goto out;
    }

    test::printHeader("Testing config choosing, scaling");
    result &= test::verifyResult(boost::bind(testConfigChoosing, true));
    test::printHeader("Testing config choosing, non-scaling");
    result &= test::verifyResult(boost::bind(testConfigChoosing, false));
    test::printHeader("Testing capability query, scaling");
    result &= test::verifyResult(boost::bind(testCapabilityQuery, true));
    test::printHeader("Testing capability query, non-scaling");
    result &= test::verifyResult(boost::bind(testCapabilityQuery, false));
    test::printHeader("Testing surface creation");
    result &= test::verifyResult(testSurfaceCreation);
    test::printHeader("Testing surface creation against limits");
    result &= test::verifyResult(testSurfaceCreationAgainstLimits);

    test::printHeader("Testing resizing extent");
    result &= test::verifyResult(testResizingExtent);
    test::printHeader("Testing changing border color");
    result &= test::verifyResult(testChangingBorderColor);
    test::printHeader("Testing clipping");
    result &= test::verifyResult(testClipping);
    test::printHeader("Testing rendering");
    result &= test::verifyResult(testRendering);

out:
    deinitWindow();

    printf("================================================\n");
    printf("Result: ");
    test::printResult(result);

    return result ? 0 : 1;
}
