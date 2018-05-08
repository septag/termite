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
option(BUILD_MODELVIEWER "Build model viewer" OFF)
option(USE_SDL2 "Use SDL2 with the engine" ON)
option(USE_SDL2_MIXER "Use SDL2 Mixer sound driver, this option implies that you set USE_SDL2 too" ON)
option(USE_CURL "Use Curl and OpenSSL libraries" ON)
option(BUILD_STATIC "Force Static library builds" OFF)
if (NOT ANDROID AND NOT IOS)
    option(BUILD_TOOLS "Build tools" ON)
    option(ENABLE_HOT_LOADING "Enable hot loading from disk" ON)
else()
    option(BUILD_TOOLS "Build tools" OFF)
    option(ENABLE_HOT_LOADING "Enable hot loading from disk" OFF)
endif()
option(BUILD_TESTS "Build test programs" OFF)
option(BUILD_EXAMPLES "Build Examples" OFF)
if (APPLE)
    option(USE_METAL "Force Use Metal Graphics API for macOS and iOS" OFF)
endif()

# Dependencies #

# Run find package scripts
if (USE_SDL2)
    include(FindSDL2)
endif()
if (USE_CURL)
    include(FindCurl)
endif()

include(FindBgfx)

if (BUILD_TOOLS)
    include(FindAssimp)
endif()

# standard windows dependencies
if (WIN32)
    if (BUILD_TOOLS AND NOT ASSIMP_FOUND)
        download_package(${DOWNLOAD_URL_ROOT}/assimp_vc140.tar.gz ${DEP_ROOT_DIR})
        include(FindAssimp)
    endif()
    if (USE_SDL2 AND (NOT SDL2_FOUND))
        download_package(${DOWNLOAD_URL_ROOT}/SDL2_vc140.tar.gz ${DEP_ROOT_DIR})
        include (FindSDL2)
    endif()
endif()

# Standard android dependencies
if (ANDROID)
    if (USE_SDL2 AND (NOT SDL2_FOUND))
        download_package(${DOWNLOAD_URL_ROOT}/SDL2_android.tar.gz ${DEP_ROOT_DIR})
        include (FindSDL2)
    endif()
endif()

# Download dependencies that doesn't have packages on any platform
if (NOT BGFX_FOUND)
    if (ANDROID)
        download_package(${DOWNLOAD_URL_ROOT}/bgfx_android.tar.gz ${DEP_ROOT_DIR})
    elseif (IOS)
        download_package(${DOWNLOAD_URL_ROOT}/bgfx_ios_arm.tar.gz ${DEP_ROOT_DIR})
    elseif (MSVC)
        download_package(${DOWNLOAD_URL_ROOT}/bgfx_vc140.tar.gz ${DEP_ROOT_DIR})
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        download_package(${DOWNLOAD_URL_ROOT}/bgfx_linux64.tar.gz ${DEP_ROOT_DIR})
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        download_package(${DOWNLOAD_URL_ROOT}/bgfx_osx64.tar.gz ${DEP_ROOT_DIR})
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

    if (USE_SDL2_MIXER)
        if (SDL2_MIXER_FOUND)
            if (WIN32)
                sdl2_mixer_copy_binaries(${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
                message(STATUS "Found SDL2_mixer library")   
            else()
                message(STATUS "Found SDL2_mixer library: ${SDL2_MIXER_LIBRARY}")
            endif()                
        else()
            message(FATAL_ERROR "SDL2_mixer library not found")
        endif()
    endif()
endif()

if (USE_CURL)
    if (CURL_FOUND)
        if (WIN32)
            message(STATUS "Found Curl library")
        else()
            message(STATUS "Found Curl Library: ${CURL_LIBRARY}")
        endif()
    else()
        message(FATAL_ERROR "Curl library not found")
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
add_definitions(-D__STDC_FORMAT_MACROS)
if(MSVC)
    add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
    add_definitions(-D_HAS_EXCEPTIONS=0)
    add_definitions(-D_HAS_ITERATOR_DEBUGGING=0)
    add_definitions(-D_SCL_SECURE=0)
    add_definitions(-D_SECURE_SCL=0)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)

    remove_cxx_flags("/EHsc /GR")
    add_cxx_flags("/GR-")
else()
    add_compile_options(-fPIC)
    add_cxx_flags("-std=c++11 -Wno-switch -fno-rtti -fno-exceptions")
endif()

# Strip duplicates from CXX flags
remove_duplicates(${CMAKE_CXX_FLAGS} CMAKE_CXX_FLAGS)
if (CMAKE_C_FLAGS)
    remove_duplicates(${CMAKE_C_FLAGS} CMAKE_C_FLAGS)
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

if (USE_CURL)
    add_definitions(-Dtermite_CURL)
endif()

if (USE_SDL2_MIXER AND NOT USE_SDL2)
    message(FATAL_ERROR "To use SDL2_mixer you must set USE_SDL2 option too")
elseif (USE_SDL2_MIXER)
    add_definitions(-Dtermite_SDL2_MIXER)
endif()

# set sub-path for reference shader inside the code
if (NOT IOS)
    set(SHADER_OUTPUT_DIR_EXT ${CMAKE_SYSTEM_NAME})
else()
    set(SHADER_OUTPUT_DIR_EXT "ios")
endif()
add_definitions(-Dtermite_SHADER_APPEND_PATH=${SHADER_OUTPUT_DIR_EXT})

# copy release flags into profile
copy_release_build_flags(PROFILE)
copy_release_build_flags(DEVELOPMENT)

if (ENABLE_HOT_LOADING) 
    set(DENABLE_HOT_LOADING "-DUSE_EFSW=1")
else()
    set(DENABLE_HOT_LOADING "-DUSE_EFSW=0")
endif()

# http://stackoverflow.com/questions/33829152/for-cmake-can-you-modify-the-release-debug-compiler-flags-with-add-compiler-fl
set(DEBUG_COMPILE_OPTIONS "-DBX_CONFIG_ALLOCATOR_DEBUG" "-Dtermite_DEV=1" "-DBX_CONFIG_CHECK_ASSERT=1" "-DRMT_ENABLED=1")
add_compile_options("$<$<CONFIG:Debug>:${DEBUG_COMPILE_OPTIONS}>" ${DENABLE_HOT_LOADING})

set(DEVELOPMENT_COMPILE_OPTIONS "-Dtermite_DEV=1" "-DRMT_ENABLED=1" ${DENABLE_HOT_LOADING})
add_compile_options("$<$<CONFIG:Development>:${DEVELOPMENT_COMPILE_OPTIONS}>")

set(PROFILE_COMPILE_OPTIONS "-Dtermite_DEV=0" "-DRMT_ENABLED=1" "-DUSE_EFSW=0")
add_compile_options("$<$<CONFIG:Profile>:${PROFILE_COMPILE_OPTIONS}>")

set(RELEASE_COMPILE_OPTIONS "-Dtermite_DEV=0" "-DRMT_ENABLED=0" "-DUSE_EFSW=0")
add_compile_options("$<$<CONFIG:Release>:${RELEASE_COMPILE_OPTIONS}>")

# Common include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# dependencies
add_subdirectory(deps/deboost.context)
add_subdirectory(deps/bx)
add_subdirectory(deps/rapidjson)
add_subdirectory(deps/nvg)
add_subdirectory(deps/remotery)
add_subdirectory(deps/AHEasing)
add_subdirectory(deps/utf8)
add_subdirectory(deps/tiny-AES128-C)
add_subdirectory(deps/lz4)
add_subdirectory(deps/miniz)
add_subdirectory(deps/etcpack)
add_subdirectory(deps/efsw)
add_subdirectory(deps/b64)
if (NOT IOS)
    set_target_properties(fcontext PROPERTIES FOLDER Deps)
endif()
if (USE_CURL)
    add_subdirectory(deps/restclient-cpp)
endif()

# common
add_subdirectory(source/imgui_impl)

## main
add_subdirectory(source/termite)

# plugins
add_subdirectory(source/driver_bgfx)
add_subdirectory(source/driver_disk_lite)

if (USE_BOX2D)
    add_subdirectory(deps/Box2D)
    set_target_properties(Box2D PROPERTIES FOLDER Deps)

    add_subdirectory(source/driver_box2d)
endif()

if (USE_SDL2_MIXER)
    add_subdirectory(source/driver_sdl_mixer)
endif()

# tools
if (BUILD_TOOLS)
    add_subdirectory(source/ls-model)
    add_subdirectory(source/modelc)
    add_subdirectory(source/animc)
    add_subdirectory(source/encrypt)
    add_subdirectory(source/texpack)
endif()

# tests
if (BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Add Games Here
if (BUILD_FISHERBOY)
    add_subdirectory(fisherboy)
endif()

if (BUILD_MODELVIEWER)
    add_subdirectory(model_viewer)
endif()
