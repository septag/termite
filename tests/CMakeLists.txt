# PROJECT: TESTS
cmake_minimum_required(VERSION 3.3)

if (WIN32)
    set(${WIN_DEPS} SDL2CopyBinaries)
endif()

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders)

if (ANDROID)
    set(PLATFORM_PLUGINS android_assets_driver)
else()
    set(PLATFORM_PLUGINS disk_driver)
endif()

# test_init
if (NOT ANDROID)
    add_executable(test_init "test_init.cpp")
    target_link_libraries(test_init termite)
    set_target_properties(test_init PROPERTIES FOLDER Tests)

    if (NOT BUILD_STATIC)
        add_dependencies(test_init ${PLATFORM_PLUGINS})
    endif()
endif()

# test_sdl
if (USE_SDL2)
    if (NOT ANDROID)
        add_executable(test_sdl "test_sdl.cpp")
    else()
        add_library(test_sdl SHARED "test_sdl.cpp" ${SDL2_ANDROID_MAIN_ENTRY})
        install(TARGETS test_sdl
            RUNTIME DESTINATION bin
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib)
    endif()

    target_include_directories(test_sdl PUBLIC ${SDL2_INCLUDE_DIR})
    target_link_libraries(test_sdl ${SDL2_LIBRARY} termite)
    set_target_properties(test_sdl PROPERTIES FOLDER Tests)

    if (NOT BUILD_STATIC)
        add_dependencies(test_sdl bgfx_driver ${PLATFORM_PLUGINS} ${WIN_DEPS})
    endif()
endif()
