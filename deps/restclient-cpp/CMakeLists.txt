cmake_minimum_required(VERSION 3.0)
project(restclient-cpp)

add_definitions(-DCURL_STATICLIB)

set(SOURCE_FILES 
    source/connection.cc
    source/helpers.cc
    source/restclient.cc 
    include/restclient-cpp/connection.h
    include/restclient-cpp/helpers.h
    include/restclient-cpp/restclient.h)

add_library(restclient-cpp STATIC ${SOURCE_FILES})
target_include_directories(restclient-cpp 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CURL_INCLUDE_DIR})

if (WIN32)
    set(ADDITIONAL_LIBS ws2_32)
elseif (ANDROID)
    set(ADDITIONAL_LIBS z)
endif()

target_link_libraries(restclient-cpp PUBLIC ${CURL_LIBRARY} ${ADDITIONAL_LIBS})
set_target_properties(restclient-cpp PROPERTIES FOLDER Deps ${IOS_GENERAL_PROPERTIES})


