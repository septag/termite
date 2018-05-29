cmake_minimum_required(VERSION 3.0)
project(tiny-AES128-C)

set(SOURCE_FILES aes.c aes.h)
add_library(tiny-AES128-C STATIC ${SOURCE_FILES})
set_target_properties(tiny-AES128-C PROPERTIES FOLDER Deps ${IOS_GENERAL_PROPERTIES})


