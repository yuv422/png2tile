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
#include <cstring>
#include <string>
#include <iostream>

#include <vector>
#include <fstream>

#include "tile.h"
#include "lodepng.h"

#define NUM_TILE_COLS_IN_PNG_IMAGE 16

#define TMX_FLIP_X_FLAG 0x80000000
#define TMX_FLIP_Y_FLAG 0x40000000

#define MAX_COLOURS 16

#define TILEMAP_H_FLIP_FLAG 0x0200
#define TILEMAP_V_FLIP_FLAG 0x0400
#define TILEMAP_SPRITE_PALETTE_FLAG 0x0800
#define TILEMAP_INFRONT_FLAG 0x1000

typedef struct {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} Color;

typedef struct {
    unsigned int width, height;
    Color palette[MAX_COLOURS];
    std::vector<unsigned char> pixels;
} Image;

typedef enum {
    GEN,
    SMS,
    SMS_CL123,
    GG
} PaletteOutputFormat;

typedef enum {
    TILE_8x8,
    TILE_8x16
} TileSize;

typedef enum {
    TILE_FORMAT_PLANAR,
    TILE_FORMAT_CHUNKY
} TileOutputFormat;

typedef struct {
    const char *input_filename;
    const char *output_tile_image_filename;
    const char *tmx_filename;
    const char *palette_filename;
    const char *tilemap_filename;
    const char *tiles_filename;
    bool mirror;
    bool remove_dups;
    PaletteOutputFormat paletteOutputFormat;
    TileSize tileSize;
    TileOutputFormat tileOutputFormat;
    int tile_start_offset;
    bool use_sprite_pal;
    bool infront_flag;
    bool output_bin;
    bool compress;
    bool quiet;
} Config;

// forwards for compressors
int PSGaiden_compressTiles(const uint8_t* pSource, const uint32_t numTiles, uint8_t* pDestination, const uint32_t destinationLength);
extern "C" {
int STM_compressTilemap(uint8_t* source, uint32_t width, uint32_t height, uint8_t* dest, uint32_t destLen);
}

Image *read_png_file(const char *filename, bool quiet) {
    std::vector<unsigned char> png;
    Image *image = new Image;
    lodepng::State state;

    unsigned error = lodepng::load_file(png, filename);
    if (!error) error = lodepng_inspect(&image->width, &image->height, &state, &png[0], png.size());
    if (error) {
        std::cout << "[read_png_file] error reading file " << error << ": "<< lodepng_error_text(error) << std::endl;
        delete image;
        return nullptr;
    }

    if (state.info_png.color.colortype != LCT_PALETTE) {
        printf("[read_png_file] Only indexed PNG files allowed\n");
        delete image;
        return nullptr;
    }

    if (!quiet && state.info_png.color.bitdepth > 4) {
        printf("[read_png_file] PNG bit depth > 4. Only the first 16 colours will be used.\n");
    }

    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = 8;


    if(!error) error = lodepng::decode(image->pixels, image->width, image->height, state, png);
    if (error) {
        std::cout << "[read_png_file] decoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
        delete image;
        return nullptr;
    }

    // Remove all colors after the first 16 palette entries.
    for (unsigned int y = 0; y < image->height; y++) {
        for (unsigned int x = 0; x < image->width; x++) {
            if (image->pixels[x + y * image->width] >= MAX_COLOURS) {
                printf("Warning: color palette index[%d] used at pixel(%d,%d).\n", image->pixels[x + y * image->width], x, y);
                image->pixels[x + y * image->width] = 0;
            }
        }
    }

    for(int i = 0; i < MAX_COLOURS; i++) {
        if (i < state.info_png.color.palettesize) {
            image->palette[i].red = state.info_png.color.palette[i * 4];
            image->palette[i].green = state.info_png.color.palette[i * 4 + 1];
            image->palette[i].blue = state.info_png.color.palette[i * 4 + 2];
        } else {
            image->palette[i].red   = 0;
            image->palette[i].green = 0;
            image->palette[i].blue  = 0;
        }
    }
    return image;
}

void write_png_file(const char *filename, int width, int height, const unsigned char *pixels, Color *palette) {
    std::vector<unsigned char> png;
    lodepng::State state;
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = 8;

    for (int i = 0; i < MAX_COLOURS; i++) {
        lodepng_palette_add(
                &state.info_raw,
                palette[i].red,
                palette[i].green,
                palette[i].blue,
                0xFF
        );
    }
    unsigned error = lodepng::encode(png, pixels, (unsigned )width, (unsigned )height, state);
    if(!error) lodepng::save_file(png, filename);
    if (error) {
        std::cout << "[write_png_file] encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
    }
}

void show_usage() {
    std::string s = "Usage:\n"
            "png2tile <input_filename> [options]\n"
            "\n"
            "Option               Effect\n"
            "\n"
            "-[no]removedupes     Enable/disable the removal of duplicate tiles\n"
            "                     *default (-removedupes)\n"
            "\n"
            "-[no]mirror          Enable/disable tile mirroring to further optimise\n"
            "                     duplicates *default (-mirror)\n"
            "\n"
            "-tilesize <size>     '8x8'      Treat tile data as 8x8 *default*\n"
            "                     '8x16'     Treat tile data as 8x16\n"
            "\n"
            "-tileformat <format> 'planar'   Output tileset data in Planar format. *default* \n"
            "                     'chunky'   Output tileset data in chunky\n"
            "                                (two pixels per byte) format. \n"
            "\n"
            "-tileoffset <n>      The starting index of the first tile. *Default is 0.\n"
            "                     The offset can be specified in either decimal or hex\n"
            "                     Hex numbers prefixed with 0x eg. 0x1A\n"
            "\n"
            "-spritepalette       Set the tilemap bit to make tiles use the sprite palette.\n"
            "                     *Default is unset.\n"
            "\n"
            "-infrontofsprites    Set the tilemap bit to make tiles appear in front of\n"
            "                     sprites. *Default is unset.\n"
            "\n"
            "-pal <format>        Palette output format\n"
            "                     gen        Output the palette in GEN/MD colour format\n"
            "                     sms        Output the palette in SMS colour format\n"
            "                     gg         Output the palette in GG colour format\n"
            "                     sms_cl123  Output the palette in SMS colour format\n"
            "                                eg cl123, cl333, cl001\n"
            "\n"
            "-savetiles <filename>\n"
            "                     Save tile data to <filename>.\n"
            "\n"
            "-savetilemap <filename>\n"
            "                     Save tilemap data to <filename>. \n"
            "\n"
            "-savepalette <filename>\n"
            "                     Save palette data to <filename>.\n"
            "\n"
            "-savetileimage <filename>\n"
            "                     Save tileset data as a PNG image.\n"
            "\n"
            "-savetmx <filename> \n"
            "                     Save tilemap and corresponding tileset in the Tiled\n"
            "                     mapeditor TMX format.\n"
            "\n"
            "-binary \n"
            "                     Output binary files instead of asm source files.\n"
            "                     Ignored for sms_cl123 palette format, TMX, and PNG output.\n"
            "\n"
            "-compress \n"
            "                     Compress output binary files. Uses STM compression for tilemaps\n"
            "                     and PSG compression for tiles. Implies -binary if not also specified.\n"
            "\n"
            "-quiet               Reduce verbosity.\n\n";

    std::cout << s;
}

Config parse_commandline_opts(int argc, char **argv) {
    Config config;

    if (argc < 2 || argv[1][0] == '-') {
        show_usage();
        exit(1);
    }

    config.input_filename = argv[1];
    config.remove_dups = true;
    config.mirror = true;
    config.paletteOutputFormat = SMS;
    config.tileSize = TILE_8x8;
    config.tileOutputFormat = TILE_FORMAT_PLANAR;
    config.use_sprite_pal = false;
    config.infront_flag = false;
    config.tile_start_offset = 0;
    config.output_bin = false;
    config.compress = false;
    config.quiet = false;

    config.output_tile_image_filename = nullptr;
    config.tmx_filename = nullptr;
    config.palette_filename = nullptr;
    config.tilemap_filename = nullptr;
    config.tiles_filename = nullptr;

    for (int i = 2; i < argc; i++) {
        const char *option = argv[i];
        if (option[0] == '-') {
            const char *cmd = &option[1];
            if (strcmp(cmd, "removedupes") == 0) {
                config.remove_dups = true;
            } else if (strcmp(cmd, "noremovedupes") == 0) {
                config.remove_dups = false;
            } else if (strcmp(cmd, "mirror") == 0) {
                config.mirror = true;
            } else if (strcmp(cmd, "nomirror") == 0) {
                config.mirror = false;
            } else if (strcmp(cmd, "tilesize") == 0) {
                i++;
                if (i < argc) {
                    if (strcmp(argv[i], "8x8") == 0) {
                        config.tileSize = TILE_8x8;
                    } else if (strcmp(argv[i], "8x16") == 0) {
                        config.tileSize = TILE_8x16;
                    } else {
                        printf("Invalid tile size '%s'. Valid sizes are ('8x8', '8x16')\n", argv[i]);
                        exit(1);
                    }
                }
            } else if (strcmp(cmd, "tileformat") == 0) {
                i++;
                if (i < argc) {
                    if (strcmp(argv[i], "planar") == 0) {
                        config.tileOutputFormat = TILE_FORMAT_PLANAR;
                    } else if (strcmp(argv[i], "chunky") == 0) {
                        config.tileOutputFormat = TILE_FORMAT_CHUNKY;
                    } else {
                        printf("Invalid tile output format '%s'. Valid formats are ('planar', 'chunky')\n", argv[i]);
                        exit(1);
                    }
                }
            } else if (strcmp(cmd, "tileoffset") == 0) {
                i++;
                if (i < argc) {
                    config.tile_start_offset = strtol(argv[i], nullptr, 0);
                }
            } else if (strcmp(cmd, "spritepalette") == 0) {
                config.use_sprite_pal = true;
            } else if (strcmp(cmd, "infrontofsprites") == 0) {
                config.infront_flag = true;
            } else if (strcmp(cmd, "pal") == 0) {
                i++;
                if (i < argc) {
                    if (strcmp(argv[i], "gen") == 0) {
                        config.paletteOutputFormat = GEN;
                    } else if (strcmp(argv[i], "sms") == 0) {
                        config.paletteOutputFormat = SMS;
                    } else if (strcmp(argv[i], "sms_cl123") == 0) {
                        config.paletteOutputFormat = SMS_CL123;
                    } else if (strcmp(argv[i], "gg") == 0) {
                        config.paletteOutputFormat = GG;
                    } else {
                        printf("Invalid palette type '%s'. Valid palette types are ('gen', 'sms', 'sms_cl123', 'gg')\n",
                               argv[i]);
                        exit(1);
                    }
                }
            } else if (strcmp(cmd, "savetiles") == 0) {
                i++;
                if (i < argc) {
                    config.tiles_filename = argv[i];
                }
            } else if (strcmp(cmd, "savetilemap") == 0) {
                i++;
                if (i < argc) {
                    config.tilemap_filename = argv[i];
                }
            } else if (strcmp(cmd, "savepalette") == 0) {
                i++;
                if (i < argc) {
                    config.palette_filename = argv[i];
                }
            } else if (strcmp(cmd, "savetileimage") == 0) {
                i++;
                if (i < argc) {
                    config.output_tile_image_filename = argv[i];
                }
            } else if (strcmp(cmd, "savetmx") == 0) {
                i++;
                if (i < argc) {
                    config.tmx_filename = argv[i];
                }
            } else if (strcmp(cmd, "binary") == 0) {
                config.output_bin = true;
            } else if (strcmp(cmd, "compress") == 0) {
                config.compress = true;
            } else if (strcmp(cmd, "quiet") == 0) {
                config.quiet = true;
            } else {
                printf("Unknown option: '-%s'\n", cmd);
                show_usage();
                exit(1);
            }
        }
    }

    if (config.tileSize == TILE_8x16 && config.remove_dups) {
        printf("Warning: remove duplicates has been disabled because 8x16 tile size was selected.\n");
        config.remove_dups = false;
    }
    if (config.compress && !config.output_bin) {
        printf("Warning: output changed to binary because compression was enabled.\n");
        config.output_bin = true;
    }

    return config;
}

void write_tiles_to_png_image(const char *output_image_filename, Image *input_image, std::vector<Tile *> *tiles) {
    int output_width = 16;
    int output_height = (int)tiles->size() / output_width;
    if (tiles->size() % output_width != 0) {
        output_height++;
    }

    output_width *= TILE_WIDTH;
    output_height *= TILE_HEIGHT;

    unsigned char *pixels = (unsigned char *) malloc(output_width * output_height);
    memset(pixels, 0, output_width * output_height);
    int size = (int) tiles->size();

    for (int i = 0; i < size; i++) {
        unsigned char *ptr = &pixels[(i / NUM_TILE_COLS_IN_PNG_IMAGE) * output_width * TILE_HEIGHT +
                            (i % NUM_TILE_COLS_IN_PNG_IMAGE) * TILE_WIDTH];
        Tile *tile = tiles->at(i);
        unsigned char *tile_data_ptr = tile->data;
        for (int j = 0; j < TILE_WIDTH; j++) {
            memcpy(ptr, tile_data_ptr, TILE_WIDTH);
            tile_data_ptr += TILE_WIDTH;
            ptr += output_width;
        }
    }

    write_png_file(output_image_filename, output_width, output_height, pixels, input_image->palette);
}

void write_tiles(const Config &config, const char *filename, std::vector<Tile *> *tiles) {
    int size = (int) tiles->size();

    std::ofstream out;
    out.open(filename, config.output_bin ?
        std::ofstream::binary : std::ofstream::out);
    // the binary output buffer
    std::vector<uint8_t> outbuf;

    for (int i = 0; i < size; i++) {
        Tile *tile = tiles->at(i);
        char buf[32];
        if (!config.output_bin) {
            snprintf(buf, 32, "%03X", i + config.tile_start_offset);
            out << "; Tile index $" << buf << "\n";
            out << ".db";
        }

        if (config.tileOutputFormat == TILE_FORMAT_PLANAR) {
            for (int y = 0; y < TILE_HEIGHT; y++) {
                for (int p = 0; p < 4; p++) {
                    uint8_t byte = 0;
                    for (int x = 0; x < TILE_WIDTH; x++) {
                        uint8_t pixel = tile->data[y * TILE_WIDTH + x];
                        byte |= ((pixel >> p & 1) << (7 - x));
                    }
                    if (!config.output_bin) {
                        snprintf(buf, 32, "%02X", byte);
                        out << " $" << buf;
                    }
                    outbuf.push_back(byte);
                }
            }
        } else if (config.tileOutputFormat == TILE_FORMAT_CHUNKY) {
            for (int j = 0; j < NUM_PIXELS_IN_TILE; j += 2) {
                uint8_t outbyte = (uint8_t) (tile->data[j + 1] & 0xF) | ((uint8_t) (tile->data[j] & 0xF) << 4);
                if (!config.output_bin) {
                    snprintf(buf, 32, "%02X", outbyte);
                    out << " $" << buf;
                }
                outbuf.push_back(outbyte);
            }
        }
        if (!config.output_bin) out << "\n";
    }

    // write binary outbuf to file
    int orig_sz = (int)outbuf.size();

    // compress
    if (config.compress) {
        uint8_t* comp_dat = (uint8_t*)malloc(orig_sz);

        int comp_sz = PSGaiden_compressTiles(outbuf.data(), size, comp_dat, orig_sz);

        if (!config.quiet) {
            std::cout << "Compressed tile data from " << orig_sz << " bytes to " << comp_sz
                << " (" << (int)(comp_sz / (float)orig_sz * 100) << "%)." << std::endl;
        }

        out.write((const char*)comp_dat, comp_sz);
        free(comp_dat); comp_dat = nullptr;

    // uncompressed binary
    } else if (config.output_bin) out.write((const char*)outbuf.data(), orig_sz);

    out.close();
}

uint8_t convert_colour_channel_to_2bit(uint8_t c) {
    if (c < 56) return 0;
    if (c < 122) return 1;
    if (c < 188) return 2;
    return 3;
}

void write_sms_palette_file(const Config& config, const char *filename, Image *input_image) {
    std::ofstream out;
    out.open(filename, config.output_bin ?
        std::ofstream::binary : std::ofstream::out);

    if (!config.output_bin) out << ".db";

    for (int i = 0; i < MAX_COLOURS; i++) {
        uint8_t c = (convert_colour_channel_to_2bit((uint8_t) input_image->palette[i].red)
                   | (convert_colour_channel_to_2bit((uint8_t) input_image->palette[i].green) << 2)
                   | (convert_colour_channel_to_2bit((uint8_t) input_image->palette[i].blue) << 4));

        if (!config.output_bin) {
            char buf[3];
            snprintf(buf, 3, "%02X", c);
            out << " $" << buf;
        } else out.write((const char*)&c, 1);
    }
    if (!config.output_bin) out << "\n";

    out.close();
}

void write_gg_palette_file(const Config& config, const char *filename, Image *input_image) {
    std::ofstream out;
    out.open(filename, config.output_bin ?
        std::ofstream::binary : std::ofstream::out);

    if (!config.output_bin) out << ".dw";

    for (int i = 0; i < MAX_COLOURS; i++) {
        uint16_t c = ((uint16_t) input_image->palette[i].red >> 4)
                   | (uint16_t) (input_image->palette[i].green >> 4) << 4
                   | (uint16_t) (input_image->palette[i].blue >> 4) << 8;

        if (!config.output_bin) {
            char buf[5];
            snprintf(buf, 5, "%04X", c);
            out << " $" << buf;
        } else out.write((const char*)&c, 2);
    }
    if (!config.output_bin) out << "\n";

    out.close();
}

void write_gen_palette_file(const Config& config, const char *filename, Image *input_image) {
    std::ofstream out;
    out.open(filename, config.output_bin ?
        std::ofstream::binary : std::ofstream::out);

    if (!config.output_bin) out << ".dw";

    for (int i = 0; i < MAX_COLOURS; i++) {
        uint16_t c = (uint16_t)(((input_image->palette[i].red >> 4) & 0xE) << 0)
            | (uint16_t)(((input_image->palette[i].green >> 4) & 0xE) << 4)
            | (uint16_t)(((input_image->palette[i].blue >> 4) & 0xE) << 8);

        if (!config.output_bin) {
            char buf[5];
            snprintf(buf, 5, "%04X", c);
            out << " $" << buf;
        } else out.write((const char*)&c, 2);
    }
    if (!config.output_bin) out << "\n";

    out.close();
}

void write_sms_cl123_palette_file(const char *filename, Image *input_image) {
    std::ofstream out;
    out.open(filename);

    out << ".db";

    for (int i = 0; i < MAX_COLOURS; i++) {
        uint8_t r = convert_colour_channel_to_2bit((uint8_t) input_image->palette[i].red);
        uint8_t g = convert_colour_channel_to_2bit((uint8_t) input_image->palette[i].green);
        uint8_t b = convert_colour_channel_to_2bit((uint8_t) input_image->palette[i].blue);

        out << " cl" << (int) r << (int) g << (int) b;
    }
    out << "\n";

    out.close();
}

unsigned int get_tmx_tile_id(std::vector<Tile *> *tilemap, int index) {
    Tile *t = tilemap->at(index);

    unsigned int id = t->original_tile != nullptr ? (unsigned int) t->original_tile->id : (unsigned int) t->id;
    id++;
    if (t->flipped_x) {
        id = id | TMX_FLIP_X_FLAG;
    }
    if (t->flipped_y) {
        id = id | TMX_FLIP_Y_FLAG;
    }

    return id;
}

void write_tmx_file(const char *filename, Image *input_image, std::vector<Tile *> *tiles, std::vector<Tile *> *tilemap,
                    TileSize tileSize) {
    std::string tileset_filename = filename;

    tileset_filename += ".png";

    write_tiles_to_png_image(tileset_filename.c_str(), input_image, tiles);

    int tilemap_width = input_image->width / TILE_WIDTH;
    int tilemap_height = input_image->height / TILE_HEIGHT;

    std::ofstream out;
    out.open(filename);

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<map version=\"1.0\" orientation=\"orthogonal\" renderorder=\"right-down\" width=\"";
    out << tilemap_width << "\" height=\"" << tilemap_height;
    out << "\" tilewidth=\"" << TILE_WIDTH << "\" tileheight=\"" << TILE_HEIGHT << "\">\n";
    out << " <tileset firstgid=\"1\" name=\"tileset\" tilewidth=\"" << TILE_WIDTH << "\" tileheight=\"" << TILE_WIDTH <<
    "\">\n";
    out << "  <image source=\"" << tileset_filename << "\" />\n";
    out << " </tileset>\n";


    out << " <layer name=\"Bottom\" width=\"" << tilemap_width << "\" height=\"" << tilemap_height << "\">\n";
    out << "  <data encoding=\"csv\" >";

    int total_tiles = (int)tilemap->size();

    if (tileSize == TILE_8x8) {
        for (int i = 0; i < total_tiles; i++) {
            unsigned int id = get_tmx_tile_id(tilemap, i);

            out << id;

            if (i < total_tiles - 1) {
                out << ",";
            }

            if (i % tilemap_width == tilemap_width - 1) {
                out << "\n";
            }
        }
    } else if (tileSize == TILE_8x16) {
        for (int y = 0; y < tilemap_height; y++) {
            int i = (y / 2) * tilemap_width * 2 + (y % 2);
            for (int x = 0; x < tilemap_width; x++, i += 2) {
                unsigned int id = get_tmx_tile_id(tilemap, i);

                out << id;

                if (i < total_tiles - 1) {
                    out << ",";
                }

                if (x % tilemap_width == tilemap_width - 1) {
                    out << "\n";
                }
            }
        }
    }

    out << "  </data>\n";
    out << " </layer>\n";
    out << "</map>\n";

    out.close();
}

void write_tilemap_file(const Config& config, const char *filename, std::vector<Tile *> *tilemap, int width) {
    std::ofstream out;
    out.open(filename, config.output_bin ?
        std::ofstream::binary : std::ofstream::out);
    std::vector<uint16_t> outbuf;

    if (!config.output_bin) out << ".dw";
    int height = 1;

    int total_tiles = (int)tilemap->size();
    for (int i = 0; i < total_tiles; i++) {
        Tile *t = tilemap->at(i);

        uint16_t id = t->original_tile != nullptr ? (uint16_t) t->original_tile->id : (uint16_t) t->id;
        id += config.tile_start_offset;

        if (t->flipped_x) {
            id = id | TILEMAP_H_FLIP_FLAG;
        }
        if (t->flipped_y) {
            id = id | TILEMAP_V_FLIP_FLAG;
        }

        if (config.use_sprite_pal) {
            id = id | TILEMAP_SPRITE_PALETTE_FLAG;
        }

        if (config.infront_flag) {
            id = id | TILEMAP_INFRONT_FLAG;
        }

        if (!config.output_bin) {
            char buf[5];
            snprintf(buf, 5, "%04X", id);
            out << " $" << buf;
        }

        outbuf.push_back(id);

        if (i % width == width - 1) {
            if (!config.output_bin) out << "\n";
            if (i < total_tiles - 1) {
                if (!config.output_bin) out << ".dw";
                height++;
            }
        }
    }

    // write binary outbuf to file
    int orig_sz = (int)outbuf.size() * 2;

    // compress
    if (config.compress) {
        uint8_t* comp_dat = (uint8_t*)malloc(orig_sz);

        int comp_sz = STM_compressTilemap((uint8_t*)outbuf.data(), width, height, comp_dat, orig_sz);
        if (!config.quiet) {
            std::cout << "Compressed tilemap from " << orig_sz << " bytes to " << comp_sz
                << " (" << (int)(comp_sz / (float)orig_sz * 100) << "%)." << std::endl;
        }

        out.write((const char*)comp_dat, comp_sz);
        free(comp_dat); comp_dat = nullptr;

    // uncompressed binary
    } else if (config.output_bin) out.write((const char*)outbuf.data(), orig_sz);

    out.close();
}

Tile *find_duplicate(Tile *tile, std::vector<Tile *> *tiles) {
    for (auto t : *tiles) {
        if (tile->isDataEqual(t)) {
            return t;
        }
    }

    return nullptr;
}

Tile *createTile(Image *image, int x, int y, int w, int h, std::vector<Tile *> *tiles, bool mirrored) {
    Tile *tile = new Tile(0, false, false, false, nullptr);

    unsigned char *ptr = &image->pixels[0] + y * image->width + x;
    unsigned char *tile_ptr = tile->data;
    for (int i = 0; i < h; i++) {
        memcpy(tile_ptr, ptr, w);
        tile_ptr += w;
        ptr += image->width;
    }

    tile->original_tile = find_duplicate(tile, tiles);
    if (tile->original_tile) {
        tile->is_duplicate = true;
    }

    if (mirrored && !tile->is_duplicate) {
        Tile *flipped = tile->flipX();
        tile->original_tile = find_duplicate(flipped, tiles);
        delete flipped;
        if (tile->original_tile) {
            tile->flipped_x = true;
            tile->is_duplicate = true;
        } else {
            flipped = tile->flipY();
            tile->original_tile = find_duplicate(flipped, tiles);
            delete flipped;
            if (tile->original_tile) {
                tile->flipped_y = true;
                tile->is_duplicate = true;
            } else {
                flipped = tile->flipXY();
                tile->original_tile = find_duplicate(flipped, tiles);
                delete flipped;
                if (tile->original_tile) {
                    tile->flipped_x = true;
                    tile->flipped_y = true;
                    tile->is_duplicate = true;
                }
            }
        }
    }

    return tile;
}

void add_new_tile(std::vector<Tile *> *tiles, Tile *tile) {
    tile->id = (uint16_t)tiles->size();
    tiles->push_back(tile);
}

int process_file(const Config &config) {
    // some extra verbosity
    if (!config.quiet) {
        printf("Processing \"%s\"...\n", config.input_filename);
    }

    Image *image = read_png_file(config.input_filename, config.quiet);
    if (image == nullptr) {
        printf("Failed to open file:  %s\n", config.input_filename);
        return 1;
    }

    if (image->width % TILE_WIDTH != 0) {
        printf("Input image width must be a multiple of %d.\n", TILE_WIDTH);
        exit(1);
    }

    if (config.tileSize == TILE_8x8 && image->height % TILE_HEIGHT != 0) {
        printf("Input image height must be a multiple of %d.\n", TILE_HEIGHT);
        exit(1);
    }

    if (config.tileSize == TILE_8x16 && image->height % 16 != 0) {
        printf("Input image height must be a multiple of 16 when 8x16 tile mode is selected.\n");
        exit(1);
    }

    std::vector<Tile *> tilemap;
    std::vector<Tile *> tiles;

    if (config.tileSize == TILE_8x8) {
        for (unsigned int y = 0; y < image->height; y += TILE_HEIGHT) {
            for (unsigned int x = 0; x < image->width; x += TILE_WIDTH) {
                Tile *tile = createTile(image, x, y, TILE_WIDTH, TILE_HEIGHT, &tiles, config.mirror);

                if (!tile->is_duplicate || !config.remove_dups) {
                    add_new_tile(&tiles, tile);
                }
                tilemap.push_back(tile);
            }
        }
    } else if (config.tileSize == TILE_8x16) {
        for (unsigned int y = 0; y < image->height; y += TILE_HEIGHT * 2) {
            for (unsigned int x = 0; x < image->width; x += TILE_WIDTH) {
                Tile *tile = createTile(image, x, y, TILE_WIDTH, TILE_HEIGHT, &tiles, config.mirror);

                if (!tile->is_duplicate || !config.remove_dups) {
                    add_new_tile(&tiles, tile);
                }
                tilemap.push_back(tile);

                tile = createTile(image, x, y + TILE_HEIGHT, TILE_WIDTH, TILE_HEIGHT, &tiles, config.mirror);

                if (!tile->is_duplicate || !config.remove_dups) {
                    add_new_tile(&tiles, tile);
                }
                tilemap.push_back(tile);
            }
        }
    }
    if (!config.quiet) {
        printf("tilemap: %d, tiles: %d\n", (int) tilemap.size(), (int) tiles.size());
    }

    if (config.output_tile_image_filename != nullptr) {
        write_tiles_to_png_image(config.output_tile_image_filename, image, &tiles);
    }

    if (config.tmx_filename != nullptr) {
        write_tmx_file(config.tmx_filename, image, &tiles, &tilemap, config.tileSize);
    }

    if (config.palette_filename != nullptr) {
        switch (config.paletteOutputFormat) {
            case GEN :
                write_gen_palette_file(config, config.palette_filename, image);
                break;
            case SMS :
                write_sms_palette_file(config, config.palette_filename, image);
                break;
            case SMS_CL123 :
                write_sms_cl123_palette_file(config.palette_filename, image);
                break;
            case GG :
                write_gg_palette_file(config, config.palette_filename, image);
                break;
            default :
                break;
        }
    }

    if (config.tilemap_filename != nullptr) {
        write_tilemap_file(config, config.tilemap_filename, &tilemap, image->width / TILE_WIDTH);
    }

    if (config.tiles_filename != nullptr) {
        write_tiles(config, config.tiles_filename, &tiles);
    }

    delete image;
    return 0;
}

int main(int argc, char **argv) {
    const Config cfg = parse_commandline_opts(argc, argv);
    return process_file(cfg);
}

