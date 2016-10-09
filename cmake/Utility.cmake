########################################################
# Get dependency dirs
function(get_cpu_arch RESULT)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(${RESULT} "x64" PARENT_SCOPE)
    else()
        set(${RESULT} "x86" PARENT_SCOPE)
    endif()
endfunction()

function(get_dep_dirs LIBNAME RESULT_LIB RESULT_INCLUDE)
    get_cpu_arch(SUFFIX)
    set(${RESULT_INCLUDE} ${DEP_ROOT_DIR}/${LIBNAME}/include PARENT_SCOPE)
    set(${RESULT_LIB} ${DEP_ROOT_DIR}/${LIBNAME}/lib/${SUFFIX} PARENT_SCOPE)
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

macro(remove_c_flag flag)
    string(REPLACE "${flag}" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endmacro()

# build compile flags
function(copy_build_flags BUILD_TYPE)
    set(CMAKE_C_FLAGS_${BUILD_TYPE} ${CMAKE_C_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_${BUILD_TYPE} ${CMAKE_CXX_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_SHARED_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_MODULE_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_MODULE_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_EXE_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_STATIC_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_STATIC_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
endfunction()