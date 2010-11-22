/**
 * EGL_NOK_shared_image conformance test
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

#include <boost/scoped_array.hpp>

#include "ext.h"
#include "native.h"
#include "util.h"
#include "testutil.h"

static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
static PFNEGLCREATESHAREDIMAGENOKPROC eglCreateSharedImageNOK;
static PFNEGLDESTROYSHAREDIMAGENOKPROC eglDestroySharedImageNOK;
static PFNEGLQUERYIMAGENOKPROC eglQueryImageNOK;

/**
 *  Verify that the needed extensions are present
 */
void testExtensionPresence()
{
    ASSERT(util::isEGLExtensionSupported("EGL_KHR_image_base"));
    ASSERT(util::isEGLExtensionSupported("EGL_NOK_image_shared"));

    eglCreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR =
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    eglCreateSharedImageNOK =
        (PFNEGLCREATESHAREDIMAGENOKPROC)eglGetProcAddress("eglCreateSharedImageNOK");
    eglDestroySharedImageNOK =
        (PFNEGLDESTROYSHAREDIMAGENOKPROC)eglGetProcAddress("eglDestroySharedImageNOK");
    eglQueryImageNOK =
        (PFNEGLQUERYIMAGENOKPROC)eglGetProcAddress("eglQueryImageNOK");

    ASSERT(eglCreateImageKHR);
    ASSERT(eglDestroyImageKHR);
    ASSERT(glEGLImageTargetTexture2DOES);
    ASSERT(eglCreateSharedImageNOK);
    ASSERT(eglDestroySharedImageNOK);
    ASSERT(eglQueryImageNOK);
}

/**
 *  Test known invalid inputs.
 *
 *  1. Invalid shared image handle.
 *  2. Queries from an invalid EGLImage.
 */
void testFailureCases()
{
    /* Invalid image */
    EGLNativeSharedImageTypeNOK sharedImage = eglCreateSharedImageNOK(util::ctx.dpy, EGL_NO_IMAGE_KHR, NULL);
    ASSERT(sharedImage == (EGLNativeSharedImageTypeNOK)0);
    ASSERT(eglGetError() != EGL_SUCCESS);

    ASSERT(eglDestroySharedImageNOK(util::ctx.dpy, (EGLNativeSharedImageTypeNOK)0) == EGL_FALSE);
    ASSERT(eglGetError() != EGL_SUCCESS);

    /* Invalid queries */
    EGLint dummy;
    ASSERT(eglQueryImageNOK(util::ctx.dpy, EGL_NO_IMAGE_KHR, EGL_WIDTH, &dummy) == EGL_FALSE);
    ASSERT(eglGetError() != EGL_SUCCESS);
}

/**
 *  Test shared images with OpenGL ES textures.
 *
 *  1. Load texture from @fileName with @width x @height dimensions using
 *     @format and @type.
 *  2. Blit the texture onto the screen.
 *  3. Verify colors from the test pattern.
 *  4. Create an EGLImage from the texture.
 *  5. Bind the EGLImage into a second texture.
 *  6. Blit the second texture onto the screen.
 *  7. Verify colors from the test pattern.
 *  8. Clone the EGLImage into a shared image.
 *  9. Verify properties of the shared image.
 * 10. Create a second EGLImage from the shared image.
 * 11. Bind the second EGLImage into a third texture.
 * 12. Blit the third texture onto the screen.
 * 13. Verify colors from the test pattern.
 *
 * Additionally, for renderable formats:
 *
 *  1. Make the corner of the texture white using glTexSubImage2D.
 *  2. Verify that the change is reflected in the texture created from the
 *     shared image.
 */
void testTextures(GLint format, GLint type, int width, int height, const
                  std::string& fileName, const uint8_t* color)
{
    EGLImageKHR image1, image2;
    GLuint sourceTexture, targetTexture1, targetTexture2;
    test::scoped<EGLNativeSharedImageTypeNOK> sharedImage(
            boost::bind(eglDestroySharedImageNOK, util::ctx.dpy, _1));
    int scale = 1;
    int spacing = scale * (width + 32);
    int offset = 0;
    const uint8_t black[] = {0x00, 0x00, 0x00, 0xff};
    const uint8_t white[] = {0xff, 0xff, 0xff, 0xff};
    const uint8_t* color2 = (format == GL_ALPHA) ? black : white;

    const EGLint imageAttributes[] =
    {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };

    const EGLint sharedImageAttributes[] =
    {
        EGL_NONE
    };

    /* Load the texture */
    glGenTextures(1, &sourceTexture);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (format == GL_ETC1_RGB8_OES)
    {
        bool texLoaded = util::loadCompressedTexture(GL_TEXTURE_2D, 0, format, width,
                                                     height, fileName);
        ASSERT(texLoaded);
    }
    else
    {
        bool texLoaded = util::loadRawTexture(GL_TEXTURE_2D, 0, format, width,
                                              height, format, type, fileName);
        ASSERT(texLoaded);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL();

    /* Draw the loaded texture */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    test::drawQuad(offset, 0, width * scale, height * scale);
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height / 2) * scale, color2)); // center
    ASSERT(test::checkColor(offset + (4)         * scale, (4)          * scale, color2)); // lower left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (4)          * scale, color));  // lower middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (4)          * scale, color2)); // lower right
    ASSERT(test::checkColor(offset + (4)         * scale, (height - 4) * scale, color));  // upper left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height - 4) * scale, color));  // upper middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (height - 4) * scale, color));  // upper right

    /* Create an EGL image from the texture */
    image1 = eglCreateImageKHR(util::ctx.dpy, util::ctx.context, EGL_GL_TEXTURE_2D_KHR,
                               (EGLClientBuffer)sourceTexture, imageAttributes);
    ASSERT_EGL();

    /* Bind the image to a texture */
    glGenTextures(1, &targetTexture1);
    glBindTexture(GL_TEXTURE_2D, targetTexture1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image1);
    ASSERT_GL();

    offset += spacing;
    test::drawQuad(offset, 0, width * scale, height * scale);
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height / 2) * scale, color2)); // center
    ASSERT(test::checkColor(offset + (4)         * scale, (4)          * scale, color2)); // lower left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (4)          * scale, color));  // lower middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (4)          * scale, color2)); // lower right
    ASSERT(test::checkColor(offset + (4)         * scale, (height - 4) * scale, color));  // upper left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height - 4) * scale, color));  // upper middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (height - 4) * scale, color));  // upper right

    /* Clone the image into a shared image */
    sharedImage= eglCreateSharedImageNOK(util::ctx.dpy, image1, sharedImageAttributes);
    ASSERT_EGL();

    /* Destroy the source texture and image */
    eglDestroyImageKHR(util::ctx.dpy, image1);
    glDeleteTextures(1, &sourceTexture);
    glDeleteTextures(1, &targetTexture1);

    /* Bind the shared image to an image */
    image2 = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT,
                               EGL_SHARED_IMAGE_NOK,
                               (EGLClientBuffer)(intptr_t)sharedImage,
                               imageAttributes);
    ASSERT_EGL();

    /* Verify the properties of the image */
    EGLint imageWidth = 0, imageHeight = 0;
    ASSERT(eglQueryImageNOK(util::ctx.dpy, image2, EGL_WIDTH, &imageWidth));
    ASSERT(eglQueryImageNOK(util::ctx.dpy, image2, EGL_HEIGHT, &imageHeight));
    ASSERT_EGL();
    ASSERT(imageWidth == width);
    ASSERT(imageHeight == height);

    /* Bind the image to a texture */
    glGenTextures(1, &targetTexture2);
    glBindTexture(GL_TEXTURE_2D, targetTexture2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image2);
    ASSERT_GL();

    /* Draw the texture */
    offset += spacing;
    test::drawQuad(offset, 0, width * scale, height * scale);
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height / 2) * scale, color2)); // center
    ASSERT(test::checkColor(offset + (4)         * scale, (4)          * scale, color2)); // lower left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (4)          * scale, color));  // lower middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (4)          * scale, color2)); // lower right
    ASSERT(test::checkColor(offset + (4)         * scale, (height - 4) * scale, color));  // upper left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height - 4) * scale, color));  // upper middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (height - 4) * scale, color));  // upper right

    if (format != GL_ETC1_RGB8_OES)
    {
        EGLImageKHR image3;
        GLuint targetTexture3;

        /* Test modification -- bind another image to the same shared image */
        image3 = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT,
                                   EGL_SHARED_IMAGE_NOK,
                                   (EGLClientBuffer)(intptr_t)sharedImage,
                                   imageAttributes);
        ASSERT_EGL();

        /* Bind the image to a texture */
        glGenTextures(1, &targetTexture3);
        glBindTexture(GL_TEXTURE_2D, targetTexture3);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image3);
        ASSERT_GL();

        /* Fill one corner with solid white */
        {
            boost::scoped_array<uint8_t> whiteBuffer(new uint8_t[width * height * 4]);
            memset(&whiteBuffer[0], 0xff, width * height * 4);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 15, 15, format, type, &whiteBuffer[0]);
            ASSERT_GL();
        }

        /* Make sure the modification is visible to the first EGL image target */
        glBindTexture(GL_TEXTURE_2D, targetTexture2);
        offset += spacing;
	test::drawQuad(offset, 0, width * scale, height * scale);
        ASSERT(test::checkColor(offset + (width / 2) * scale, (height / 2) * scale, color2)); // center
        ASSERT(test::checkColor(offset + (4)         * scale, (4)          * scale, color2)); // lower left
        ASSERT(test::checkColor(offset + (width / 2) * scale, (4)          * scale, color));  // lower middle
        ASSERT(test::checkColor(offset + (width - 4) * scale, (4)          * scale, color2)); // lower right
        ASSERT(test::checkColor(offset + (4)         * scale, (height - 4) * scale, color2)); // upper left
        ASSERT(test::checkColor(offset + (width / 2) * scale, (height - 4) * scale, color));  // upper middle
        ASSERT(test::checkColor(offset + (width - 4) * scale, (height - 4) * scale, color));  // upper right

        /* Clean up */
        glDeleteTextures(1, &targetTexture3);
        eglDestroyImageKHR(util::ctx.dpy, image3);
    }

    /* Clean up */
    glDeleteTextures(1, &targetTexture2);
    eglDestroyImageKHR(util::ctx.dpy, image2);
}

/**
 *  Test shared images with OpenGL ES framebuffers.
 *
 *  1. Create a texture of size @width x @height using @format and @type.
 *  2. Bind the texture into a framebuffer via a renderbuffer.
 *  3. Render a test pattern.
 *  4. Blit the texture to the screen and verify test pattern.
 *  5. Create an EGLImage from the texture.
 *  6. Bind the EGLImage to a second texture.
 *  7. Blit the second texture onto the screen.
 *  8. Verify colors from the test pattern.
 *  9. Clone the EGLImage into a shared image.
 * 10. Create a second EGLImage from the shared image.
 * 11. Bind the second EGLImage into a third texture.
 * 12. Blit the third texture onto the screen.
 * 13. Verify colors from the test pattern.
 */
void testFramebuffers(GLint format, GLint type, int width, int height,
                      const uint8_t* color)
{
    EGLImageKHR image1, image2;
    GLuint sourceTexture, targetTexture1, targetTexture2;
    GLuint framebuffer;
    test::scoped<EGLNativeSharedImageTypeNOK> sharedImage(
            boost::bind(eglDestroySharedImageNOK, util::ctx.dpy, _1));
    int scale = 1;
    int spacing = scale * (width + 32);
    int offset = 0;
    const uint8_t white[] = {0xff, 0xff, 0xff, 0xff};

    const EGLint imageAttributes[] =
    {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };

    const EGLint sharedImageAttributes[] =
    {
        EGL_NONE
    };

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    /* Create the texture */
    glGenTextures(1, &sourceTexture);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, type, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL();

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sourceTexture, 0);
    ASSERT_GL();

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    ASSERT(status == GL_FRAMEBUFFER_COMPLETE);

    /* Render the test pattern into the framebuffer */
    int b = 16;
    glViewport(0, 0, width, height);
    glClearColor(color[0] / 255.f, color[1] / 255.f, color[2] / 255.f, color[3] / 255.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor(b, b, width - 2 * b, height - 2 * b);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor(0, height - b, b, b);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor(width - b, height - b, b, b);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    ASSERT_GL();

    /* Draw the filled texture */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    test::drawQuad(offset, 0, width * scale, height * scale);
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height / 2) * scale, white)); // center
    ASSERT(test::checkColor(offset + (4)         * scale, (4)          * scale, white)); // lower left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (4)          * scale, color)); // lower middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (4)          * scale, white)); // lower right
    ASSERT(test::checkColor(offset + (4)         * scale, (height - 4) * scale, color)); // upper left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height - 4) * scale, color)); // upper middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (height - 4) * scale, color)); // upper right

    /* Create an EGL image from the texture */
    image1 = eglCreateImageKHR(util::ctx.dpy, util::ctx.context, EGL_GL_TEXTURE_2D_KHR,
                               (EGLClientBuffer)sourceTexture, imageAttributes);
    ASSERT_EGL();

    /* Bind the image to a texture */
    glGenTextures(1, &targetTexture1);
    glBindTexture(GL_TEXTURE_2D, targetTexture1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image1);
    ASSERT_GL();

    offset += spacing;
    test::drawQuad(offset, 0, width * scale, height * scale);
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height / 2) * scale, white)); // center
    ASSERT(test::checkColor(offset + (4)         * scale, (4)          * scale, white)); // lower left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (4)          * scale, color)); // lower middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (4)          * scale, white)); // lower right
    ASSERT(test::checkColor(offset + (4)         * scale, (height - 4) * scale, color)); // upper left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height - 4) * scale, color)); // upper middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (height - 4) * scale, color)); // upper right

    /* Clone the image into a shared image */
    sharedImage = eglCreateSharedImageNOK(util::ctx.dpy, image1, sharedImageAttributes);
    ASSERT_EGL();

    /* Destroy the source texture and image */
    eglDestroyImageKHR(util::ctx.dpy, image1);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &sourceTexture);

    /* Bind the shared image to an image */
    image2 = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT,
                               EGL_SHARED_IMAGE_NOK,
                               (EGLClientBuffer)(intptr_t)sharedImage,
                               imageAttributes);
    ASSERT_EGL();

    /* Bind the image to a texture */
    glGenTextures(1, &targetTexture2);
    glBindTexture(GL_TEXTURE_2D, targetTexture2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image2);
    ASSERT_GL();

    /* Draw the texture */
    offset += spacing;
    test::drawQuad(offset, 0, width * scale, height * scale);
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height / 2) * scale, white)); // center
    ASSERT(test::checkColor(offset + (4)         * scale, (4)          * scale, white)); // lower left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (4)          * scale, color)); // lower middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (4)          * scale, white)); // lower right
    ASSERT(test::checkColor(offset + (4)         * scale, (height - 4) * scale, color)); // upper left
    ASSERT(test::checkColor(offset + (width / 2) * scale, (height - 4) * scale, color)); // upper middle
    ASSERT(test::checkColor(offset + (width - 4) * scale, (height - 4) * scale, color)); // upper right

    /* Clean up */
    eglDestroyImageKHR(util::ctx.dpy, image2);
    glDeleteTextures(1, &targetTexture2);
}

/**
 *  Measure the time needed to bind a shared image into a texture.
 *
 *  Initialization:
 *
 *  1. Create a texture of size @width x @height.
 *  2. Create an EGLImage from the texture.
 *
 *  Test loop:
 *
 *  1. Create a shared EGLImage from the EGLImage.
 *  2. Create an EGLImage from the shared image.
 *  3. Bind the EGLImage into a texture.
 *  4. Render the texture onto the screen and read back a single pixel to force
 *     rendering to complete.
 *
 *  The test results contain the average timings of the listed test steps.
 */
void testMappingLatency(int width, int height)
{
    EGLImageKHR image1, image2;
    GLuint sourceTexture, targetTexture;
    int cycles = 64;
    int64_t start, totalSharedImageCreation = 0, totalImageCreation = 0,
            totalImageBinding = 0, totalRendering = 0;
    uint8_t color[4];

    const EGLint imageAttributes[] =
    {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };

    const EGLint sharedImageAttributes[] =
    {
        EGL_NONE
    };

    /* Create the texture */
    glGenTextures(1, &sourceTexture);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL();

    /* Create an EGL image from the texture */
    image1 = eglCreateImageKHR(util::ctx.dpy, util::ctx.context, EGL_GL_TEXTURE_2D_KHR,
                               (EGLClientBuffer)sourceTexture, imageAttributes);
    ASSERT_EGL();

    /* Measure creation and binding times */
    glGenTextures(1, &targetTexture);
    glBindTexture(GL_TEXTURE_2D, targetTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    for (int i = 0; i < cycles; i++)
    {
        EGLNativeSharedImageTypeNOK sharedImage;(
                boost::bind(eglDestroySharedImageNOK, util::ctx.dpy, _1));

        /* Create the shared image */
        start = util::getTime();
        sharedImage = eglCreateSharedImageNOK(util::ctx.dpy, image1, sharedImageAttributes);
        totalSharedImageCreation += util::getTime() - start;

        /* Bind it to an image */
        start = util::getTime();
        image2 = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT,
                                   EGL_SHARED_IMAGE_NOK,
                                   (EGLClientBuffer)(intptr_t)sharedImage,
                                   imageAttributes);
        totalImageCreation += util::getTime() - start;

        /* Bind the image to a texture */
        start = util::getTime();
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image2);
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
        eglDestroyImageKHR(util::ctx.dpy, image2);
        eglDestroySharedImageNOK(util::ctx.dpy, sharedImage);
        sharedImage = 0;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, color);
        test::swapBuffers();
        ASSERT_GL();
        ASSERT_EGL();
    }

    printf("%lld us / %lld us / %lld us / %lld us : ",
            totalSharedImageCreation / cycles / 1000UL,
            totalImageCreation / cycles / 1000UL,
            totalImageBinding / cycles / 1000UL,
            totalRendering / cycles / 1000UL);

    /* Clean up */
    eglDestroyImageKHR(util::ctx.dpy, image1);
    glDeleteTextures(1, &targetTexture);
    glDeleteTextures(1, &sourceTexture);
}

uint32_t colorAt(int width, int height, int x, int y)
{
    uint8_t r = (width % 2) * (x ^ y);
    uint8_t g = ((width + height + 1) % 2) * (x ^ y);
    uint8_t b = (height % 2) * (x ^ y);
    uint8_t a = 0xff;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

/**
 *  Stress test to ensure many shared images can be created rapidly.
 *
 *  1. Initialize a texture of size @width x @height using @format and @type
 *     with the test pattern.
 *  2. Create an EGLImage from the texture.
 *  3. Create a shared EGLImage from the EGLImage.
 *  4. Create a second EGLImage from the shared EGLImage.
 *  5. Bind the second EGLImage into a texture.
 *  6. Blit the texture onto the screen.
 *  7. Verify test pattern.
 */
bool testDynamicTextures(GLint format, GLint type, int width, int height)
{
    EGLImageKHR image1, image2;
    GLuint sourceTexture, targetTexture;
    test::scoped<EGLNativeSharedImageTypeNOK> sharedImage(
            boost::bind(eglDestroySharedImageNOK, util::ctx.dpy, _1));
    bool result = true;

    const EGLint imageAttributes[] =
    {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };

    const EGLint sharedImageAttributes[] =
    {
        EGL_NONE
    };

    /* Generate the texture */
    glGenTextures(1, &sourceTexture);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    boost::scoped_array<uint32_t> texData(new uint32_t[width * height]);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            texData[y * width + x] = colorAt(width, height, x, y);
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, type, &texData[0]);
    memset(&texData[0], 0, width * height * 4);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    ASSERT_GL();

    /* Create an EGL image from the texture */
    image1 = eglCreateImageKHR(util::ctx.dpy, util::ctx.context, EGL_GL_TEXTURE_2D_KHR,
                               (EGLClientBuffer)sourceTexture, imageAttributes);
    ASSERT_EGL();

    /* Clone the image into a shared image */
    sharedImage = eglCreateSharedImageNOK(util::ctx.dpy, image1, sharedImageAttributes);
    ASSERT_EGL();

    /* Destroy the source texture and image */
    eglDestroyImageKHR(util::ctx.dpy, image1);
    glDeleteTextures(1, &sourceTexture);

    /* Bind the shared image to an image */
    image2 = eglCreateImageKHR(util::ctx.dpy, EGL_NO_CONTEXT,
                               EGL_SHARED_IMAGE_NOK,
                               (EGLClientBuffer)(intptr_t)sharedImage,
                               imageAttributes);
    ASSERT_EGL();

    /* Verify the properties of the image */
    EGLint imageWidth = 0, imageHeight = 0;
    ASSERT(eglQueryImageNOK(util::ctx.dpy, image2, EGL_WIDTH, &imageWidth));
    ASSERT(eglQueryImageNOK(util::ctx.dpy, image2, EGL_HEIGHT, &imageHeight));
    ASSERT_EGL();
    ASSERT(imageWidth == width);
    ASSERT(imageHeight == height);

    /* Bind the image to a texture */
    glGenTextures(1, &targetTexture);
    glBindTexture(GL_TEXTURE_2D, targetTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image2);
    ASSERT_GL();

    /* Draw the texture */
    test::drawQuad(0, 0, width, height);

    /* Verify results */
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &texData[0]);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint32_t p1 = colorAt(width, height, x, height - y - 1);
            uint32_t p2 = texData[y * width + x];
            uint8_t r1 = (p1 & 0x000000ff);
            uint8_t g1 = (p1 & 0x0000ff00) >> 8;
            uint8_t b1 = (p1 & 0x00ff0000) >> 16;
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
    glDeleteTextures(1, &targetTexture);
    eglDestroyImageKHR(util::ctx.dpy, image1);
    eglDestroyImageKHR(util::ctx.dpy, image2);

    return result;
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
        GLint format;
        GLint type;
        const char* fileName;
        uint8_t color[4];
        int width;
        int height;
    } entries[] =
    {
        {GL_RGBA,           GL_UNSIGNED_BYTE,           "blue_64x64_rgba8888.raw", {0x00, 0x00, 0xff, 0xff}, 64,  64},
        {GL_RGBA,           GL_UNSIGNED_SHORT_4_4_4_4,  "blue_64x64_rgba4444.raw", {0x00, 0x00, 0xff, 0xff}, 64,  64},
        {GL_RGBA,           GL_UNSIGNED_SHORT_5_5_5_1,  "blue_64x64_rgba1555.raw", {0x00, 0x00, 0xff, 0xff}, 64,  64},
        {GL_RGB,            GL_UNSIGNED_SHORT_5_6_5,    "blue_64x64_rgb565.raw",   {0x00, 0x00, 0xff, 0xff}, 64,  64},
        {GL_LUMINANCE,      GL_UNSIGNED_BYTE,           "blue_64x64_r8.raw",       {0x00, 0x00, 0x00, 0xff}, 64,  64},
        {GL_ALPHA,          GL_UNSIGNED_BYTE,           "blue_64x64_r8.raw",       {0xff, 0x00, 0xff, 0xff}, 64,  64},
        {GL_ETC1_RGB8_OES,  0,                          "green_64x64_etc1.raw",    {0x00, 0xff, 0x00, 0xff}, 64,  64},
        {GL_RGBA,           GL_UNSIGNED_BYTE,           "red_113x47_rgba8888.raw", {0xff, 0x00, 0x00, 0xff}, 113, 47},
        {GL_RGBA,           GL_UNSIGNED_SHORT_4_4_4_4,  "red_113x47_rgba4444.raw", {0xff, 0x00, 0x00, 0xff}, 113, 47},
        {GL_RGBA,           GL_UNSIGNED_SHORT_5_5_5_1,  "red_113x47_rgba5551.raw", {0xff, 0x00, 0x00, 0xff}, 113, 47},
        {GL_RGB,            GL_UNSIGNED_SHORT_5_6_5,    "red_113x47_rgb565.raw",   {0xff, 0x00, 0x00, 0xff}, 113, 47},
        {GL_LUMINANCE,      GL_UNSIGNED_BYTE,           "red_113x47_r8.raw",       {0x00, 0x00, 0x00, 0xff}, 113, 47},
        {GL_ALPHA,          GL_UNSIGNED_BYTE,           "red_113x47_r8.raw",       {0xff, 0x00, 0xff, 0xff}, 113, 47},
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

    for (unsigned int i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
    {
        test::printHeader("Testing texture format %s (%dx%d)",
                util::textureFormatName(entries[i].format, entries[i].type).c_str(),
                entries[i].width, entries[i].height);
        result &= test::verifyResult(
                boost::bind(testTextures, entries[i].format, entries[i].type, entries[i].width,
                            entries[i].height, entries[i].fileName, entries[i].color));
        test::swapBuffers();

        if (entries[i].format == GL_ETC1_RGB8_OES ||
            entries[i].format == GL_ALPHA ||
            entries[i].format == GL_LUMINANCE)
        {
            continue;
        }

        test::printHeader("Testing framebuffer format %s (%dx%d)",
                util::textureFormatName(entries[i].format, entries[i].type).c_str(),
                entries[i].width, entries[i].height);
        result &= test::verifyResult(
                boost::bind(testFramebuffers, entries[i].format, entries[i].type, entries[i].width,
                            entries[i].height, entries[i].color));
        test::swapBuffers();
    }

    test::printHeader("Running stress test");
    for (unsigned int i = 1; i < 512; i++)
    {
        int width = i;
        int height = i / 2 + 1;

        glClear(GL_COLOR_BUFFER_BIT);
        result &= test::verify(
                boost::bind(testDynamicTextures, GL_RGBA, GL_UNSIGNED_BYTE, width, height));

        test::swapBuffers();

        if (!result)
        {
            break;
        }
    }
    test::printResult(result);

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
