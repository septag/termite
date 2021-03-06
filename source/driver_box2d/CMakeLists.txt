# PROJECT: box2d_driver
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # ignore some compiler specific warnings
    add_compile_options(-Wno-enum-compare)
endif()

# PRIVATE_LIBS
if (CMAKE_COMPILER_IS_GNUCC)
    if (${CMAKE_CXX_COMPILER_VERSION} LESS "6.0.0" AND ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(GCC_LIBS gcc gcc_s)        
    endif()
endif()

add_library(box2d_driver ${BUILD_LIBRARY_TYPE} ${SOURCE_FILES})

target_link_libraries(box2d_driver PRIVATE Box2D bx nanovg ${GCC_LIBS})
set_target_properties(box2d_driver PROPERTIES FOLDER Plugins ${IOS_GENERAL_PROPERTIES})


