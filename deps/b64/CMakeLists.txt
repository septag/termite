cmake_minimum_required(VERSION 3.0)
project(b64)

set(SOURCE_FILES decode.c encode.c b64.h)
add_library(b64 STATIC ${SOURCE_FILES})
set_target_properties(b64 PROPERTIES FOLDER Deps)


