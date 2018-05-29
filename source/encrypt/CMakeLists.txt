# PROJECT: encrypt
cmake_minimum_required(VERSION 3.3)

file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c*")
source_group(source FILES ${SOURCE_FILES})

add_executable(encrypt ${SOURCE_FILES} ${INCLUDE_FILES})
target_link_libraries(encrypt bx tiny-AES128-C lz4)
target_include_directories(encrypt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../deps)

set_target_properties(encrypt PROPERTIES FOLDER Tools ${IOS_DEPLOYMENT_TARGET})
install(TARGETS encrypt RUNTIME DESTINATION bin)
