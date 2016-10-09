# PROJECT: stock_render
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

add_library(stock_render ${BUILD_LIBRARY_TYPE} ${SOURCE_FILES})

target_link_libraries(stock_render PRIVATE bx)
set_target_properties(stock_render PROPERTIES FOLDER Plugins)
