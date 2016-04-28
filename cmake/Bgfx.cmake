include(Utility)

function(bgfx_get_platform PLATFORM)
    if (WIN32)
        set(${PLATFORM} "windows" PARENT_SCOPE)
    elseif (APPLE)
        set(${PLATFORM} "osx" PARENT_SCOPE)
    elseif (UNIX)
        set(${PLATFORM} "linux" PARENT_SCOPE)
    endif()
endfunction()

function(bgfx_add_shaders SHADER_FILES SHADER_DEFINES INCLUDE_DIRS OUTPUT_DIR OUTPUT_C_HEADER SHADER_GEN_FILES)
    if (WIN32)
        set(SHADERC_EXT ".exe")
    endif()

    bgfx_get_platform(SHADER_PLATFORM)
    set(BGFX_SHADER_INCLUDE_DIR ${DEP_ROOT_DIR}/bgfx/include/shader)
    set(BGFX_SHADERC_PATH ${DEP_ROOT_DIR}/bgfx/bin/shaderc${SHADERC_EXT})

    # make output directory
    if (NOT IS_DIRECTORY ${OUTPUT_DIR})
        file(MAKE_DIRECTORY ${OUTPUT_DIR})
    endif()

    # make include dirs string
    if (INCLUDE_DIRS)
        join_array("${INCLUDE_DIRS}" "$<SEMICOLON>" JOINED_INCLUDE_DIRS)
       set(BGFX_SHADER_INCLUDE_DIR "${BGFX_SHADER_INCLUDE_DIR}$<SEMICOLON>${JOINED_INCLUDE_DIRS}")
    endif()

    if (SHADER_DEFINES)
        join_array("${SHADER_DEFINES}" "$<SEMICOLON>" BGFX_SHADER_DEFINES)
    endif()

    if (NOT OUTPUT_DIR)
        set(OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    else()
        get_filename_component(OUTPUT_DIR ${OUTPUT_DIR} ABSOLUTE)
    endif()

    # build shaders
    set(GEN_FILES)
    foreach(SHADER_FILE ${SHADER_FILES})
        get_filename_component(SHADER_FILE_ABS ${SHADER_FILE} ABSOLUTE)
        get_filename_component(SHADER_FILE_EXT ${SHADER_FILE} EXT)
        get_filename_component(SHADER_FILE_NAME ${SHADER_FILE} NAME_WE)
        get_filename_component(SHADER_FILE_DIR ${SHADER_FILE_ABS} DIRECTORY)

        # shader type and output file name
        if (SHADER_FILE_EXT STREQUAL ".vsc")
            set(SHADER_TYPE "vertex")
            set(OUTPUT_FILEPATH ${OUTPUT_DIR}/${SHADER_FILE_NAME}.vso)
        elseif (SHADER_FILE_EXT STREQUAL ".fsc")
            set(SHADER_TYPE "fragment")
            set(OUTPUT_FILEPATH ${OUTPUT_DIR}/${SHADER_FILE_NAME}.fso)
        endif()

        # Build only if SHADER_TYPE is determined
        if (SHADER_TYPE)
            set(ARGS -f ${SHADER_FILE} -o ${OUTPUT_FILEPATH} -i ${BGFX_SHADER_INCLUDE_DIR})

            if (OUTPUT_C_HEADER)
                set(ARGS ${ARGS} --bin2c) 
            endif()

            if (BGFX_SHADER_DEFINES)
                set(ARGS ${ARGS} --define ${BGFX_SHADER_DEFINES})
            endif()

            set(ARGS ${ARGS} --platform ${SHADER_PLATFORM})

            # Shader profile (dx only)
            if (WIN32)
                if (SHADER_TYPE STREQUAL "vertex")
                    set(BGFX_SHADER_PROFILE --profile vs_4_0)
                elseif (SHADER_TYPE STREQUAL "fragment")
                    set(BGFX_SHADER_PROFILE --profile ps_4_0)
                endif()
                set(ARGS ${ARGS} ${BGFX_SHADER_PROFILE})
            endif()

            # Varyingdef files is the same name as the shader file (under the same directory) with .vdef extension
            file(RELATIVE_PATH VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER_FILE_DIR}/${SHADER_FILE_NAME}.vdef)
            set(ARGS ${ARGS} --type ${SHADER_TYPE} --varyingdef ${VARYING_DEF})

            file(RELATIVE_PATH OUTPUT_RELATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${OUTPUT_FILEPATH})
            list(APPEND GEN_FILES ${OUTPUT_RELATIVE_PATH})
            file(RELATIVE_PATH INPUT_SHADER_FILE_REL ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER_FILE_ABS})

            add_custom_command(
                OUTPUT ${OUTPUT_FILEPATH} 
                COMMAND ${BGFX_SHADERC_PATH} 
                ARGS ${ARGS} 
                DEPENDS ${SHADER_FILE}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Compiling bgfx shader '${INPUT_SHADER_FILE_REL}' ..." 
                VERBATIM)
        endif()

    endforeach()

    set(${SHADER_GEN_FILES} "${GEN_FILES}" PARENT_SCOPE)

endfunction()

