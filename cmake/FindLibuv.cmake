# Sets:
# LIBUV_FOUND
# LIBUV_LIBRARY
# LIBUV_INCLUDE_DIR
# function: libuv_copy_binaries (win32)

if (ANDROID)
    set(LIBUV_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/libuv_android_arm CACHE PATH "Libuv root directory")
    set(LIBUV_LIBRARY ${LIBUV_ROOT_DIR}/lib/libuv.a)
    set(LIBUV_INCLUDE_DIR ${LIBUV_ROOT_DIR}/include)
    set(LIBUV_FOUND TRUE)
    return()
endif()

if (WIN32)
    set(LIBUV_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/libuv CACHE PATH "Libuv root directory")

    # Find path of each library
    find_path(LIBUV_INCLUDE_DIR
        NAMES uv.h
        HINTS ${LIBUV_ROOT_DIR}/include)

    find_path(LIBUV_LIBRARY_DIR
        NAMES uv.lib libuv.lib libuv.so
        HINTS ${LIBUV_ROOT_DIR}/lib)

    find_library(LIBUV_LIBRARY_RELEASE uv PATHS ${LIBUV_LIBRARY_DIR})

    set(LIBUV_LIBRARY ${LIBUV_LIBRARY_RELEASE})

    if (LIBUV_LIBRARY_RELEASE AND LIBUV_INCLUDE_DIR)
        set(LIBUV_FOUND TRUE)
    endif()

    if(MSVC)
        function(libuv_copy_binaries TargetDirectory)
            add_custom_target(LibuvCopyBinaries
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIBUV_ROOT_DIR}/bin/uv.dll ${TargetDirectory}/uv.dll
            COMMENT "Copying Libuv binaries to '${TargetDirectory}'"
            VERBATIM)
        endfunction()   
    endif() 
else ()
    find_path(
      LIBUV_INCLUDE_DIR
      NAMES uv.h
      PATHS /usr/local/include /usr/include)

    find_library(
      LIBUV_LIBRARY
      NAMES uv
      PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu)

    if (LIBUV_INCLUDE_DIR AND LIBUV_LIBRARY)
      set(LIBUV_FOUND TRUE)
    endif ()
endif()


