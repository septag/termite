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

# build compile flags
function(copy_release_build_flags BUILD_TYPE)
    set(CMAKE_C_FLAGS_${BUILD_TYPE} ${CMAKE_C_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_${BUILD_TYPE} ${CMAKE_CXX_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_SHARED_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_MODULE_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_MODULE_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_EXE_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
    set(CMAKE_STATIC_LINKER_FLAGS_${BUILD_TYPE} ${CMAKE_STATIC_LINKER_FLAGS_RELEASE} PARENT_SCOPE)
endfunction()

##############################################
# remove CXX flags
function(remove_cxx_flags FLAGS)
    separate_arguments(FLAGS)
    foreach(FLAG ${FLAGS})
        string(REPLACE "${FLAG}" "" TEMP_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        set(CMAKE_CXX_FLAGS ${TEMP_CXX_FLAGS} PARENT_SCOPE)

        if (CMAKE_CXX_FLAGS_DEBUG)
            string(REPLACE "${FLAG}" "" TEMP_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
            set(CMAKE_CXX_FLAGS_DEBUG ${TEMP_CXX_FLAGS_DEBUG} PARENT_SCOPE)
        endif()

        if (CMAKE_CXX_FLAGS_DEVELOPMENT)
            string(REPLACE "${FLAG}" "" TEMP_CXX_FLAGS_DEVELOPMENT "${CMAKE_CXX_FLAGS_DEVELOPMENT}")
            set(CMAKE_CXX_FLAGS_DEBUG ${TEMP_CXX_FLAGS_DEVELOPMENT} PARENT_SCOPE)
        endif()

        if (CMAKE_CXX_FLAGS_PROFILE)
            string(REPLACE "${FLAG}" "" TEMP_CXX_FLAGS_PROFILE "${CMAKE_CXX_FLAGS_PROFILE}")
            set(CMAKE_CXX_FLAGS_DEBUG ${TEMP_CXX_FLAGS_PROFILE} PARENT_SCOPE)
        endif()

        if (CMAKE_CXX_FLAGS_RELEASE)
            string(REPLACE "${FLAG}" "" TEMP_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
            set(CMAKE_CXX_FLAGS_DEBUG ${TEMP_CXX_FLAGS_RELEASE} PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

function(add_cxx_flags FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAGS}" PARENT_SCOPE)
endfunction()

function(remove_duplicates COMPILE_FLAGS RESULT)
    set(TEMP_FLAGS ${COMPILE_FLAGS})
    separate_arguments(TEMP_FLAGS)  
    list(REMOVE_DUPLICATES TEMP_FLAGS)
    string(REPLACE ";" " " COMPILE_FLAGS_STRIPPED "${TEMP_FLAGS}") 
    set(${RESULT} ${COMPILE_FLAGS_STRIPPED} PARENT_SCOPE)
endfunction()