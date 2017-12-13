cmake_minimum_required(VERSION 3.0)
project(bx)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/*.c*")
list(REMOVE_ITEM SOURCE_FILES "src/amalgamated.cpp")
file(GLOB INCLUDE_FILES_BX RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "include/bx/*.h") 
file(GLOB INCLUDE_FILES_BXX RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "include/bxx/*.h")
source_group(source FILES ${SOURCE_FILES})
source_group(bx_include FILES ${INCLUDE_FILES_BX})
source_group(bxx_include FILES ${INCLUDE_FILES_BXX})

add_definitions("-D__STDC_LIMIT_MACROS" "-D__STDC_FORMAT_MACROS" "-D__STDC_CONSTANT_MACROS")

add_library(bx STATIC ${SOURCE_FILES} ${INCLUDE_FILES_BX} ${INCLUDE_FILES_BXX})

if (MSVC)
    set(COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/compat/msvc)
elseif(APPLE)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/compat/osx)
    elseif (IOS)
        set(COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/compat/ios)
    endif()
endif()

#
target_include_directories(bx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${COMPAT_DIR})
