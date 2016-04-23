# PROJECT: termite
cmake_minimum_required(VERSION 3.0)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*" "*.h")
file(GLOB INCLUDE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "../../include/termite/*.h")
source_group(source FILES ${SOURCE_FILES})
source_group(include FILES ${INCLUDE_FILES})

add_library(termite SHARED ${SOURCE_FILES} ${INCLUDE_FILES})

get_filename_component(FCONTEXT_INCLUDE_DIR ../../deps/deboost.context/include ABSOLUTE)
include_directories(${FCONTEXT_INCLUDE_DIR})

target_link_libraries(termite fcontext)

get_filename_component(TERMITE_INCLUDE_DIR ../../include/termite ABSOLUTE)
include_directories(${TERMITE_INCLUDE_DIR})

add_dependencies(termite fcontext)

# PCH
add_precompiled_header(termite pch.h)

install(TARGETS termite
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
install(FILES ${INCLUDE_FILES} DESTINATION include/termite)

# install bx headers too
#foreach(BX_INCLUDE_DIR ${BX_INCLUDE_DIRS})
#    message(${BX_INCLUDE_DIR})
#    file(GLOB_RECURSE BX_HEADER_FILES RELATIVE ${BX_INCLUDE_DIR} "*.h")
#    install(FILES ${BX_HEADER_FILES} DESTINATION include/bx)
#    message(${BX_HEADER_FILES})
#endforeach()

