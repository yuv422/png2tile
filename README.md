# png2tile
[![Build status](https://ci.appveyor.com/api/projects/status/6jmr85ie4nko8rj0?svg=true)](https://ci.appveyor.com/project/yuv422/png2tile)

Convert PNG images into Sega Master System / Mega drive tile format.

It is written in C++ and should be fairly portable. PNG read/write is provided by [LodePNG](https://github.com/lvandeve/lodepng). 

This project is heavily inspired by Maxim's BMP2Tile application https://github.com/maxim-zhao/bmp2tile

Compression code also from Maxim https://github.com/maxim-zhao/bmp2tilecompressors

## Source

https://github.com/yuv422/png2tile

## Usage


    png2tile <png filename> [options]
    
    Option               Effect
    
    -[no]removedupes     Enable/disable the removal of duplicate tiles
                         *default (-removedupes)
    
    -[no]mirror          Enable/disable tile mirroring to further optimise
                         duplicates *default (-mirror)
    
    -tilesize <size>     '8x8'      Treat tile data as 8x8 *default*
                         '8x16'     Treat tile data as 8x16
    
    -tileformat <format> 'planar'   Output tileset data in Planar format. *default* 
                         'chunky'   Output tileset data in chunky
                                    (two pixels per byte) format. 
    
    -tileoffset <n>      The starting index of the first tile. *Default is 0.
                         The offset can be specified in either decimal or hex
                         Hex numbers prefixed with 0x eg. 0x1A
    
    -spritepalette       Set the tilemap bit to make tiles use the sprite palette.
                         *Default is unset.
    
    -infrontofsprites    Set the tilemap bit to make tiles appear in front of
                         sprites. *Default is unset.
    
    -pal <format>        Palette output format
                         gen        Output the palette in GEN/MD colour format
                         sms        Output the palette in SMS colour format
                         gg         Output the palette in GG colour format
                         sms_cl123  Output the palette in SMS colour format
                                    eg cl123, cl333, cl001
    
    -savetiles <filename>
                         Save tile data to <filename>.
    
    -savetilemap <filename>
                         Save tilemap data to <filename>. 
    
    -savepalette <filename>
                         Save palette data to <filename>.
    
    -savetileimage <filename>
                         Save tileset data as a PNG image.
    
    -savetmx <filename>
                         Save tilemap and corresponding tileset in the Tiled
                         mapeditor TMX format.
    
    -binary
                         Output binary files instead of asm source files.
                         Ignored for sms_cl123 palette format, TMX, and PNG output.
    
    -compress
                         Compress output binary files. Uses STM compression for tilemaps
                         and PSG compression for tiles. Implies -binary if not also specified.

## Compiling png2tile

png2tile uses `CMake`.

Compiling should (hopefully) be as simple as

```shell
cmake .
make
```

