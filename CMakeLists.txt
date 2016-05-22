cmake_minimum_required(VERSION 3.0)
project(bx)

add_library(bx INTERFACE)

set(BX_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/deps/bx/include)
if (MSVC)
    set(COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/compat/msvc)
elseif(APPLE)
    set(COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/compat/osx)
endif()

if(UNIX)
    set(COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/compat/posix)
endif()

target_include_directories(bx INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include ${COMPAT_DIR})
