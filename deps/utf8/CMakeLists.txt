cmake_minimum_required(VERSION 3.0)
project(utf8)

set(SOURCE_FILES utf8.c utf8.h)
add_library(utf8 STATIC ${SOURCE_FILES})
set_target_properties(utf8 PROPERTIES FOLDER Deps ${IOS_GENERAL_PROPERTIES})


