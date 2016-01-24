#include <iostream>

#include <png.h>
#include <vector>

using namespace std;

#define NUM_PIXELS_IN_TILE 64

#define TILE_HEIGHT 8
#define TILE_WIDTH 8

#define PNG_HEADER_CHECK_SIZE 8

#define NUM_TILE_COLS_IN_PNG_IMAGE 16

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
    SMS,
    SMS_CL123,
    GG
} PaletteOutputFormat;

typedef enum {
    TILE_8x8,
    TILE_8x16
} TileSize;

typedef struct Tile {
    int id;
    char *data;
    bool flipped_x;
    bool flipped_y;
    bool is_duplicate;
    Tile *original_tile;
} Tile;

typedef struct {
    const char *input_filename;
    const char *output_tile_image_filename;
    bool mirror;
    bool remove_dups;
    PaletteOutputFormat paletteOutputFormat;
    TileSize tileSize;
    int tile_start_offset;
} Config;

// PNG read/write logic based on code from Guillaume Cottenceau
// http://zarb.org/~gc/html/libpng.html

Image *read_png_file(const char * filename) {
    png_structp png_ptr;
    png_infop info_ptr;

    png_byte color_type;
    png_byte header[PNG_HEADER_CHECK_SIZE];

    Image *image =  new Image;

    /* open file and test for it being a png */
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        printf("[read_png_file] File %s could not be opened for reading", filename);
    fread(header, 1, PNG_HEADER_CHECK_SIZE, fp);
    if (png_sig_cmp((png_bytep)header, 0, PNG_HEADER_CHECK_SIZE))
        printf("[read_png_file] File %s is not recognized as a PNG file", filename);

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr)
        printf("[read_png_file] png_create_read_struct failed");

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        printf("[read_png_file] png_create_info_struct failed");

    if (setjmp(png_jmpbuf(png_ptr)))
        printf("[read_png_file] Error during init_io");

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, PNG_HEADER_CHECK_SIZE);

    png_read_info(png_ptr, info_ptr);

    image->width = png_get_image_width(png_ptr, info_ptr);
    image->height = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    image->bit_depth = png_get_bit_depth(png_ptr, info_ptr);


    if(color_type != PNG_COLOR_TYPE_PALETTE) {
        printf("[read_png_file] Only indexed PNG files allowed");
        fclose(fp);
        delete image;
        return NULL;
    }

    png_get_PLTE(png_ptr, info_ptr, &image->palette, &image->num_palette_entries);

    if(image->bit_depth > 4) {
        printf("[read_png_file] PNG bit depth > 4. Only the first 16 colours will be used.\n");
    }

    png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);


    /* read file */
    if (setjmp(png_jmpbuf(png_ptr)))
        printf("[read_png_file] Error during read_image");

    png_set_packing(png_ptr);

    image->row_pointers = (png_bytepp) malloc(sizeof(png_bytep) * image->height);
    image->stride = image->width;
    image->pixels = (png_bytep) malloc(image->stride * image->height);
    for (int y=0; y<image->height; y++)
        image->row_pointers[y] = &image->pixels[y * image->stride];


    png_read_image(png_ptr, image->row_pointers);

    fclose(fp);

    return image;
}

void write_png_file(const char *filename, int width, int height, png_byte bit_depth, char *pixels, png_colorp palette, int num_colours) {

    png_structp png_ptr;
    png_infop info_ptr;
    png_bytepp row_pointers;

    row_pointers = (png_bytepp) malloc(sizeof(png_bytep) * height);
    for (int y=0; y<height; y++)
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
    printf("Usage:\n\npng2tile <input file> [options]\n");
}

Config parse_commandline_opts(int argc, char **argv) {
    Config config;

    if (argc<2 || argv[1][0] == '-') {
        show_usage();
        exit(1);
    }

    config.input_filename = argv[1];
    config.remove_dups = true;
    config.mirror = true;
    config.paletteOutputFormat = SMS;
    config.tileSize = TILE_8x8;
    config.tile_start_offset = 0;

    config.output_tile_image_filename = NULL;

    for(int i=2;i<argc;i++) {
        const char *option = argv[i];
        if(option[0] == '-') {
           const char *cmd = &option[1];
           if(strcmp(cmd, "removedupes")==0) {
               config.remove_dups = true;
           } else if (strcmp(cmd, "noremovedupes")==0) {
               config.remove_dups = false;
           } else if (strcmp(cmd, "mirror")==0) {
               config.mirror = true;
           } else if (strcmp(cmd, "nomirror")==0) {
               config.mirror = false;
           } else if (strcmp(cmd, "8x8")==0) {

           } else if (strcmp(cmd, "8x16")==0) {

           } else if (strcmp(cmd, "planar")==0) {

           } else if (strcmp(cmd, "chunky")==0) {

           } else if (strcmp(cmd, "tileoffset")==0) {
               i++;
               if (i<argc) {
                   config.tile_start_offset = atoi(argv[i]); //FIXME handle hex in the format 0x123 or $123
               }
           } else if (strcmp(cmd, "spritepalette")==0) {

           } else if (strcmp(cmd, "infrontofsprites")==0) {

           } else if (strcmp(cmd, "pal")==0) {

           } else if (strcmp(cmd, "savetiles")==0) {

           } else if (strcmp(cmd, "savetilemap")==0) {

           } else if (strcmp(cmd, "savepalette")==0) {

           } else if (strcmp(cmd, "savetileimage")==0) {
               i++;
               if (i<argc) {
                   config.output_tile_image_filename = argv[i];
               }
           } else {
               show_usage();
           }
        }
    }
    return config;
}

void write_tiles_to_png_image(const char *output_image_filename, Image *input_image, vector<Tile*> *tiles) {
    int output_width = 16;
    int output_height = tiles->size() / output_width;
    if (tiles->size() % output_width != 0) {
        output_height++;
    }

    output_width *= TILE_WIDTH;
    output_height *= TILE_HEIGHT;

    char *pixels = (char *)malloc(output_width * output_height);
    memset(pixels,0, output_width * output_height);
    int size = (int)tiles->size();

    for(int i=0;i<size;i++) {
        char *ptr = &pixels[(i/NUM_TILE_COLS_IN_PNG_IMAGE) * output_width * TILE_HEIGHT + (i%NUM_TILE_COLS_IN_PNG_IMAGE) * TILE_WIDTH];
        Tile *tile = tiles->at(i);
        char *tile_data_ptr = tile->data;
        for(int j=0;j<TILE_WIDTH;j++) {
            memcpy(ptr, tile_data_ptr, TILE_WIDTH);
            tile_data_ptr += TILE_WIDTH;
            ptr += output_width;
        }
    }
    write_png_file(output_image_filename, output_width, output_height, input_image->bit_depth, pixels, input_image->palette, input_image->num_palette_entries);

    //write_png_file(output_image_filename, input_image->width, input_image->height, input_image->bit_depth, (char *)input_image->pixels, input_image->palette, input_image->num_palette_entries);
}

Tile *find_duplicate(Tile *tile, vector<Tile*> *tiles) {
    int size = (int)tiles->size();

    for(int i=0;i<size;i++) {
        Tile *t = tiles->at(i);
        int count=0;
        for(;count<NUM_PIXELS_IN_TILE;count++) {
            if(t->data[count] != tile->data[count]) {
                break;
            }
        }
        if(count == NUM_PIXELS_IN_TILE) {
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

    tile->data = (char *)malloc(NUM_PIXELS_IN_TILE);

    return tile;
}

Tile *tile_flip_x(Tile *tile) {
    Tile *flipped_tile = new_tile(tile->id, true, tile->flipped_y, tile->is_duplicate, tile->original_tile);

    for(int y=0;y<TILE_HEIGHT;y++) {
        for(int x=0;x<TILE_WIDTH;x++) {
            flipped_tile->data[y*TILE_WIDTH+x] = tile->data[y*TILE_WIDTH+((TILE_WIDTH-1)-x)];
        }
    }

    return flipped_tile;
}

Tile *tile_flip_y(Tile *tile) {
    Tile *flipped_tile = new_tile(tile->id, tile->flipped_x, true, tile->is_duplicate, tile->original_tile);

    for(int x=0;x<TILE_WIDTH;x++) {
        for(int y=0;y<TILE_HEIGHT;y++) {
            flipped_tile->data[y*TILE_WIDTH+x] = tile->data[(TILE_HEIGHT-1-y)*TILE_WIDTH+x];
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

Tile *createTile(Image *image, int x, int y, int w, int h, vector<Tile*> *tiles, bool mirrored) {
    Tile *tile = new_tile(0, false, false, false, NULL);

    png_bytep ptr = image->pixels + y * image->stride + x;
    char *tile_ptr = tile->data;
    for(int i=0;i<h;i++) {
        memcpy(tile_ptr, ptr, w);
        tile_ptr += w;
        ptr += image->stride;
    }

    tile->original_tile = find_duplicate(tile, tiles);
    if(tile->original_tile) {
        tile->is_duplicate = true;
    }

    if(mirrored && !tile->is_duplicate) {
        Tile *flipped = tile_flip_x(tile);
        tile->original_tile = find_duplicate(flipped, tiles);
        delete flipped;
        if(tile->original_tile) {
            tile->flipped_x = true;
            tile->is_duplicate = true;
        } else {
            flipped = tile_flip_y(tile);
            tile->original_tile = find_duplicate(flipped, tiles);
            delete flipped;
            if(tile->original_tile) {
                tile->flipped_y = true;
                tile->is_duplicate = true;
            } else {
                flipped = tile_flip_xy(tile);
                tile->original_tile = find_duplicate(flipped, tiles);
                delete flipped;
                if(tile->original_tile) {
                    tile->flipped_x = true;
                    tile->flipped_y = true;
                    tile->is_duplicate = true;
                }
            }
        }
    }

    return tile;
}

void add_new_tile(int tile_start_offset, vector<Tile*> *tiles, Tile *tile) {
    tile->id = tile_start_offset + tiles->size();
    tiles->push_back(tile);
}

void process_file(Config config) {
    Image *image = read_png_file(config.input_filename);
    if(image == NULL) {
        return;
    }

    if(image->width % TILE_WIDTH != 0) {
        printf("Input image width must be a multiple of %d.", TILE_WIDTH);
        exit(1);
    }

    if(config.tileSize == TILE_8x8 && image->height % TILE_HEIGHT != 0) {
        printf("Input image height must be a multiple of %d.", TILE_HEIGHT);
        exit(1);
    }

    if(config.tileSize == TILE_8x16 && image->height % 16 != 0) {
        printf("Input image height must be a multiple of 16 when 8x16 tile mode is selected.");
        exit(1);
    }

    vector<Tile*> tilemap;
    vector<Tile*> tiles;

    for(int y=0; y < image->height; y+=TILE_HEIGHT) {
        for(int x=0; x < image->width; x+=TILE_WIDTH) {
            Tile *tile = createTile(image, x, y, TILE_WIDTH, TILE_HEIGHT, &tiles, config.mirror);

            if(!tile->is_duplicate || !config.remove_dups) {
                add_new_tile(config.tile_start_offset, &tiles, tile);
            }
            tilemap.push_back(tile);
        }
    }

    printf("tilemap: %d, tiles: %d\n", (int)tilemap.size(), (int)tiles.size());

    if (config.output_tile_image_filename != NULL) {
        write_tiles_to_png_image(config.output_tile_image_filename, image, &tiles);
    }
}


int main(int argc, char **argv) {
    Config cfg = parse_commandline_opts(argc, argv);
    process_file(cfg);

    return 0;
}