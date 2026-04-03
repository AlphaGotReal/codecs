cmake_minimum_required(VERSION 3.10)

project(debug-codecs C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil)

function(add_ff TARGET_NAME)
  add_executable(${TARGET_NAME} ${ARGN})
  target_compile_options(${TARGET_NAME} PRIVATE -g -fsanitize=address -fno-omit-frame-pointer)
  target_link_options(${TARGET_NAME} PRIVATE -fsanitize=address)
  target_link_libraries(${TARGET_NAME} PkgConfig::FFMPEG)
endfunction()

add_ff(copyv copyv.c)
add_ff(debug debug.c)
add_ff(h26x h26x.c)
add_ff(vpx vpx.c)
