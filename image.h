/*
The MIT License (MIT)

Copyright (c) 2016-2026 Eric Fry

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef PNG2TILE_IMAGE_H
#define PNG2TILE_IMAGE_H

#include <vector>

#define MAX_COLOURS 16

class Color {
public:
    unsigned char red = 0;
    unsigned char green = 0;
    unsigned char blue = 0;

    Color(const unsigned char red, const unsigned char green, const unsigned char blue)
        : red(red),
          green(green),
          blue(blue) {}

    friend bool operator<(const Color& lhs, const Color& rhs) {
        if (lhs.red < rhs.red)
            return true;
        if (rhs.red < lhs.red)
            return false;
        if (lhs.green < rhs.green)
            return true;
        if (rhs.green < lhs.green)
            return false;
        return lhs.blue < rhs.blue;
    }
};

typedef struct {
    unsigned int width, height;
    std::vector<Color> palette;
    std::vector<unsigned char> pixels;
} Image;

#endif //PNG2TILE_IMAGE_H
