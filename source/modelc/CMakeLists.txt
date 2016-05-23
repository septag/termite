# PROJECT: modelc
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*" "../tools_common/*.c*")
source_group(source FILES ${SOURCE_FILES})

set(INCLUDE_FILES ../include_common/t3d_format.h)
source_group(common FILES ${INCLUDE_FILES})

add_executable(modelc ${SOURCE_FILES} ${INCLUDE_FILES})
target_link_libraries(modelc ${ASSIMP_LIBRARY} bx)
target_include_directories(modelc PRIVATE ${ASSIMP_INCLUDE_DIR})
add_dependencies(modelc AssimpCopyBinaries)

install(TARGETS modelc RUNTIME DESTINATION bin)
