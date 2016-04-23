##############################################
# package downloader
function(download_package URL TARGET)
    get_filename_component(FILE_NAME ${URL} NAME_WE)
    if (IS_DIRECTORY ${TARGET}/${FILE_NAME})
        message(STATUS "Existing library for library '${FILE_NAME}' is found.")
        message(STATUS "  - If you wish to update, delete the deps/${FILE_NAME} directory")
        return()
    endif()

    # choose name suffix based on platform and compiler
    if (MSVC14)
        set(FILE_SUFFIX "vc140")
    else()
        return()
    endif()
    
    # download the real file
    get_filename_component(FILE_EXT ${URL} EXT)
    get_filename_component(FILE_PATH ${URL} DIRECTORY)
    
    set(PLATFORM_FILENAME ${FILE_NAME}_${FILE_SUFFIX}${FILE_EXT}) 
    set(URL ${FILE_PATH}/${PLATFORM_FILENAME})
    set(DEST_FILEPATH ${CMAKE_BINARY_DIR}/packages/${PLATFORM_FILENAME})

    if (NOT EXISTS ${DEST_FILEPATH})
        message("Downloading ${URL} ...")
        file(DOWNLOAD ${URL} ${DEST_FILEPATH})
    else()
        message(STATUS "Library '${FILE_NAME}' is already downloaded.")
        message(STATUS "  - If you wish to redownload, delete the file: '${DEST_FILEPATH}'")
    endif()
    
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/packages/${PLATFORM_FILENAME}
                    WORKING_DIRECTORY ${TARGET}
                    RESULT_VARIABLE rv)
    if (NOT ${rv} EQUAL 0)
        message(FATAL_ERROR "Installing dependency '${PLATFORM_FILENAME}' failed")
    endif()
    
endfunction()
