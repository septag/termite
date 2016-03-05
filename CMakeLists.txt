cmake_minimum_required(VERSION 2.8)
project(fcontext)
enable_language(C ASM)

include_directories(include/fcontext)

set(HEADER "include/fcontext/fcontext.h")
set(SOURCES "source/stack.cpp")

# Currently no cross-compiling or ARM configs are supported
# Cpu Arch
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    MESSAGE("x86_64 compiler detected")
    SET(CPU_ARCH "x86_64")
else (CMAKE_SIZEOF_VOID_P EQUAL 8)
    MESSAGE("i386 compiler detected")
    SET(CPU_ARCH "i386")
endif (CMAKE_SIZEOF_VOID_P EQUAL 8)

# OS
MESSAGE("System: ${CMAKE_SYSTEM_NAME}")
if (UNIX)
    if (APPLE)
        SET(ASM_EXT "sysv_macho_gas.S")
    else (APPLE)
        SET(ASM_EXT "sysv_elf_gas.S")
    endif (APPLE)
else(UNIX)
    if (WIN32)
        SET(ASM_EXT "ms_pe_masm.asm")
    endif(WIN32)
endif(UNIX)    

set(ASM_SOURCES "asm/make_${CPU_ARCH}_${ASM_EXT}"
                "asm/jump_${CPU_ARCH}_${ASM_EXT}"
                "asm/ontop_${CPU_ARCH}_${ASM_EXT}")

add_library(fcontext STATIC ${SOURCES} ${ASM_SOURCES})

install(TARGETS fcontext DESTINATION lib)
install(FILES ${HEADER} DESTINATION include/fcontext)