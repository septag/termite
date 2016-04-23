# PROJECT: ls-model
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*" "../tools_common/*.c*")
source_group(source FILES ${SOURCE_FILES})

include_directories(${DEP_ROOT_DIR})
include_directories(${ASSIMP_INCLUDE_DIRS})

add_executable(ls-model ${SOURCE_FILES})

set(ASSIMP_LIBFILE "$<$<CONFIG:Debug>:${ASSIMP_LIBS_DEBUG}>"
                   "$<$<CONFIG:Release>:${ASSIMP_LIBS}>"
                   "$<$<CONFIG:Profile>:${ASSIMP_LIBS}>"
                   "$<$<CONFIG:Development>:${ASSIMP_LIBS}>")
target_link_libraries(ls-model ${ASSIMP_LIBFILE})

install(TARGETS ls-model RUNTIME DESTINATION bin)
