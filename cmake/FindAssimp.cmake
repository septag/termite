# ASSIMP_FOUND
# ASSIMP_LIBRARY: (debug, optimized) for windows
# ASSIMP_INCLUDE_DIR
# function: assimp_copy_binaries (win32)

if(WIN32)
	set(ASSIMP_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/assimp CACHE PATH "ASSIMP root directory")

	# Find path of each library
	find_path(ASSIMP_INCLUDE_DIR
		NAMES
			assimp/scene.h assimp/Importer.hpp
		HINTS
			${ASSIMP_ROOT_DIR}/include
	)

	if(MSVC12)
		set(ASSIMP_MSVC_VERSION "vc120")
	elseif(MSVC14)	
		set(ASSIMP_MSVC_VERSION "vc140")
	endif(MSVC12)

	if(MSVC12 OR MSVC14)
	
		find_path(ASSIMP_LIBRARY_DIR
			NAMES
				assimp-${ASSIMP_MSVC_VERSION}-mt.lib
			HINTS
				${ASSIMP_ROOT_DIR}/lib
		)

		find_library(ASSIMP_LIBRARY_RELEASE				assimp-${ASSIMP_MSVC_VERSION}-mt.lib 			PATHS ${ASSIMP_LIBRARY_DIR})
		find_library(ASSIMP_LIBRARY_DEBUG				assimp-${ASSIMP_MSVC_VERSION}-mtd.lib			PATHS ${ASSIMP_LIBRARY_DIR})
		
		set(ASSIMP_LIBRARY 
			optimized 	${ASSIMP_LIBRARY_RELEASE}
			debug		${ASSIMP_LIBRARY_DEBUG}
		)

		if (ASSIMP_LIBRARY_RELEASE AND ASSIMP_LIBRARY_DEBUG AND ASSIMP_INCLUDE_DIR)
			set(ASSIMP_FOUND TRUE)
		endif()
		
		function(assimp_copy_binaries TargetDirectory)
			add_custom_target(AssimpCopyBinaries
				COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASSIMP_ROOT_DIR}/bin/assimp-${ASSIMP_MSVC_VERSION}-mtd.dll 	${TargetDirectory}/assimp-${ASSIMP_MSVC_VERSION}-mtd.dll
				COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASSIMP_ROOT_DIR}/bin/assimp-${ASSIMP_MSVC_VERSION}-mt.dll 	${TargetDirectory}/assimp-${ASSIMP_MSVC_VERSION}-mt.dll
			COMMENT "Copying Assimp binaries to '${TargetDirectory}'"
			VERBATIM)
		endfunction()	
	endif()	
else(WIN32)
	find_path(
	  ASSIMP_INCLUDE_DIR
	  NAMES assimp/scene.h assimp/Importer.hpp
	  HINTS /usr/local/include/ /usr/include)
    
	find_library(
	  ASSIMP_LIBRARY
	  NAMES assimp
	  PATHS /usr/local/lib/ /usr/lib/x86_64-linux-gnu /usr/lib
	)

	if (ASSIMP_INCLUDE_DIR AND ASSIMP_LIBRARY)
	  set(ASSIMP_FOUND TRUE)
	endif ()
endif()
