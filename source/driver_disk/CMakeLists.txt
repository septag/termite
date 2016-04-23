# PROJECT: tdriver_disk
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

include_directories(${LIBUV_INCLUDE_DIRS})

add_library(tdriver_disk SHARED ${SOURCE_FILES})

set(LIBUV_LIBFILE "$<$<CONFIG:Debug>:${LIBUV_LIBS_DEBUG}>"
                  "$<$<CONFIG:Release>:${LIBUV_LIBS}>"
                  "$<$<CONFIG:Profile>:${LIBUV_LIBS}>"
                  "$<$<CONFIG:Development>:${LIBUV_LIBS}>")
target_link_libraries(tdriver_disk ${LIBUV_LIBFILE})
target_link_libraries(tdriver_disk termite)

install(TARGETS tdriver_disk 
        LIBRARY DESTINATION bin/plugins
        RUNTIME DESTINATION bin/plugins)
