# PROJECT: android_assets_driver
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

add_library(android_assets_driver ${BUILD_LIBRARY_TYPE} ${SOURCE_FILES})

target_link_libraries(android_assets_driver PRIVATE bx)
set_target_properties(android_assets_driver PROPERTIES FOLDER Plugins)

