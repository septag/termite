# PROJECT: modelc
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*" "../tools_common/*.c*")
source_group(source FILES ${SOURCE_FILES})

set(INCLUDE_FILES ../include_common/tanim_format.h)
source_group(common FILES ${INCLUDE_FILES})

add_executable(animc ${SOURCE_FILES} ${INCLUDE_FILES})
target_link_libraries(animc ${ASSIMP_LIBRARY} bx)
target_include_directories(animc PRIVATE ${ASSIMP_INCLUDE_DIR})

if (WIN32)
    add_dependencies(animc AssimpCopyBinaries)
endif()

set_target_properties(animc PROPERTIES FOLDER Tools)
install(TARGETS animc RUNTIME DESTINATION bin)
