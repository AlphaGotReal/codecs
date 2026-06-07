cmake_minimum_required(VERSION 3.10)
project(codecs C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(YAML  REQUIRED IMPORTED_TARGET yaml-0.1)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)

add_library(hash STATIC hash.c)
target_include_directories(hash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(opts STATIC opts.c)
target_include_directories(opts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(opts PUBLIC PkgConfig::YAML)

add_library(pixfmt STATIC pixfmt.c)
target_include_directories(pixfmt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pixfmt PUBLIC hash)

add_library(decoder STATIC decoder.c)
target_include_directories(decoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(decoder PUBLIC PkgConfig::FFMPEG pixfmt)

add_library(encoder STATIC encoder.c)
target_include_directories(encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(encoder PUBLIC PkgConfig::FFMPEG hash opts pixfmt)

add_subdirectory(gui)
add_subdirectory(test)
