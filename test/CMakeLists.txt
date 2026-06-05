pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil)

function(add_ff TARGET_NAME)
  add_executable(${TARGET_NAME} ${ARGN})
  target_link_libraries(${TARGET_NAME} PkgConfig::FFMPEG)
endfunction()

add_ff(h26x h26x.c)
add_ff(vpx vpx.c)
add_ff(av1 av1.c)
add_ff(h264_nvenv h264_nvenc.c)
add_ff(av1_nvenv av1_nvenc.c)

add_executable(test_hash test_hash.c)
target_link_libraries(test_hash hash)
target_include_directories(test_hash PRIVATE ${CMAKE_SOURCE_DIR})
