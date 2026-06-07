pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(player player.c)
target_link_libraries(player PRIVATE decoder PkgConfig::SDL2)
target_include_directories(player PRIVATE ${CMAKE_SOURCE_DIR})
