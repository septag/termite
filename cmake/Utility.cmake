########################################################
# Get dependency dirs
function(get_dep_dirs LIBNAME RESULT_LIB RESULT_INCLUDE)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPU_ARCH "x64")
    else()
        set(CPU_ARCH "x86")
    endif()
    
    set(${RESULT_INCLUDE} ${DEP_ROOT_DIR}/${LIBNAME}/include PARENT_SCOPE)
    set(${RESULT_LIB} ${DEP_ROOT_DIR}/${LIBNAME}/lib/${CPU_ARCH} PARENT_SCOPE)
endfunction()

function(join_array VALUES GLUE OUTPUT)
  string (REGEX REPLACE "([^\\]|^);" "\\1${GLUE}" _TMP_STR "${VALUES}")
  string (REGEX REPLACE "[\\](.)" "\\1" _TMP_STR "${_TMP_STR}") #fixes escaping
  set (${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()

##############################################
# remove CXX flags
macro(remove_cxx_flag flag)
    string(REPLACE "${flag}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endmacro()

# build compile flags
function(copy_build_flags BUILD_TYPE)
    set(CMAKE_CXX_FLAGS_${BUILD_TYPE} ${CMAKE_CXX_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_SHARED_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_${BUILD_TYPE} ${CMAKE_SHARED_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_SHARED_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_${BUILD_TYPE} ${CMAKE_CXX_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_CXX_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_MODULE_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_CXX_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_CXX_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_STATIC_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_CXX_FLAGS_RELEASE} PARENT_SCOPE)
endfunction()