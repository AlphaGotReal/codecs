cmake_minimum_required(VERSION 3.10)
project(debug-codecs C)

add_library(hash STATIC hash.c)
target_include_directories(hash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(test)
