cmake_minimum_required(VERSION 3.0)
project(lz4)

set(SOURCE_FILES lz4.c lz4.h lz4hc.c lz4hc.h xxhash.h xxhash.c)
add_library(lz4 STATIC ${SOURCE_FILES})
#target_include_directories(lz4 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(lz4 PROPERTIES FOLDER Deps)



