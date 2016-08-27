# PROJECT: TESTS
cmake_minimum_required(VERSION 3.3)

if (WIN32)
    set(${WIN_DEPS} SDL2CopyBinaries)
endif()

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders)

# test_init
add_executable(test_init "test_init.cpp")
target_link_libraries(test_init termite)

# test_sdl
set(SHADER_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/test_model.vsc 
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/test_model.fsc)
source_group(shaders FILES ${SHADER_SOURCES})
bgfx_add_shaders("${SHADER_SOURCES}" IGNORE IGNORE ${SHADER_OUTPUT_DIR} FALSE IGNORE SHADER_GEN_FILES)
source_group(shaders\\compiled FILES ${SHADER_GEN_FILES})

add_executable(test_sdl "test_sdl.cpp" ${SHADER_SOURCES} ${SHADER_GEN_FILES})
target_include_directories(test_sdl PUBLIC ${SDL2_INCLUDE_DIR})
target_link_libraries(test_sdl termite ${SDL2_LIBRARY})

# test_gui
add_executable(test_gui "test_gui.cpp")
target_include_directories(test_gui PUBLIC ${SDL2_INCLUDE_DIR})
target_link_libraries(test_gui termite ${SDL2_LIBRARY} nanovg)

# test_component
add_executable(test_component "test_component.cpp")
target_include_directories(test_component PUBLIC ${SDL2_INCLUDE_DIR})
target_link_libraries(test_component termite ${SDL2_LIBRARY})

# test_box2d
add_executable(test_box2d "test_box2d.cpp")
target_include_directories(test_box2d PUBLIC ${SDL2_INCLUDE_DIR})
target_link_libraries(test_box2d termite ${SDL2_LIBRARY} Box2D nanovg)

# test_sprite
add_executable(test_sprite "test_sprite.cpp")
target_include_directories(test_sprite PUBLIC ${SDL2_INCLUDE_DIR})
target_link_libraries(test_sprite termite ${SDL2_LIBRARY} Box2D nanovg)

# Require plugins to be built
add_dependencies(test_init tdriver_disk)
add_dependencies(test_sdl trender_stock tdriver_disk tdriver_bgfx ${WIN_DEPS})

