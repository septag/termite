# CURL_FOUND
# CURL_LIBRARY:
# OPENSSL_LIBRARY
# CURL_INCLUDE_DIR
# function: assimp_copy_binaries (win32)

if (ANDROID)    
    set(CURL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/curl_android CACHE PATH "Curl root directory")
    set(FIND_EXTRA_FLAG NO_CMAKE_FIND_ROOT_PATH)
elseif (IOS)
    set(CURL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/curl_ios CACHE PATH "Curl root directory")
    set(FIND_EXTRA_FLAG NO_CMAKE_FIND_ROOT_PATH NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
else()
    set(CURL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/curl CACHE PATH "Curl root directory")
endif()

# Find path of each library
find_path(CURL_INCLUDE_DIR
    NAMES
        curl/curl.h
    HINTS
        ${CURL_ROOT_DIR}/include
    ${FIND_EXTRA_FLAG}
)

if (ANDROID)
    set(CURL_LIB_DIR_SUB "/${ANDROID_ABI}")
endif()

find_library(CURL_LIBRARY curl PATHS ${CURL_ROOT_DIR}/lib${CURL_LIB_DIR_SUB} ${FIND_EXTRA_FLAG})

if (CURL_LIBRARY AND CURL_INCLUDE_DIR)
    set(CURL_FOUND TRUE)
endif()
