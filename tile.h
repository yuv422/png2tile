/*
The MIT License (MIT)

Copyright (c) 2016-2021 Eric Fry

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
#ifndef PNG2TILE_TILE_H
#define PNG2TILE_TILE_H

#include <cstdint>
#define NUM_PIXELS_IN_TILE 64

#define TILE_HEIGHT 8
#define TILE_WIDTH 8

class Tile {
public:
    uint16_t id;
    unsigned char data[NUM_PIXELS_IN_TILE];
    bool flipped_x;
    bool flipped_y;
    bool is_duplicate;
    Tile *original_tile;

    Tile(uint16_t id, bool flippedX, bool flippedY, bool isDuplicate, Tile *originalTile);

    Tile *flipX();
    Tile *flipY();
    Tile *flipXY();

    bool isDataEqual(Tile *anotherTile);
};


#endif //PNG2TILE_TILE_H
