# PROJECT: modelc
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*" "../tools_common/*.c*")
source_group(source FILES ${SOURCE_FILES})

set(INCLUDE_FILES ../include_common/t3d_format.h)
source_group(common FILES ${INCLUDE_FILES})

include_directories(${DEP_ROOT_DIR})

# assimp
if (CMAKE_BUILD_TYPE)
    set(ASSIMP_LIBFILE "$<$<CONFIG:Debug>:${ASSIMP_LIBS_DEBUG}>"
                       "$<$<CONFIG:Release>:${ASSIMP_LIBS}>"
                       "$<$<CONFIG:Profile>:${ASSIMP_LIBS}>"
                       "$<$<CONFIG:Development>:${ASSIMP_LIBS}>")
else()
    set(ASSIMP_LIBFILE ${ASSIMP_LIBS})
endif()

include_directories(${ASSIMP_INCLUDE_DIRS})
add_executable(modelc ${SOURCE_FILES} ${INCLUDE_FILES})
target_link_libraries(modelc ${ASSIMP_LIBFILE})

install(TARGETS modelc RUNTIME DESTINATION bin)
