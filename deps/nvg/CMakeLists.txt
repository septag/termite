# PROJECT: nanovg
# Extracted from bgfx project

cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
set(SOURCE_FILES ${SOURCE_FILES} "fontstash.h")
set(INCLUDE_FILES "nanovg.h")

source_group(source FILES ${SOURCE_FILES})
source_group(includes FILES ${INCLUDE_FILES})

set(SHADER_SOURCES nanovg_fill.vsc nanovg_fill.fsc)
source_group(shaders FILES ${SHADER_SOURCES})

bgfx_add_shaders("${SHADER_SOURCES}" IGNORE IGNORE "shaders_h" TRUE IGNORE SHADER_GEN_FILES)
source_group(shaders\\compiled FILES ${SHADER_GEN_FILES})

# ImGui Drawing for termite
add_library(nanovg STATIC ${SOURCE_FILES} ${INCLUDE_FILES} ${SHADER_SOURCES} ${SHADER_GEN_FILES})
target_link_libraries(nanovg PRIVATE bx)

