# PROJECT: disk_lite_driver
cmake_minimum_required(VERSION 3.3)

set(SOURCE_FILES "disk_lite_driver.cpp")
if (IOS)
    list(APPEND SOURCE_FILES "apple_bundle.mm")
endif()
source_group(source FILES "" ${SOURCE_FILES})

if (ENABLE_HOT_LOADING)
    set(EXTRA_LIBS "efsw")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../deps)
add_library(disk_lite_driver ${BUILD_LIBRARY_TYPE} ${SOURCE_FILES})
target_link_libraries(disk_lite_driver PRIVATE bx ${EXTRA_LIBS} lz4)
set_target_properties(disk_lite_driver PROPERTIES FOLDER Plugins)


