cmake_minimum_required(VERSION 3.3)
project(png2tile)

find_package(PNG REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp)
add_executable(png2tile ${SOURCE_FILES})


include_directories(${PNG_INCLUDE_DIR})
target_link_libraries(png2tile ${PNG_LIBRARY})