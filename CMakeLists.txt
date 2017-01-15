cmake_minimum_required(VERSION 3.3)
project(termite)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/install CACHE PATH "Install path" FORCE)

# Default output directories for runtime and libraries
if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out)
endif()
if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out)
endif()

if (NOT EXISTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

set(DEP_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(DOWNLOAD_URL_ROOT "http://78.46.11.243/packages")

if (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Profile;Development" CACHE STRING "" FORCE)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# load modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PrecompiledHeader)
include(Utility)
include(DownloadPackage)
include(Bgfx)

# options
option(USE_BOX2D "Build Box2D Physics engine" ON)
option(BUILD_FISHERBOY "Build Fisherboy Game" OFF)
option(USE_SDL2 "Use SDL2 with the engine" ON)
option(BUILD_STATIC "Force Static library builds" OFF)
if (NOT ANDROID AND NOT IOS)
    option(BUILD_TOOLS "Build tools" ON)
else()
    option(BUILD_TOOLS "Build tools" OFF)
endif()

# Dependencies

# Run find package scripts
if (USE_SDL2)
    include(FindSDL2)
endif()

include(FindAssimp)
include(FindLibuv)
include(FindBgfx)

# standard windows dependencies
if (WIN32)
    if (NOT ASSIMP_FOUND)
        download_package(${DOWNLOAD_URL_ROOT}/assimp_vc140.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
        include(FindAssimp)
    endif()
    if (NOT LIBUV_FOUND)
        download_package(${DOWNLOAD_URL_ROOT}/libuv_vc140.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
        include(FindLibuv)
    endif()
    if (USE_SDL2 AND (NOT SDL2_FOUND))
        download_package(${DOWNLOAD_URL_ROOT}/sdl_vc140.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
        include (FindSDL2)
    endif()
endif()

# Download dependencies that doesn't have packages on any platform
if (NOT BGFX_FOUND)
    if (ANDROID)
        download_package(${DOWNLOAD_URL_ROOT}/bgfx_android_arm.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
    elseif (IOS)
        download_package(${DOWNLOAD_URL_ROOT}/bgfx_ios_arm.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
    elseif (MSVC)
        download_package(${DOWNLOAD_URL_ROOT}/bgfx_vc140.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        download_package(${DOWNLOAD_URL_ROOT}/bgfx_linux64.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        download_package(${DOWNLOAD_URL_ROOT}/bgfx_osx64.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
    endif()

    include(FindBgfx)
endif()

if (BGFX_FOUND)
    if (WIN32)
        message(STATUS "Found Bgfx library")
    else()
        message(STATUS "Found Bgfx Library: ${BGFX_LIBRARY}")
    endif()
else()
    message(FATAL_ERROR "Bgfx library not found")
endif()

if (NOT ANDROID AND NOT IOS)
    if (LIBUV_FOUND)
        if (WIN32)
            libuv_copy_binaries(${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
            message(STATUS "Found libuv library")    
        else()
            message(STATUS "Found libuv library: ${LIBUV_LIBRARY}")
        endif()
    else()
        message(FATAL_ERROR "Libuv library not found")
    endif()
endif()

if (USE_SDL2)   
    if (SDL2_FOUND)
        if (WIN32)
            sdl2_copy_binaries(${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
            message(STATUS "Found SDL2 library")    
        else()
            message(STATUS "Found SDL2 library: ${SDL2_LIBRARY}")
        endif()
    else()
        message(FATAL_ERROR "SDL2 library not found")
    endif()
endif()

if (BUILD_TOOLS)
    if (ASSIMP_FOUND)
        if (WIN32)
            assimp_copy_binaries(${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
            message(STATUS "Found Assimp library")    
        else()
            message(STATUS "Found Assimp library: ${ASSIMP_LIBRARY}")
        endif()
    elseif(NOT ANDROID and NOT IOS) 
        message(FATAL_ERROR "Assimp library not found")
    endif()
endif()

################################################################################
# Compilation flags
if(MSVC)
    add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
    add_definitions(-D_HAS_EXCEPTIONS=0)
    add_definitions(-D_HAS_ITERATOR_DEBUGGING=0)
    add_definitions(-D_SCL_SECURE=0)
    add_definitions(-D_SECURE_SCL=0)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)

    remove_cxx_flag("/EHsc")
    remove_cxx_flag("/GR")
    add_compile_options(/GR-)
else()
    add_compile_options(-std=c++11 -fPIC -Wno-switch -fno-rtti -fno-exceptions)
endif()

if (ANDROID)    
    add_definitions(-D__STDC_LIMIT_MACROS 
                    -D__STDC_CONSTANT_MACROS)
    set(BUILD_STATIC 1)
endif()

if (IOS)
    set(BUILD_STATIC 1)
endif()

if (BUILD_STATIC)
    add_definitions(-Dtermite_STATIC_LIB)
    set(BUILD_LIBRARY_TYPE STATIC)
else()
    add_definitions(-Dtermite_SHARED_LIB)
    set(BUILD_LIBRARY_TYPE SHARED)
endif()

if (USE_SDL2)
    add_definitions(-Dtermite_SDL2)
endif()

add_definitions(-Dtermite_SHADER_APPEND_PATH=${CMAKE_SYSTEM_NAME})

# copy release flags into profile
copy_build_flags(PROFILE)
copy_build_flags(DEVELOPMENT)

# http://stackoverflow.com/questions/33829152/for-cmake-can-you-modify-the-release-debug-compiler-flags-with-add-compiler-fl
set(DEBUG_COMPILE_OPTIONS "-DBX_CONFIG_ALLOCATOR_DEBUG" "-Dtermite_DEV=1")
add_compile_options("$<$<CONFIG:Debug>:${DEBUG_COMPILE_OPTIONS}>")

set(DEVELOPMENT_COMPILE_OPTIONS "-Dtermite_DEV=1")
add_compile_options("$<$<CONFIG:Development>:${DEVELOPMENT_COMPILE_OPTIONS}>")

set(PROFILE_COMPILE_OPTIONS "-Dtermite_DEV=0")
add_compile_options("$<$<CONFIG:Profile>:${PROFILE_COMPILE_OPTIONS}>")

set(RELEASE_COMPILE_OPTIONS "-Dtermite_DEV=0")
add_compile_options("$<$<CONFIG:Release>:${RELEASE_COMPILE_OPTIONS}>")

# Common include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Sub projects
# dependencies
add_subdirectory(deps/deboost.context)
add_subdirectory(deps/bx)
add_subdirectory(deps/nvg)
set_target_properties(fcontext PROPERTIES FOLDER Deps)
set_target_properties(nanovg PROPERTIES FOLDER Deps)

# common
add_subdirectory(source/imgui_impl)

## main
add_subdirectory(source/termite)

# plugins
add_subdirectory(source/driver_bgfx)
if (ANDROID)
    add_subdirectory(source/driver_android_assets)
elseif(NOT IOS)
    add_subdirectory(source/driver_disk)
endif()

if (USE_BOX2D)
    add_subdirectory(deps/Box2D)
    set_target_properties(Box2D PROPERTIES FOLDER Deps)

    add_subdirectory(source/driver_box2d)
endif()

#add_subdirectory(source/render_stock)

# tools
if (BUILD_TOOLS)
    add_subdirectory(source/ls-model)
    add_subdirectory(source/modelc)
    add_subdirectory(source/animc)
endif()

# tests
add_subdirectory(tests)

# copy binaries
if (WIN32)
    set_target_properties(AssimpCopyBinaries PROPERTIES FOLDER Deps/CopyBinaries) 
    set_target_properties(LibuvCopyBinaries PROPERTIES FOLDER Deps/CopyBinaries) 
    set_target_properties(SDL2CopyBinaries PROPERTIES FOLDER Deps/CopyBinaries) 
endif()

# Digger
if (BUILD_FISHERBOY)
    add_subdirectory(fisherboy)
endif()


