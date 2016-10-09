# PROJECT: disk_driver
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

add_library(disk_driver ${BUILD_LIBRARY_TYPE} ${SOURCE_FILES})

target_include_directories(disk_driver PRIVATE ${LIBUV_INCLUDE_DIR})
target_link_libraries(disk_driver PRIVATE ${LIBUV_LIBRARY} bx)
set_target_properties(disk_driver PROPERTIES FOLDER Plugins)

if (WIN32)
    add_dependencies(disk_driver LibuvCopyBinaries)
endif()

