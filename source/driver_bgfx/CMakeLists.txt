# PROJECT: bgfx_driver
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

add_library(bgfx_driver ${BUILD_LIBRARY_TYPE} ${SOURCE_FILES})

target_include_directories(bgfx_driver PRIVATE ${BGFX_INCLUDE_DIR})
target_link_libraries(bgfx_driver PRIVATE ${BGFX_LIBRARY} bx)
set_target_properties(bgfx_driver PROPERTIES FOLDER Plugins)


