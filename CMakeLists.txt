cmake_minimum_required(VERSION 3.3)
project(kashmir)

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

########################################################
# Check dependencies

# Run find package scripts
include(FindSDL2)
include(FindAssimp)
include(FindLibuv)
include(FindBgfx)

# windows dependencies
if (WIN32)
    # Assimp
    if (NOT ASSIMP_FOUND)
        download_package(${DOWNLOAD_URL_ROOT}/assimp_vc140.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
        include(FindAssimp)
    endif()

    # libuv
    if (NOT LIBUV_FOUND)
        download_package(${DOWNLOAD_URL_ROOT}/libuv_vc140.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
        include(FindLibuv)
    endif()

    # SDL2
    if (NOT SDL2_FOUND)
        download_package(${DOWNLOAD_URL_ROOT}/sdl_vc140.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
        include (FindSDL2)
    endif()
endif()

# Download dependencies that doesn't have packages on any platform
if (NOT BGFX_FOUND)
    download_package(${DOWNLOAD_URL_ROOT}/bgfx_vc140.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps)
    include(FindBgfx)
endif()

if (ASSIMP_FOUND)
    if (WIN32)
        assimp_copy_binaries(${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        message(STATUS "Found Assimp library")    
    else()
        message(STATUS "Found Assimp library: ${ASSIMP_LIBRARY}")
    endif()
else()
    message(FATAL_ERROR "Assimp library not found")
endif()

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

if (BGFX_FOUND)
    if (WIN32)
        message(STATUS "Found Bgfx library")
    else()
        message(STATUS "Found Bgfx Library: ${BGFX_LIBRARY}")
    endif()
else()
    message(FATAL_ERROR "Bgfx library not found")
endif()

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
    add_compile_options(-std=c++11 -fPIC -fno-rtti -fno-exceptions -Wno-switch)
endif()

add_definitions(-Dtermite_SHARED_LIB)

# copy release flags into profile
copy_build_flags(PROFILE)
copy_build_flags(DEVELOPMENT)

# http://stackoverflow.com/questions/33829152/for-cmake-can-you-modify-the-release-debug-compiler-flags-with-add-compiler-fl
set(DEBUG_COMPILE_OPTIONS "-DBX_CONFIG_ALLOCATOR_DEBUG")
add_compile_options("$<$<CONFIG:Debug>:${DEBUG_COMPILE_OPTIONS}>")

# Common include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Sub projects
# dependencies
add_subdirectory(deps/deboost.context)
add_subdirectory(deps/bx)
add_subdirectory(deps/Box2D)
set_target_properties(fcontext PROPERTIES FOLDER Deps)
set_target_properties(Box2D PROPERTIES FOLDER Deps)

# common
add_subdirectory(source/imgui_impl)
set_target_properties(imgui_impl PROPERTIES FOLDER Common)
set_target_properties(imgui PROPERTIES FOLDER Deps)

# main
add_subdirectory(source/termite)

# plugins
add_subdirectory(source/driver_bgfx)
add_subdirectory(source/driver_disk)
add_subdirectory(source/render_stock)
set_target_properties(tdriver_bgfx PROPERTIES FOLDER Plugins)
set_target_properties(tdriver_disk PROPERTIES FOLDER Plugins)
set_target_properties(trender_stock PROPERTIES FOLDER Plugins)

# tools
add_subdirectory(source/ls-model)
add_subdirectory(source/modelc)
add_subdirectory(source/animc)
set_target_properties(ls-model PROPERTIES FOLDER Tools)
set_target_properties(modelc PROPERTIES FOLDER Tools)
set_target_properties(animc PROPERTIES FOLDER Tools)

# tests
add_subdirectory(tests)
set_target_properties(test_init PROPERTIES FOLDER Tests)
set_target_properties(test_sdl PROPERTIES FOLDER Tests)
set_target_properties(test_gfx PROPERTIES FOLDER Tests)

# copy binaries
if (WIN32)
    set_target_properties(AssimpCopyBinaries PROPERTIES FOLDER Deps/CopyBinaries) 
    set_target_properties(LibuvCopyBinaries PROPERTIES FOLDER Deps/CopyBinaries) 
    set_target_properties(SDL2CopyBinaries PROPERTIES FOLDER Deps/CopyBinaries) 
endif()

# kashmir
add_subdirectory(source/kashmir)
