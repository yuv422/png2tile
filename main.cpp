#include <cstring>
#include <string>
#include <iostream>

#include <png.h>
#include <vector>
#include <fstream>

#define NUM_PIXELS_IN_TILE 64

#define TILE_HEIGHT 8
#define TILE_WIDTH 8

#define PNG_HEADER_CHECK_SIZE 8

#define NUM_TILE_COLS_IN_PNG_IMAGE 16

#define TMX_FLIP_X_FLAG 0x80000000
#define TMX_FLIP_Y_FLAG 0x40000000

#define MAX_COLOURS 16

#define TILEMAP_H_FLIP_FLAG 0x0200
#define TILEMAP_V_FLIP_FLAG 0x0400
#define TILEMAP_SPRITE_PALETTE_FLAG 0x0800
#define TILEMAP_INFRONT_FLAG 0x1000

typedef struct {
    png_byte bit_depth;
    int width, height;
    png_colorp palette;
    int num_palette_entries;
    int stride;
    png_bytepp row_pointers;
    png_bytep pixels;
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

typedef struct Tile {
    uint16_t id;
    char *data;
    bool flipped_x;
    bool flipped_y;
    bool is_duplicate;
    Tile *original_tile;
} Tile;

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
} Config;

// PNG read/write logic based on code from Guillaume Cottenceau
// http://zarb.org/~gc/html/libpng.html

Image *read_png_file(const char *filename) {
    png_structp png_ptr;
    png_infop info_ptr;

    png_byte color_type;
    png_byte header[PNG_HEADER_CHECK_SIZE];

    Image *image = new Image;

    /* open file and test for it being a png */
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("[read_png_file] File %s could not be opened for reading", filename);
        exit(1);
    }

    fread(header, 1, PNG_HEADER_CHECK_SIZE, fp);
    if (png_sig_cmp((png_bytep) header, 0, PNG_HEADER_CHECK_SIZE)) {
        printf("[read_png_file] File %s is not recognized as a PNG file", filename);
        exit(1);
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr) {
        printf("[read_png_file] png_create_read_struct failed");
        exit(1);
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        printf("[read_png_file] png_create_info_struct failed");
        exit(1);
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        printf("[read_png_file] Error during init_io");
        exit(1);
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, PNG_HEADER_CHECK_SIZE);

    png_read_info(png_ptr, info_ptr);

    image->width = png_get_image_width(png_ptr, info_ptr);
    image->height = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    image->bit_depth = png_get_bit_depth(png_ptr, info_ptr);


    if (color_type != PNG_COLOR_TYPE_PALETTE) {
        printf("[read_png_file] Only indexed PNG files allowed");
        fclose(fp);
        delete image;
        return NULL;
    }

    png_get_PLTE(png_ptr, info_ptr, &image->palette, &image->num_palette_entries);

    if (image->bit_depth > 4) {
        printf("[read_png_file] PNG bit depth > 4. Only the first 16 colours will be used.\n");
    }

    png_set_interlace_handling(png_ptr);
    png_set_packing(png_ptr);
    png_read_update_info(png_ptr, info_ptr);


    /* read file */
    if (setjmp(png_jmpbuf(png_ptr))) {
        printf("[read_png_file] Error during read_image");
        exit(1);
    }

    image->row_pointers = (png_bytepp) malloc(sizeof(png_bytep) * image->height);
    image->stride = image->width;
    image->pixels = (png_bytep) malloc(image->stride * image->height);
    for (int y = 0; y < image->height; y++)
        image->row_pointers[y] = &image->pixels[y * image->stride];


    png_read_image(png_ptr, image->row_pointers);

    fclose(fp);

    return image;
}

void write_png_file(const char *filename, int width, int height, png_byte bit_depth, char *pixels, png_colorp palette,
                    int num_colours) {

    png_structp png_ptr;
    png_infop info_ptr;
    png_bytepp row_pointers;

    row_pointers = (png_bytepp) malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++)
        row_pointers[y] = (png_byte *) &pixels[y * width];

    /* create file */
    FILE *fp = fopen(filename, "wb");
    if (!fp)
        printf("[write_png_file] File %s could not be opened for writing", filename);


    /* initialize stuff */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr)
        printf("[write_png_file] png_create_write_struct failed");

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        printf("[write_png_file] png_create_info_struct failed");

    if (setjmp(png_jmpbuf(png_ptr)))
        printf("[write_png_file] Error during init_io");

    png_init_io(png_ptr, fp);


    /* write header */
    if (setjmp(png_jmpbuf(png_ptr)))
        printf("[write_png_file] Error during writing header");

    bit_depth = 8; //FIXME we should repack the pixels down to the correct bit depth.
    png_set_IHDR(png_ptr, info_ptr, width, height,
                 bit_depth, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_set_PLTE(png_ptr, info_ptr, palette, num_colours);

    png_write_info(png_ptr, info_ptr);

    /* write bytes */
    if (setjmp(png_jmpbuf(png_ptr)))
        printf("[write_png_file] Error during writing bytes");

    png_write_image(png_ptr, row_pointers);


    /* end write */
    if (setjmp(png_jmpbuf(png_ptr)))
        printf("[write_png_file] Error during end of write");

    png_write_end(png_ptr, NULL);

    free(row_pointers);

    fclose(fp);
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
            "                     Ignored for sms_cl123 palette format, TMX, and PNG output.\n\n";
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

    config.output_tile_image_filename = NULL;
    config.tmx_filename = NULL;
    config.palette_filename = NULL;
    config.tilemap_filename = NULL;
    config.tiles_filename = NULL;

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
                    config.tile_start_offset = strtol(argv[i], NULL, 0);
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
            } else {
                printf("Unknown option: '-%s'\n", cmd);
                show_usage();
            }
        }
    }

    if (config.tileSize == TILE_8x16 && config.remove_dups) {
        printf("Warning: remove duplicates has been disabled because 8x16 tile size was selected.\n");
        config.remove_dups = false;
    }

    return config;
}

void write_tiles_to_png_image(const char *output_image_filename, Image *input_image, std::vector<Tile *> *tiles) {
    int output_width = 16;
    int output_height = tiles->size() / output_width;
    if (tiles->size() % output_width != 0) {
        output_height++;
    }

    output_width *= TILE_WIDTH;
    output_height *= TILE_HEIGHT;

    char *pixels = (char *) malloc(output_width * output_height);
    memset(pixels, 0, output_width * output_height);
    int size = (int) tiles->size();

    for (int i = 0; i < size; i++) {
        char *ptr = &pixels[(i / NUM_TILE_COLS_IN_PNG_IMAGE) * output_width * TILE_HEIGHT +
                            (i % NUM_TILE_COLS_IN_PNG_IMAGE) * TILE_WIDTH];
        Tile *tile = tiles->at(i);
        char *tile_data_ptr = tile->data;
        for (int j = 0; j < TILE_WIDTH; j++) {
            memcpy(ptr, tile_data_ptr, TILE_WIDTH);
            tile_data_ptr += TILE_WIDTH;
            ptr += output_width;
        }
    }

    write_png_file(output_image_filename, output_width, output_height, input_image->bit_depth, pixels,
                   input_image->palette, input_image->num_palette_entries);
}

void write_tiles(Config config, const char *filename, std::vector<Tile *> *tiles) {
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
            sprintf(buf, "%03X", i + config.tile_start_offset);
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
                        sprintf(buf, "%02X", byte);
                        out << " $" << buf;
                    }
                    outbuf.push_back(byte);
                }
            }
        } else if (config.tileOutputFormat == TILE_FORMAT_CHUNKY) {
            for (int j = 0; j < NUM_PIXELS_IN_TILE; j += 2) {
                uint8_t outbyte = (uint8_t) (tile->data[j + 1] & 0xF) | ((uint8_t) (tile->data[j] & 0xF) << 4);
                if (!config.output_bin) {
                    sprintf(buf, "%02X", outbyte);
                    out << " $" << buf;
                }
                outbuf.push_back(outbyte);
            }
        }
        if (!config.output_bin) out << "\n";
    }

    // write binary outbuf to file
    if (config.output_bin) out.write((const char*)outbuf.data(), outbuf.size());

    out.close();
}

uint8_t convert_colour_channel_to_2bit(uint8_t c) {
    if (c < 56) return 0;
    if (c < 122) return 1;
    if (c < 188) return 2;
    return 3;
}

void write_sms_palette_file(Config config, const char *filename, Image *input_image) {
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
            sprintf(buf, "%02X", c);
            out << " $" << buf;
        } else out.write((const char*)&c, 1);
    }
    if (!config.output_bin) out << "\n";

    out.close();
}

void write_gg_palette_file(Config config, const char *filename, Image *input_image) {
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
            sprintf(buf, "%04X", c);
            out << " $" << buf;
        } else out.write((const char*)&c, 2);
    }
    if (!config.output_bin) out << "\n";

    out.close();
}

void write_gen_palette_file(Config config, const char *filename, Image *input_image) {
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
            sprintf(buf, "%04X", c);
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

    unsigned int id = t->original_tile != NULL ? (unsigned int) t->original_tile->id : (unsigned int) t->id;
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

    int total_tiles = tilemap->size();

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

void write_tilemap_file(Config config, const char *filename, std::vector<Tile *> *tilemap, int width) {
    std::ofstream out;
    out.open(filename, config.output_bin ?
        std::ofstream::binary : std::ofstream::out);
    std::vector<uint16_t> outbuf;

    if (!config.output_bin) out << ".dw";
    int height = 1;

    int total_tiles = tilemap->size();
    for (int i = 0; i < total_tiles; i++) {
        Tile *t = tilemap->at(i);

        uint16_t id = t->original_tile != NULL ? (uint16_t) t->original_tile->id : (uint16_t) t->id;
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
            sprintf(buf, "%04X", id);
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
    if (config.output_bin) out.write((const char*)outbuf.data(), outbuf.size() * 2);

    out.close();
}

Tile *find_duplicate(Tile *tile, std::vector<Tile *> *tiles) {
    int size = (int) tiles->size();

    for (int i = 0; i < size; i++) {
        Tile *t = tiles->at(i);
        int count = 0;
        for (; count < NUM_PIXELS_IN_TILE; count++) {
            if (t->data[count] != tile->data[count]) {
                break;
            }
        }
        if (count == NUM_PIXELS_IN_TILE) {
            return t;
        }
    }

    return NULL;
}

Tile *new_tile(int id, bool flipped_x, bool flipped_y, bool is_duplicate, Tile *original_tile) {
    Tile *tile = new Tile;

    tile->id = id;

    tile->flipped_x = flipped_x;
    tile->flipped_y = flipped_y;

    tile->is_duplicate = is_duplicate;
    tile->original_tile = original_tile;

    tile->data = (char *) malloc(NUM_PIXELS_IN_TILE);

    return tile;
}

Tile *tile_flip_x(Tile *tile) {
    Tile *flipped_tile = new_tile(tile->id, true, tile->flipped_y, tile->is_duplicate, tile->original_tile);

    for (int y = 0; y < TILE_HEIGHT; y++) {
        for (int x = 0; x < TILE_WIDTH; x++) {
            flipped_tile->data[y * TILE_WIDTH + x] = tile->data[y * TILE_WIDTH + ((TILE_WIDTH - 1) - x)];
        }
    }

    return flipped_tile;
}

Tile *tile_flip_y(Tile *tile) {
    Tile *flipped_tile = new_tile(tile->id, tile->flipped_x, true, tile->is_duplicate, tile->original_tile);

    for (int x = 0; x < TILE_WIDTH; x++) {
        for (int y = 0; y < TILE_HEIGHT; y++) {
            flipped_tile->data[y * TILE_WIDTH + x] = tile->data[(TILE_HEIGHT - 1 - y) * TILE_WIDTH + x];
        }
    }

    return flipped_tile;
}

Tile *tile_flip_xy(Tile *tile) {
    Tile *flipped_x = tile_flip_x(tile);
    Tile *flipped_xy = tile_flip_y(flipped_x);
    delete flipped_x;

    return flipped_xy;
}

Tile *createTile(Image *image, int x, int y, int w, int h, std::vector<Tile *> *tiles, bool mirrored) {
    Tile *tile = new_tile(0, false, false, false, NULL);

    png_bytep ptr = image->pixels + y * image->stride + x;
    char *tile_ptr = tile->data;
    for (int i = 0; i < h; i++) {
        memcpy(tile_ptr, ptr, w);
        tile_ptr += w;
        ptr += image->stride;
    }

    tile->original_tile = find_duplicate(tile, tiles);
    if (tile->original_tile) {
        tile->is_duplicate = true;
    }

    if (mirrored && !tile->is_duplicate) {
        Tile *flipped = tile_flip_x(tile);
        tile->original_tile = find_duplicate(flipped, tiles);
        delete flipped;
        if (tile->original_tile) {
            tile->flipped_x = true;
            tile->is_duplicate = true;
        } else {
            flipped = tile_flip_y(tile);
            tile->original_tile = find_duplicate(flipped, tiles);
            delete flipped;
            if (tile->original_tile) {
                tile->flipped_y = true;
                tile->is_duplicate = true;
            } else {
                flipped = tile_flip_xy(tile);
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
    tile->id = tiles->size();
    tiles->push_back(tile);
}

void process_file(Config config) {
    Image *image = read_png_file(config.input_filename);
    if (image == NULL) {
        return;
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
        for (int y = 0; y < image->height; y += TILE_HEIGHT) {
            for (int x = 0; x < image->width; x += TILE_WIDTH) {
                Tile *tile = createTile(image, x, y, TILE_WIDTH, TILE_HEIGHT, &tiles, config.mirror);

                if (!tile->is_duplicate || !config.remove_dups) {
                    add_new_tile(&tiles, tile);
                }
                tilemap.push_back(tile);
            }
        }
    } else if (config.tileSize == TILE_8x16) {
        for (int y = 0; y < image->height; y += TILE_HEIGHT * 2) {
            for (int x = 0; x < image->width; x += TILE_WIDTH) {
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
    printf("tilemap: %d, tiles: %d\n", (int) tilemap.size(), (int) tiles.size());

    if (config.output_tile_image_filename != NULL) {
        write_tiles_to_png_image(config.output_tile_image_filename, image, &tiles);
    }

    if (config.tmx_filename != NULL) {
        write_tmx_file(config.tmx_filename, image, &tiles, &tilemap, config.tileSize);
    }

    if (config.palette_filename != NULL) {
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

    if (config.tilemap_filename != NULL) {
        write_tilemap_file(config, config.tilemap_filename, &tilemap, image->width / TILE_WIDTH);
    }

    if (config.tiles_filename != NULL) {
        write_tiles(config, config.tiles_filename, &tiles);
    }
}

int main(int argc, char **argv) {
    Config cfg = parse_commandline_opts(argc, argv);
    process_file(cfg);

    return 0;
}

