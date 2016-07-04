# PROJECT: tdriver_disk
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

add_library(tdriver_disk SHARED ${SOURCE_FILES})

target_include_directories(tdriver_disk PRIVATE ${LIBUV_INCLUDE_DIR})
target_link_libraries(tdriver_disk PRIVATE ${LIBUV_LIBRARY} bx)

if (WIN32)
    add_dependencies(tdriver_disk LibuvCopyBinaries)
endif()

install(TARGETS tdriver_disk 
        LIBRARY DESTINATION bin/plugins
        RUNTIME DESTINATION bin/plugins)
