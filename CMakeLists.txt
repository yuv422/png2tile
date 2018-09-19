cmake_minimum_required(VERSION 3.3)
project(png2tile)

find_package(PNG REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -Wno-sign-compare")

set(SOURCE_FILES
    compressors/gfxcomp_stm.c
    compressors/gfxcomp_phantasystargaiden.cpp
    main.cpp
)

add_executable(png2tile ${SOURCE_FILES})

include_directories(${PNG_INCLUDE_DIR})
target_link_libraries(png2tile ${PNG_LIBRARY} -lz)

