# PROJECT: termite
cmake_minimum_required(VERSION 3.3)

# Source Files
file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*" "*.h")
file(GLOB INCLUDE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "../../include/termite/*.h")
source_group(source FILES ${SOURCE_FILES})
source_group(include FILES ${INCLUDE_FILES})

## Shaders
file(GLOB SHADER_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.?sc")
source_group(shaders FILES ${SHADER_SOURCES})
bgfx_add_shaders("${SHADER_SOURCES}" IGNORE IGNORE "shaders_h" TRUE IGNORE SHADER_GEN_FILES)
source_group(shaders\\compiled FILES ${SHADER_GEN_FILES})

add_library(termite ${BUILD_LIBRARY_TYPE} ${SOURCE_FILES} ${INCLUDE_FILES} ${SHADER_SOURCES} ${SHADER_GEN_FILES})

# libraries for unix/posix
if (ANDROID)
    set(STD_LIBS log dl GLESv2 EGL android)
elseif (UNIX)
    set(STD_LIBS pthread dl)
endif()

if (CMAKE_COMPILER_IS_GNUCC)
    if (${CMAKE_CXX_COMPILER_VERSION} LESS "6.5.0")
        message(WARNING "Using -lgcc -lgcc_s workaround for gcc bug")
        set(PRIVATE_LIBS gcc gcc_s)        
    endif()
endif()

# statically linked plugin libraries
if (BUILD_STATIC)
    if (ANDROID)
        set(DISK_DRIVER android_assets_driver)
    else()
        set(DISK_DRIVER disk_driver)
    endif()
    set(PLUGIN_LIBS bgfx_driver ${DISK_DRIVER})
endif()

target_link_libraries(termite 
    PRIVATE fcontext imgui_impl ${PLUGIN_LIBS} ${PRIVATE_LIBS}
    PUBLIC bx ${STD_LIBS})

# 
get_filename_component(TERMITE_INCLUDE_DIR ../../include/termite ABSOLUTE)
target_include_directories(termite PUBLIC ${TERMITE_INCLUDE_DIR} ${DEP_ROOT_DIR})
if (USE_SDL2)
    target_include_directories(termite PUBLIC ${SDL2_INCLUDE_DIR})
    target_link_libraries(termite PUBLIC ${SDL2_LIBRARY})
endif()

# PCH
add_precompiled_header(termite pch.h)

## Install
install(TARGETS termite
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
install(FILES ${INCLUDE_FILES} DESTINATION include/termite)

