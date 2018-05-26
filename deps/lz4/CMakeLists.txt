cmake_minimum_required(VERSION 3.0)
project(lz4)

set(SOURCE_FILES lz4.c lz4.h lz4hc.c lz4hc.h xxhash.h xxhash.c)
add_library(lz4 STATIC ${SOURCE_FILES})
set_target_properties(lz4 PROPERTIES FOLDER Deps ${IOS_GENERAL_PROPERTIES})



