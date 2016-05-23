##############################################
# package downloader
function(download_package URL TARGET)
    get_filename_component(FILE_NAME ${URL} NAME)
    get_filename_component(FILE_PATH ${URL} DIRECTORY)

    set(DEST_FILEPATH ${CMAKE_BINARY_DIR}/packages/${FILE_NAME})

    if (NOT EXISTS ${DEST_FILEPATH})
        message("Downloading ${URL} ...")
        file(DOWNLOAD ${URL} ${DEST_FILEPATH})
    else()
        message(STATUS "Library '${FILE_NAME}' is already downloaded.")
        message(STATUS "  - If you wish to redownload, delete the path: '${DEST_FILEPATH}'")
    endif()
    
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${DEST_FILEPATH}
                    WORKING_DIRECTORY ${TARGET}
                    RESULT_VARIABLE rv)
    if (NOT rv OR NOT ${rv} EQUAL 0)
        message(FATAL_ERROR "Installing dependency '${FILE_NAME}' failed")
    endif()    
endfunction()
