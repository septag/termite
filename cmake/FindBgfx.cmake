# BGFX_FOUND
# BGFX_LIBRARY: (debug, optimized) for windows
# BGFX_INCLUDE_DIR
# function: assimp_copy_binaries (win32)

set(BGFX_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/bgfx CACHE PATH "BGFX root directory")

# Find path of each library
find_path(BGFX_INCLUDE_DIR
	NAMES
		bgfx/bgfx.h
	HINTS
		${BGFX_ROOT_DIR}/include
)

find_path(BGFX_LIBRARY_DIR
	NAMES
		bgfxRelease.lib
	HINTS
		${BGFX_ROOT_DIR}/lib
)

find_library(BGFX_LIBRARY_RELEASE bgfxRelease.lib PATHS ${BGFX_LIBRARY_DIR})
find_library(BGFX_LIBRARY_DEBUG	bgfxDebug.lib PATHS ${BGFX_LIBRARY_DIR})

set(BGFX_LIBRARY 
	optimized 	${BGFX_LIBRARY_RELEASE}
	debug		${BGFX_LIBRARY_DEBUG}
)

if (BGFX_LIBRARY_RELEASE AND BGFX_LIBRARY_DEBUG AND BGFX_INCLUDE_DIR)
	set(BGFX_FOUND TRUE)
endif()
