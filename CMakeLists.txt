cmake_minimum_required(VERSION 3.10)

project(debug-codecs C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil)

add_executable(main main.c)
target_compile_options(main PRIVATE -g)
target_link_libraries(main PkgConfig::FFMPEG)
