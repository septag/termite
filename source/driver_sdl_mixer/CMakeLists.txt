# PROJECT: sdl_mixer_driver
cmake_minimum_required(VERSION 3.3)

set(SOURCE_FILES "sdl_mixer_driver.cpp")
source_group(source FILES ${SOURCE_FILES})

add_library(sdl_mixer_driver ${BUILD_LIBRARY_TYPE} ${SOURCE_FILES})

target_link_libraries(sdl_mixer_driver PRIVATE bx ${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY})
target_include_directories(sdl_mixer_driver PRIVATE  ${SDL2_INCLUDE_DIR} ${SDL2_MIXER_INCLUDE_DIR})
set_target_properties(sdl_mixer_driver PROPERTIES FOLDER Plugins)


