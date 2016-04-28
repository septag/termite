# PROJECT: imgui_impl
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*" "*.h")
source_group(source FILES ${SOURCE_FILES})

set(IMGUI_SOURCES 
    ${DEP_ROOT_DIR}/imgui/imgui.cpp
    ${DEP_ROOT_DIR}/imgui/imgui.h
    ${DEP_ROOT_DIR}/imgui/imconfig.h
    ${DEP_ROOT_DIR}/imgui/imgui_demo.cpp
    ${DEP_ROOT_DIR}/imgui/imgui_draw.cpp
    ${DEP_ROOT_DIR}/imgui/imgui_internal.h)
source_group(ImGui FILES ${IMGUI_SOURCES})

set(SHADER_SOURCES imgui.vsc imgui.fsc)
source_group(shaders FILES ${SHADER_SOURCES})

bgfx_add_shaders("${SHADER_SOURCES}" IGNORE IGNORE "shaders_h" TRUE SHADER_GEN_FILES)
source_group(shaders\\compiled FILES ${SHADER_GEN_FILES})

include_directories(${DEP_ROOT_DIR})

add_library(imgui_impl STATIC ${SOURCE_FILES} ${IMGUI_SOURCES} ${SHADER_SOURCES} ${SHADER_GEN_FILES})

