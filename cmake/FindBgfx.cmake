# BGFX_FOUND
# BGFX_LIBRARY: (debug, optimized) for windows
# BGFX_INCLUDE_DIR
# function: assimp_copy_binaries (win32)

if (ANDROID)    
    set(BGFX_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/bgfx_android_arm CACHE PATH "BGFX root directory")
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

find_library(BGFX_LIBRARY_RELEASE bgfxRelease PATHS ${BGFX_ROOT_DIR}/lib ${FIND_EXTRA_FLAG})
find_library(BGFX_LIBRARY_DEBUG	bgfxDebug PATHS ${BGFX_ROOT_DIR}/lib ${FIND_EXTRA_FLAG})

set(BGFX_LIBRARY 
	optimized 	${BGFX_LIBRARY_RELEASE}
	debug		${BGFX_LIBRARY_DEBUG}
)

if (BGFX_LIBRARY_RELEASE AND BGFX_LIBRARY_DEBUG AND BGFX_INCLUDE_DIR)
	set(BGFX_FOUND TRUE)
endif()
