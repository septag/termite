# PROJECT: TESTS
cmake_minimum_required(VERSION 3.3)

# test_init
add_executable(test_init "test_init.cpp")
target_link_libraries(test_init termite)

# test_sdl
add_executable(test_sdl "test_sdl.cpp")
target_include_directories(test_sdl PUBLIC ${SDL2_INCLUDE_DIR})
target_link_libraries(test_sdl termite ${SDL2_LIBRARY})

# Require plugins to be built
add_dependencies(test_init tdriver_disk)
add_dependencies(test_sdl trender_default tdriver_disk tdriver_bgfx SDL2CopyBinaries)

install(TARGETS test_init RUNTIME DESTINATION bin)
install(TARGETS test_sdl RUNTIME DESTINATION bin)
