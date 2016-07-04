# PROJECT: termite
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*" "*.h")
file(GLOB INCLUDE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "../../include/termite/*.h")
source_group(source FILES ${SOURCE_FILES})
source_group(include FILES ${INCLUDE_FILES})

# Shaders
file(GLOB SHADER_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.?sc")
source_group(shaders FILES ${SHADER_SOURCES})
bgfx_add_shaders("${SHADER_SOURCES}" IGNORE IGNORE "shaders_h" TRUE IGNORE SHADER_GEN_FILES)
source_group(shaders\\compiled FILES ${SHADER_GEN_FILES})

add_library(termite SHARED ${SOURCE_FILES} ${INCLUDE_FILES} ${SHADER_SOURCES} ${SHADER_GEN_FILES})

# libraries for unix/posix
if (UNIX)
    set(POSIX_LIBS pthread dl)
endif()

target_link_libraries(termite 
    PRIVATE fcontext
    PUBLIC bx ${POSIX_LIBS})

#
get_filename_component(TERMITE_INCLUDE_DIR ../../include/termite ABSOLUTE)
target_include_directories(termite PUBLIC ${TERMITE_INCLUDE_DIR} ${DEP_ROOT_DIR})

# PCH
add_precompiled_header(termite pch.h)

# Install
install(TARGETS termite
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
install(FILES ${INCLUDE_FILES} DESTINATION include/termite)

