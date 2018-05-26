# taken from apple implementation
cmake_minimum_required(VERSION 3.0)
project(base64)

set(SOURCE_FILES base64.c base64.h)
add_library(base64 STATIC ${SOURCE_FILES})
set_target_properties(base64 PROPERTIES 
    FOLDER Deps
    ${IOS_GENERAL_PROPERTIES})



