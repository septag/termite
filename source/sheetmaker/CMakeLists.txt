# PROJECT: sheetmaker
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

set(INCLUDE_FILES ../include_common/sprite_format.h)
source_group(common FILES ${INCLUDE_FILES})

add_executable(sheetmaker ${SOURCE_FILES} ${INCLUDE_FILES})
target_link_libraries(sheetmaker termite ${SDL2_LIBRARY} nanovg)
target_include_directories(sheetmaker PRIVATE ${SDL2_INCLUDE_DIR})

set_target_properties(sheetmaker PROPERTIES FOLDER Tools)
install(TARGETS sheetmaker RUNTIME DESTINATION bin)
