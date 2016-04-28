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

# load modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PrecompiledHeader)
include(Utility)
include(DownloadPackage)
include(Bgfx)
include(FindSDL2)
include(FindLibuv)

if (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Profile;Development" CACHE STRING "" FORCE)
endif()

set(DOWNLOAD_URL_ROOT "144.76.213.164/packages")
set(DEP_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

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
    add_compile_options(-std=c++11 -fPIC -fno-rtti -fno-exceptions)
endif()

add_definitions(-Dtermite_SHARED_LIB)

# copy release flags into profile
copy_build_flags(PROFILE)
copy_build_flags(DEVELOPMENT)

# http://stackoverflow.com/questions/33829152/for-cmake-can-you-modify-the-release-debug-compiler-flags-with-add-compiler-fl
set(DEBUG_COMPILE_OPTIONS "-DBX_CONFIG_ALLOCATOR_DEBUG")
add_compile_options("$<$<CONFIG:Debug>:${DEBUG_COMPILE_OPTIONS}>")

########################################################
# Download dependencies
message("Downloading external dependencies ...")

# libuv
if (NOT LIBUV_FOUND)
    download_package(${DOWNLOAD_URL_ROOT}/libuv.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps DOWNLOAD_RESULT)
    get_dep_dirs("libuv" LIBUV_LIB_DIR LIBUV_INCLUDE_DIR)
    find_library(LIBUV_LIBS_DEBUG NAMES "uv_d" PATHS ${LIBUV_LIB_DIR})
    find_library(LIBUV_LIBS NAMES "uv" PATHS ${LIBUV_LIB_DIR})
    if (NOT LIBUV_LIBS)
        message(FATAL_ERROR "libuv library not found")
    endif()
    set(LIBUV_LIBRARIES
            optimized ${LIBUV_LIBS}
            debug ${LIBUV_LIBS_DEBUG})
endif()

# Bgfx 
download_package(${DOWNLOAD_URL_ROOT}/bgfx.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps DOWNLOAD_RESULT)
get_dep_dirs("bgfx" BGFX_LIB_DIR BGFX_INCLUDE_DIRS)
find_library(BGFX_LIBS_DEBUG NAMES "bgfxDebug" PATHS ${BGFX_LIB_DIR})
find_library(BGFX_LIBS NAMES "bgfxRelease" PATHS ${BGFX_LIB_DIR})
if (NOT BGFX_LIBS)
    message(FATAL_ERROR "Bgfx library not found")
endif()
set(BGFX_LIBRARIES
        optimized ${BGFX_LIBS}
        debug ${BGFX_LIBS_DEBUG})

# Assimp
download_package(${DOWNLOAD_URL_ROOT}/assimp.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps DOWNLOAD_RESULT)
get_dep_dirs("assimp" ASSIMP_LIB_DIR ASSIMP_INCLUDE_DIRS)
if (MSVC)
    set(ASSIMP_NAME_SUFFIX "-vc140-mt")
else()
    set(ASSIMP_NAME_SUFFIX)
endif()

find_library(ASSIMP_LIBS_DEBUG NAMES "assimp${ASSIMP_NAME_SUFFIX}d" PATHS ${ASSIMP_LIB_DIR})
find_library(ASSIMP_LIBS NAMES "assimp${ASSIMP_NAME_SUFFIX}" PATHS ${ASSIMP_LIB_DIR})
if (NOT ASSIMP_LIBS)
    message(FATAL_ERROR "Assimp library not found")
endif()

# SDL2
unset(SDL2_BIN_DIR CACHE)
unset(SDL2_LIBRARY_TEMP CACHE)
unset(SDL2MAIN_LIBRARY CACHE)
if (NOT SDL2_FOUND)
    download_package(${DOWNLOAD_URL_ROOT}/sdl.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/deps DOWNLOAD_RESULT)
    get_dep_dirs("sdl" SDL2_LIB_DIR SDL2_INCLUDE_DIRS)
    find_library(SDL2_LIBRARY NAMES "SDL2" PATHS ${SDL2_LIB_DIR})
    if (NOT SDL2_LIBRARY)
        message(FATAL_ERROR "SDL2 library not found")
    endif()
    set(SDL2_INCLUDE_DIR ${SDL2_INCLUDE_DIRS} CACHE PATH "Include directory for SDL2 lib" FORCE)
endif()

# copy dependency binaries to output directory
set(DEP_FILES ${LIBUV_LIBRARIES} ${ASSIMP_LIBS_DEBUG} ${ASSIMP_LIBS} ${SDL2_LIBRARY} ${BGFX_LIBRARIES})
foreach(DEP_FILE ${DEP_FILES})
    if(WIN32)
        get_filename_component(DEP_FILE_DIR ${DEP_FILE} DIRECTORY)
        get_filename_component(DEP_FILE_NAME ${DEP_FILE} NAME_WE)
        set(DEP_FILE_BIN ${DEP_FILE_DIR}/${DEP_FILE_NAME}.dll)
    else()
        set(DEP_FILE_BIN ${DEP_FILE})
    endif()

    if(EXISTS ${DEP_FILE_BIN})
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DEP_FILE_BIN} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endif()
endforeach()


# Common include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
set(BX_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/deps/bx/include)
if (MSVC)
    set(BX_INCLUDE_DIRS ${BX_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/deps/bx/include/compat/msvc)
elseif(APPLE)
    set(BX_INCLUDE_DIRS ${BX_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/deps/bx/include/compat/osx)
elseif(UNIX)
    set(BX_INCLUDE_DIRS ${BX_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/deps/bx/include/compat/posix)
endif()
include_directories(${BX_INCLUDE_DIRS})

# Sub projects
# dependencies
add_subdirectory(deps/deboost.context)
set_target_properties(fcontext PROPERTIES FOLDER Deps)

# common
add_subdirectory(source/imgui_impl)
set_target_properties(imgui_impl PROPERTIES FOLDER Common)

# main
add_subdirectory(source/termite)

# plugins
add_subdirectory(source/driver_bgfx)
add_subdirectory(source/driver_disk)
add_subdirectory(source/render_default)
set_target_properties(tdriver_bgfx PROPERTIES FOLDER Plugins)
set_target_properties(tdriver_disk PROPERTIES FOLDER Plugins)
set_target_properties(trender_default PROPERTIES FOLDER Plugins)

# tools
add_subdirectory(source/ls-model)
add_subdirectory(source/modelc)
set_target_properties(ls-model PROPERTIES FOLDER Tools)
set_target_properties(modelc PROPERTIES FOLDER Tools)

# tests
add_subdirectory(tests)
set_target_properties(test_init PROPERTIES FOLDER Tests)
set_target_properties(test_sdl PROPERTIES FOLDER Tests)

