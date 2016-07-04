# PROJECT: tdriver_bgfx
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

add_library(tdriver_bgfx SHARED ${SOURCE_FILES})

target_include_directories(tdriver_bgfx PRIVATE ${BGFX_INCLUDE_DIR})
target_link_libraries(tdriver_bgfx PRIVATE ${BGFX_LIBRARY} bx)

install(TARGETS tdriver_bgfx 
        LIBRARY DESTINATION bin/plugins
        RUNTIME DESTINATION bin/plugins)

