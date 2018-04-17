# PROJECT: texpack
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

add_executable(texpack ${SOURCE_FILES} ${INCLUDE_FILES})
target_link_libraries(texpack bx lz4)
target_include_directories(texpack PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../deps)

set_target_properties(texpack PROPERTIES FOLDER Tools)
install(TARGETS texpack RUNTIME DESTINATION bin)
