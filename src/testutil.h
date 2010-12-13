/**
 * Test suite utility functions
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
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <GLES2/gl2.h>

#include <stdexcept>

#define ASSERT(X) \
    do \
    { \
        if (!(X)) \
        { \
            test::fail("Assertion failed at %s:%d: %s\n", __FILE__, __LINE__, #X); \
        } \
    } while (0)

namespace test
{

extern const char *vertSource;
extern const char *fragSource;

namespace color {
    extern const char *vertSource;
    extern const char *fragSource;
    void drawQuad(int x, int y, int w, int h, GLfloat r, GLfloat g, GLfloat b);
}

void fail(const char* format, ...);
bool printResult(bool result);
bool printResult(const std::runtime_error& error);
void printHeader(const char* header, ...);
void swapBuffers();
void drawQuad(int x, int y, int w, int h);
bool checkColor(int x, int y, const uint8_t* expected);
bool compareRGB565(uint16_t p1, uint16_t p2);
bool compareRGBA8888(uint32_t p1, uint32_t p2);

/* Test pattern: four vertical stripes (white, red, green, blue) with lower
 * half with half-intensity */
template <typename TYPE>
void drawTestPattern(TYPE* pixels, int width, int height, int pitch,
        int redSize, int greenSize, int blueSize, int alphaSize,
        int redShift, int greenShift, int blueShift, int alphaShift,
        bool originAtTop)
{
    assert(redSize <= 8);
    assert(greenSize <= 8);
    assert(blueSize <= 8);
    assert(alphaSize <= 8);

    pitch /= sizeof(TYPE);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int r = 0, g = 0, b = 0, a = 0xff;
            switch (4 * x / width)
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
            TYPE p = 0;

            if (originAtTop && y > height / 2)
            {
                r >>= 1;
                g >>= 1;
                b >>= 1;
            }
            else if (!originAtTop && y < height / 2)
            {
                r >>= 1;
                g >>= 1;
                b >>= 1;
            }

            p |= (r >> (8 - redSize))   << redShift;
            p |= (g >> (8 - greenSize)) << greenShift;
            p |= (b >> (8 - blueSize))  << blueShift;
            p |= (a >> (8 - alphaSize)) << alphaShift;
            pixels[y * pitch + x] = p;
        }
    }
}

/**
 *  A helper template class for releasing resources when they leave the current
 *  scope. Similar to std::auto_ptr and boost::scoped_ptr, but allows a custom
 *  deletion function and makes it easier to convert to and from the contained
 *  type.
 */
template <typename TYPE>
class scoped
{
public:
    /** Deletion callback function */
    typedef typename boost::function<void(const TYPE&)> deleter;

    /**
     *  \param v  Contained value
     *  \param f  Deleter function
     */
    explicit scoped(const TYPE& v, deleter f = 0):
        m_value(v),
        m_deleter(f)
    {
    }

    /**
     *  \param f  Deleter function
     */
    scoped(deleter f = 0):
        m_value(0),
        m_deleter(f)
    {
    }

    scoped():
        m_value(0)
    {
    }

    virtual ~scoped()
    {
        if (this->m_value && m_deleter)
        {
            m_deleter(this->m_value);
        }
    }

    scoped<TYPE>& operator=(const TYPE& v)
    {
        m_value = v;
        return *this;
    }

    operator TYPE()
    {
        return m_value;
    }

    TYPE* operator&()
    {
        return &m_value;
    }

protected:
    TYPE m_value;
private:
    deleter m_deleter;
};

/**
 *  Call a function and catch any std::runtime_error exceptions it raises.
 *  When an exception is caught, it is printed to the screen.
 *
 *  \returns true when no exceptions were raised, false otherwise.
 */
template <typename FUNC>
bool verify(FUNC func)
{
    try
    {
        func();
        return true;
    }
    catch (std::runtime_error e)
    {
        printf("%s\n", e.what());
        return false;
    }
}

/**
 *  Call a function and catch any std::runtime_error exceptions it raises. The
 *  result (OK/FAIL) is printed to the screen.
 *
 *  \returns true when no exceptions were raised, false otherwise.
 */
template <typename FUNC>
bool verifyResult(FUNC func)
{
    try
    {
        func();
        test::printResult(true);
        return true;
    }
    catch (std::runtime_error e)
    {
        test::printResult(e);
        return false;
    }
}

}

#endif // TEST_UTIL_H
