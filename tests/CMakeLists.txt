# PROJECT: TESTS
cmake_minimum_required(VERSION 3.3)

if (WIN32)
    set(${WIN_DEPS} SDL2CopyBinaries)
endif()

# test_init
add_executable(test_init "test_init.cpp")
target_link_libraries(test_init termite)

# test_sdl
set(SHADER_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/test_model.vsc 
    ${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/test_model.fsc)
source_group(shaders FILES ${SHADER_SOURCES})
bgfx_add_shaders("${SHADER_SOURCES}" IGNORE IGNORE ${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders FALSE IGNORE SHADER_GEN_FILES)
source_group(shaders\\compiled FILES ${SHADER_GEN_FILES})

add_executable(test_sdl "test_sdl.cpp" ${SHADER_SOURCES} ${SHADER_GEN_FILES})
target_include_directories(test_sdl PUBLIC ${SDL2_INCLUDE_DIR})
target_link_libraries(test_sdl termite ${SDL2_LIBRARY})

# Require plugins to be built
add_dependencies(test_init tdriver_disk)

add_dependencies(test_sdl trender_default tdriver_disk tdriver_bgfx ${WIN_DEPS})

install(TARGETS test_init RUNTIME DESTINATION bin)
install(TARGETS test_sdl RUNTIME DESTINATION bin)
