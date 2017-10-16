PROJECT(miniz)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(miniz_SOURCE miniz.c miniz_zip.c miniz_tinfl.c miniz_tdef.c)

add_library(miniz STATIC ${miniz_SOURCE})
set_target_properties(miniz PROPERTIES FOLDER Deps)

