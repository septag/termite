cmake_minimum_required(VERSION 3.0)
project(AHEasing)

set(SOURCE_FILES AHEasing/easing.c AHEasing/easing.h)

add_library(AHEasing STATIC ${SOURCE_FILES})

target_include_directories(AHEasing INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(AHEasing PROPERTIES FOLDER Deps)
