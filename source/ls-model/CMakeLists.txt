# PROJECT: ls-model
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*" "../tools_common/*.c*")
source_group(source FILES ${SOURCE_FILES})

add_executable(ls-model ${SOURCE_FILES})

target_link_libraries(ls-model ${ASSIMP_LIBRARY} bx)
target_include_directories(ls-model PRIVATE ${ASSIMP_INCLUDE_DIR})

if (WIN32)
    add_dependencies(ls-model AssimpCopyBinaries)
endif()

install(TARGETS ls-model RUNTIME DESTINATION bin)
