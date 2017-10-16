cmake_minimum_required(VERSION 3.0)
project(etcpack)

set(SOURCE_FILES etcdec.cxx)

add_library(etcpack STATIC ${SOURCE_FILES})
set_target_properties(etcpack PROPERTIES FOLDER Deps)


