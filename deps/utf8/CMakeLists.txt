cmake_minimum_required(VERSION 3.0)
project(utf8)

set(SOURCE_FILES utf8.c utf8.h)
add_library(utf8 STATIC ${SOURCE_FILES})
#target_include_directories(utf8 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(utf8 PROPERTIES FOLDER Deps)


