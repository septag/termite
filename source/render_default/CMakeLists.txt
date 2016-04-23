# PROJECT: trender_default
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

include_directories(${DEP_ROOT_DIR})

add_library(trender_default SHARED ${SOURCE_FILES})

target_link_libraries(trender_default termite)
target_link_libraries(trender_default imgui_impl)

install(TARGETS trender_default 
        LIBRARY DESTINATION bin/plugins
        RUNTIME DESTINATION bin/plugins)
