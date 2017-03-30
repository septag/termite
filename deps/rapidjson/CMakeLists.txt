cmake_minimum_required(VERSION 3.0)
project(rapidjson)

add_library(rapidjson INTERFACE)

target_include_directories(rapidjson INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
