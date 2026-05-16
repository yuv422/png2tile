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

#include "tile.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <ostream>
#include <set>

Tile::Tile(uint16_t id, Image* image, int x, int y) {
    this->id = id;
    this->tilemapX = x;
    this->tilemapY = y;
    this->flipped_x = false;
    this->flipped_y = false;
    this->is_duplicate = false;
    this->palette_index = 0;
    this->original_tile = nullptr;

    unsigned char *ptr = &image->pixels[0] + y * image->width + x;
    unsigned char *tile_ptr = data;
    for (int i = 0; i < TILE_HEIGHT; i++) {
        memcpy(tile_ptr, ptr, TILE_WIDTH);
        tile_ptr += TILE_WIDTH;
        ptr += image->width;
    }
    setPalIndex();
}

Tile::Tile(uint16_t id, bool flippedX, bool flippedY, bool isDuplicate, int palIdx, Tile *originalTile) : id(id),
                                                                                                          flipped_x(flippedX),
                                                                                                          flipped_y(flippedY),
                                                                                                          is_duplicate(isDuplicate),
                                                                                                          palette_index(palIdx),
                                                                                                          original_tile(originalTile) {

}

Tile *Tile::flipX() {
    Tile *flipped_tile = new Tile(id, true, flipped_y, is_duplicate, palette_index, original_tile);

    for (int y = 0; y < TILE_HEIGHT; y++) {
        for (int x = 0; x < TILE_WIDTH; x++) {
            flipped_tile->data[y * TILE_WIDTH + x] = data[y * TILE_WIDTH + ((TILE_WIDTH - 1) - x)];
        }
    }

    return flipped_tile;
}

Tile *Tile::flipY() {
    Tile *flipped_tile = new Tile(id, flipped_x, true, is_duplicate, palette_index, original_tile);

    for (int x = 0; x < TILE_WIDTH; x++) {
        for (int y = 0; y < TILE_HEIGHT; y++) {
            flipped_tile->data[y * TILE_WIDTH + x] = data[(TILE_HEIGHT - 1 - y) * TILE_WIDTH + x];
        }
    }

    return flipped_tile;
}

Tile *Tile::flipXY() {
    Tile *flipped_x_tile = flipX();
    Tile *flipped_xy_tile = flipped_x_tile->flipY();
    delete flipped_x_tile;

    return flipped_xy_tile;
}

bool Tile::isDataEqual(Tile *anotherTile) {
    int count = 0;
    for (; count < NUM_PIXELS_IN_TILE; count++) {
        if (data[count] != anotherTile->data[count]) {
            break;
        }
    }
    if (count == NUM_PIXELS_IN_TILE) {
        return true;
    }
    return false;
}

bool Tile::validateColorUsage() const {
    std::set<int> colorsUsed;
    for (int i = 0; i < NUM_PIXELS_IN_TILE; i++) {
        colorsUsed.insert(data[i]);
    }
    return colorsUsed.size() <= 16;
}

void Tile::setPalette(const std::vector<std::set<int>>& palette) {
    std::set<int> colorsUsed;
    for (int i = 0; i < NUM_PIXELS_IN_TILE; i++) {
        colorsUsed.insert(data[i]);
    }
    for (int i = 0; i < palette.size(); i++) {
        auto pal = palette[i];
        if (std::includes(pal.begin(), pal.end(), colorsUsed.begin(), colorsUsed.end())) {
            palette_index = i;
            std::map<int, int> colorMap;
            int j = 0;
            for (int color: pal) {
                colorMap[color] = j;
                j++;
            }
            for (int j = 0; j < NUM_PIXELS_IN_TILE; j++) {
                data[j] = colorMap[data[j]];
            }
            return;
        }
    }
    std::cerr << "Error: Could not find a palette for tile at (" << tilemapX << ", " << tilemapY << ")" << std::endl;
}

void Tile::setPalIndex() {
    int minPalIdx = data[0];
    int maxPalIdx = data[0];
    for (int i = 1; i < NUM_PIXELS_IN_TILE; i++) {
        if (data[i] < minPalIdx) {
            minPalIdx = data[i];
        } else if (data[i] > maxPalIdx) {
            maxPalIdx = data[i];
        }
    }
    palette_index = minPalIdx % 16;
}
