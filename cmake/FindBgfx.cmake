# BGFX_FOUND
# BGFX_LIBRARY: (debug, optimized) for windows
# BGFX_INCLUDE_DIR
# function: assimp_copy_binaries (win32)

if (ANDROID)    
    set(BGFX_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/bgfx_android CACHE PATH "BGFX root directory")
    set(FIND_EXTRA_FLAG NO_CMAKE_FIND_ROOT_PATH)
elseif (IOS)
    set(BGFX_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/bgfx_ios_arm CACHE PATH "Bgfx root directory")
    set(FIND_EXTRA_FLAG NO_CMAKE_FIND_ROOT_PATH)
else()
    set(BGFX_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/bgfx CACHE PATH "BGFX root directory")
endif()

# Find path of each library
find_path(BGFX_INCLUDE_DIR
	NAMES
		bgfx/bgfx.h
	HINTS
		${BGFX_ROOT_DIR}/include
    ${FIND_EXTRA_FLAG}
)

if (ANDROID)
    if (ANDROID_ABI MATCHES "armeabi-v7a")
        set(BGFX_LIB_DIR_SUB "/armeabi-v7a")
    elseif (ANDROID_ABI MATCHES "armeabi")
        set(BGFX_LIB_DIR_SUB "/armeabi")
    elseif (ANDROID_ABI MATCHES "x86")
        set(BGFX_LIB_DIR_SUB "/x86")
    endif()
endif()

find_library(BGFX_LIBRARY_RELEASE bgfxRelease PATHS ${BGFX_ROOT_DIR}/lib${BGFX_LIB_DIR_SUB} ${FIND_EXTRA_FLAG})
find_library(BGFX_LIBRARY_DEBUG	bgfxDebug PATHS ${BGFX_ROOT_DIR}/lib${BGFX_LIB_DIR_SUB} ${FIND_EXTRA_FLAG})

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	find_library(BX_LIBRARY_RELEASE bxRelease PATHS ${BGFX_ROOT_DIR}/lib${BGFX_LIB_DIR_SUB} ${FIND_EXTRA_FLAG})
	find_library(BX_LIBRARY_DEBUG bxDebug PATHS ${BGFX_ROOT_DIR}/lib${BGFX_LIB_DIR_SUB} ${FIND_EXTRA_FLAG})
endif()

set(BGFX_LIBRARY 
	optimized 	${BGFX_LIBRARY_RELEASE} ${BX_LIBRARY_RELEASE}
	debug		${BGFX_LIBRARY_DEBUG} ${BX_LIBRARY_DEBUG}
)

if (BGFX_LIBRARY_RELEASE AND BGFX_LIBRARY_DEBUG AND BGFX_INCLUDE_DIR)
	set(BGFX_FOUND TRUE)
endif()
