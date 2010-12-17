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
 */
#include "testutil.h"
#include "util.h"

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/types.h>

namespace test
{

const char *vertSource =
    "precision mediump float;\n"
    "attribute vec2 in_position;\n"
    "attribute vec2 in_texcoord;\n"
    "varying vec2 texcoord;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(in_position, 0.0, 1.0);\n"
    "   texcoord = in_texcoord;\n"
    "}\n";

const char *fragSource =
    "precision mediump float;\n"
    "varying vec2 texcoord;\n"
    "uniform sampler2D texture;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_FragColor = texture2D(texture, texcoord);\n"
    "   /* Make fully transparent fragments purple so we can check for them on RGB framebuffers */\n"
    "   if (length(gl_FragColor - vec4(0.0, 0.0, 0.0, 0.0)) < 0.1)\n"
    "           gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);\n"
    "}\n";

void drawQuad(int x, int y, int w, int h)
{
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLint program;

    const GLfloat viewW = 0.5f * viewport[2];
    const GLfloat viewH = 0.5f * viewport[3];
    const GLfloat texW = 1.0f, texH = 1.0f;
    const GLfloat quadX1 = x       / viewW - 1.0f, quadY1 = y       / viewH - 1.0f;
    const GLfloat quadX2 = (x + w) / viewW - 1.0f, quadY2 = (y + h) / viewH - 1.0f;
    const GLfloat texcoords[] =
    {
         0,       texH,
         0,       0,
         texW,    texH,
         texW,    0
    };

    const GLfloat vertices[] =
    {
        quadX1, quadY1,
        quadX1, quadY2,
        quadX2, quadY1,
        quadX2, quadY2,
    };

    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    GLint positionAttr = glGetAttribLocation(program, "in_position");
    GLint texcoordAttr = glGetAttribLocation(program, "in_texcoord");
    ASSERT_GL();

    glVertexAttribPointer(positionAttr, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(texcoordAttr, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
    glEnableVertexAttribArray(positionAttr);
    glEnableVertexAttribArray(texcoordAttr);
    ASSERT_GL();

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    ASSERT_GL();
}

namespace color {

const char *vertSource =
    "precision mediump float;\n"
    "attribute vec2 in_position;\n"
    "attribute vec4 in_color;\n"
    "varying vec4 color;\n"
    "\n"
    "void main()\n"
    "{\n"
    "	gl_Position = vec4(in_position, 0.0, 1.0);\n"
    "	color = in_color;\n"
    "}\n";

const char *fragSource =
    "precision mediump float;\n"
    "varying vec4 color;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_FragColor = color;\n"
    "}\n";

void drawQuad(int x, int y, int w, int h, GLfloat r, GLfloat g, GLfloat b)
{
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLint program;

    const GLfloat viewW = 0.5f * viewport[2];
    const GLfloat viewH = 0.5f * viewport[3];
    const GLfloat quadX1 = x       / viewW - 1.0f, quadY1 = y       / viewH - 1.0f;
    const GLfloat quadX2 = (x + w) / viewW - 1.0f, quadY2 = (y + h) / viewH - 1.0f;
    const GLfloat color[] =
    {
        r, g, b, 1.0f,
        r, g, b, 1.0f,
        r, g, b, 1.0f,
        r, g, b, 1.0f,
    };

    const GLfloat vertices[] =
    {
        quadX1, quadY1,
        quadX1, quadY2,
        quadX2, quadY1,
        quadX2, quadY2,
    };

    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    GLint positionAttr = glGetAttribLocation(program, "in_position");
    GLint colorAttr = glGetAttribLocation(program, "in_color");
    ASSERT_GL();

    glVertexAttribPointer(positionAttr, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(colorAttr, 4, GL_FLOAT, GL_FALSE, 0, color);
    glEnableVertexAttribArray(positionAttr);
    glEnableVertexAttribArray(colorAttr);
    ASSERT_GL();

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    ASSERT_GL();
}

}

bool checkColor(int x, int y, const uint8_t* expected)
{
    uint8_t color[4];
    int epsilon = 4;
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);

    if (abs(color[0] - expected[0]) > epsilon ||
        abs(color[1] - expected[1]) > epsilon ||
        abs(color[2] - expected[2]) > epsilon ||
        abs(color[3] - expected[3]) > epsilon)
    {
       printf("Color mismatch at (%d, %d): expected %02x%02x%02x%02x, got %02x%02x%02x%02x\n",
               x, y,
               expected[0], expected[1], expected[2], expected[3],
               color[0], color[1], color[2], color[3]);
       return false;
    }
    return true;
}

void swapBuffers()
{
    eglSwapBuffers(util::ctx.dpy, util::ctx.surface);
}

bool printResult(bool result)
{
    bool tty = isatty(STDOUT_FILENO);
    if (!result)
    {
        printf(tty ? "\033[31;1mFAIL\033[0m\n" : "FAIL\n");
    }
    else
    {
        printf(tty ? "\033[32;1mOK\033[0m\n" : "OK\n");
    }
    return result;
}

bool printResult(const std::runtime_error& error)
{
    printResult(false);
    printf("%s\n", error.what());
    return false;
}

void printHeader(const char* header, ...)
{
    char msg[1024];
    va_list ap;

    va_start(ap, header);
    vsnprintf(msg, sizeof(msg), header, ap);
    va_end(ap);

    printf("%-47s: ", msg);
    fflush(stdout);
}

bool compareRGB565(uint16_t p1, uint16_t p2)
{
    int r1 = (p1 & 0xf800) >> 11;
    int g1 = (p1 & 0x07e0) >> 5;
    int b1 = (p1 & 0x001f);
    int r2 = (p2 & 0xf800) >> 11;
    int g2 = (p2 & 0x07e0) >> 5;
    int b2 = (p2 & 0x001f);

    // Allow some leeway in the colors due to dithering
    if (abs(r1 - r2) > 4 ||
        abs(g1 - g2) > 8 ||
        abs(b1 - b2) > 4)
    {
        return false;
    }
    return true;
}

bool compareRGBA8888(uint32_t p1, uint32_t p2)
{
    uint8_t b1 = (p1 & 0x000000ff);
    uint8_t g1 = (p1 & 0x0000ff00) >> 8;
    uint8_t r1 = (p1 & 0x00ff0000) >> 16;
    uint8_t a1 = (p1 & 0xff000000) >> 24;
    uint8_t r2 = (p2 & 0x000000ff);
    uint8_t g2 = (p2 & 0x0000ff00) >> 8;
    uint8_t b2 = (p2 & 0x00ff0000) >> 16;
    uint8_t a2 = (p2 & 0xff000000) >> 24;
    int t = 4;

    if (abs(r1 - r2) > t ||
        abs(g1 - g2) > t ||
        abs(b1 - b2) > t ||
        abs(a1 - a2) > t)
    {
        return false;
    }
    return true;
}

void fail(const char* format, ...)
{
    char msg[1024];
    va_list ap;

    va_start(ap, format);
    vsnprintf(msg, sizeof(msg), format, ap);
    va_end(ap);

    throw std::runtime_error(msg);
}

void assert(bool condition, const char* format, ...)
{
    if (condition)
    {
        return;
    }

    char msg[1024];
    va_list ap;

    va_start(ap, format);
    vsnprintf(msg, sizeof(msg), format, ap);
    va_end(ap);

    throw std::runtime_error(msg);
}

} // namespace test
