# PROJECT: imgui
project(imgui)
cmake_minimum_required(VERSION 3.3)

set(SOURCE_FILES "imgui_impl.cpp" "imgui_impl.h")
source_group(source FILES ${SOURCE_FILES})

set(IMGUI_SOURCES 
    ${DEP_ROOT_DIR}/imgui/imgui.cpp
    ${DEP_ROOT_DIR}/imgui/imgui.h
    ${DEP_ROOT_DIR}/imgui/imconfig.h
    ${DEP_ROOT_DIR}/imgui/imgui_demo.cpp
    ${DEP_ROOT_DIR}/imgui/imgui_draw.cpp
    ${DEP_ROOT_DIR}/imgui/imgui_internal.h)
source_group(source\\ImGui FILES ${IMGUI_SOURCES})

set(IMGUIZMO_SOURCES
    ${DEP_ROOT_DIR}/ImGuizmo/ImGuizmo.h 
    ${DEP_ROOT_DIR}/ImGuizmo/ImGuizmo.cpp)
source_group(source\\ImGuizmo FILES ${IMGUIZMO_SOURCES})

set(SHADER_SOURCES imgui.vsc imgui.fsc)
source_group(shaders FILES ${SHADER_SOURCES})

bgfx_add_shaders("${SHADER_SOURCES}" IGNORE IGNORE "shaders_h" TRUE IGNORE SHADER_GEN_FILES)
source_group(shaders\\compiled FILES ${SHADER_GEN_FILES})

# ImGui Drawing for termite
add_library(imgui STATIC ${SOURCE_FILES} ${IMGUI_SOURCES} ${IMGUIZMO_SOURCES} 
            ${SHADER_SOURCES} ${SHADER_GEN_FILES})
target_include_directories(imgui PUBLIC ${DEP_ROOT_DIR}/imgui ${DEP_ROOT_DIR}/ImGuizmo)
set_target_properties(imgui PROPERTIES FOLDER Common ${IOS_GENERAL_PROPERTIES})
target_link_libraries(imgui PRIVATE bx)

