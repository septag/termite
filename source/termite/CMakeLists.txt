# PROJECT: termite
cmake_minimum_required(VERSION 3.3)

# Source Files
file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*" "*.h")
file(GLOB INCLUDE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "../../include/termite/*.h")
if (IOS)
    list(APPEND SOURCE_FILES "ios_window.mm")
endif()
source_group(source FILES ${SOURCE_FILES})
source_group(include FILES ${INCLUDE_FILES})

## Shaders
set(SHADER_SOURCES "ddraw.vsc"  "ddraw.fsc" 
                   "sprite.vsc" "sprite.fsc"
                   "vg.vsc"     "vg.fsc"
                   "blit.vsc"   "blit.fsc")
source_group(shaders FILES ${SHADER_SOURCES})
bgfx_add_shaders("${SHADER_SOURCES}" IGNORE "../shaders" "shaders_h" TRUE IGNORE SHADER_GEN_FILES)
source_group(shaders\\compiled FILES ${SHADER_GEN_FILES})

set(FADE_SHADER_SOURCES "effect_fade.vsc" "effect_fade.fsc")
source_group(shaders FILES ${FADE_SHADER_SOURCES})

bgfx_add_shaders("${FADE_SHADER_SOURCES}" "FADE_OUT;FADE_COLOR" "../shaders" "shaders_h" TRUE "out_color" FADEOUT_COLOR_FILES) 
source_group(shaders\\compiled FILES ${FADEOUT_COLOR_FILES})

bgfx_add_shaders("${FADE_SHADER_SOURCES}" "FADE_IN;FADE_COLOR" "../shaders" "shaders_h" TRUE "in_color" FADEIN_COLOR_FILES) 
source_group(shaders\\compiled FILES ${FADEIN_COLOR_FILES})

bgfx_add_shaders("${FADE_SHADER_SOURCES}" "FADE_OUT;FADE_ALPHA" "../shaders" "shaders_h" TRUE "out_alpha" FADEOUT_ALPHA_FILES) 
source_group(shaders\\compiled FILES ${FADEOUT_ALPHA_FILES})

bgfx_add_shaders("${FADE_SHADER_SOURCES}" "FADE_IN;FADE_ALPHA" "../shaders" "shaders_h" TRUE "in_alpha" FADEIN_ALPHA_FILES) 
source_group(shaders\\compiled FILES ${FADEIN_ALPHA_FILES})

add_library(termite ${BUILD_LIBRARY_TYPE} ${SOURCE_FILES} ${INCLUDE_FILES} 
            ${SHADER_SOURCES} ${FADE_SHADER_SOURCES}
            ${SHADER_GEN_FILES} ${FADEOUT_COLOR_FILES} ${FADEIN_COLOR_FILES} ${FADEOUT_ALPHA_FILES} ${FADEIN_ALPHA_FILES})

# STD_LIBS
# libraries for unix/posix
if (ANDROID)
    set(STD_LIBS log dl GLESv2 EGL android)
elseif (UNIX)
    set(STD_LIBS pthread dl)
endif()

# PRIVATE_LIBS
if (CMAKE_COMPILER_IS_GNUCC)
    if (${CMAKE_CXX_COMPILER_VERSION} LESS "6.0.0" AND ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        message(WARNING "Using -lgcc -lgcc_s workaround for gcc bug")
        set(PRIVATE_LIBS gcc gcc_s)        
    endif()
endif()

# statically linked plugin libraries
if (BUILD_STATIC)
    if (ANDROID)
        set(DISK_DRIVER android_assets_driver)
    elseif (USE_DISK_LITE_DRIVER)
        set(DISK_DRIVER disk_lite_driver)
    else()
        set(DISK_DRIVER disk_driver)
    endif()
    set(PLUGIN_LIBS bgfx_driver box2d_driver ${DISK_DRIVER})
endif()

if (NOT IOS)
    set(FCONTEXT_LIB fcontext)
else()
    add_dependencies(termite fcontext)
    get_filename_component(FCONTEXT_LIB ${CMAKE_CURRENT_SOURCE_DIR}/../../out/libfcontext.a ABSOLUTE)
    get_filename_component(FCONTEXT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/deboost.context/include ABSOLUTE)
endif()

target_link_libraries(termite 
    PRIVATE ${FCONTEXT_LIB} imgui_impl ${PLUGIN_LIBS} ${PRIVATE_LIBS}
    PUBLIC bx rapidjson ${STD_LIBS})

# 
get_filename_component(TERMITE_INCLUDE_DIR ../../include/termite ABSOLUTE)
target_include_directories(termite PUBLIC ${TERMITE_INCLUDE_DIR} ${DEP_ROOT_DIR} ${FCONTEXT_INCLUDE_DIR})
if (USE_SDL2)
    target_include_directories(termite PUBLIC ${SDL2_INCLUDE_DIR})
    target_link_libraries(termite PUBLIC ${SDL2_LIBRARY})
endif()

# PCH
if (NOT ANDROID)
    add_precompiled_header(termite pch.h)
endif()

## Install
install(TARGETS termite
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
install(FILES ${INCLUDE_FILES} DESTINATION include/termite)

