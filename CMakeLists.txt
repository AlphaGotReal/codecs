cmake_minimum_required(VERSION 3.10)
project(debug-codecs C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(YAML REQUIRED IMPORTED_TARGET yaml-0.1)

add_library(hash STATIC hash.c)
target_include_directories(hash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(opts STATIC opts.c)
target_include_directories(opts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(opts PUBLIC PkgConfig::YAML)

add_subdirectory(test)
