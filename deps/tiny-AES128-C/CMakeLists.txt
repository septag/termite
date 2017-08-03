cmake_minimum_required(VERSION 3.0)
project(tiny-AES128-C)

set(SOURCE_FILES aes.c aes.h)
add_library(tiny-AES128-C STATIC ${SOURCE_FILES})
#target_include_directories(tiny-AES128-C INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(tiny-AES128-C PROPERTIES FOLDER Deps)


