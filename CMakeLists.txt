cmake_minimum_required(VERSION 3.3)
project(png2tile)

set (CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
    compressors/gfxcomp_stm.c
    compressors/gfxcomp_phantasystargaiden.cpp
    lodepng.cpp
    lodepng.h
    main.cpp
    tile.cpp
    tile.h)

add_executable(png2tile ${SOURCE_FILES})

target_link_libraries(png2tile)

