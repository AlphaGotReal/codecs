add_executable(dspeed dspeed.c)
target_link_libraries(dspeed decoder)
target_include_directories(dspeed PRIVATE ${CMAKE_SOURCE_DIR})
