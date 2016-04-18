cmake_minimum_required(VERSION 3.0)
project(fcontext)
enable_language(CXX ASM)

include_directories(include/fcontext)

if(MSVC)
  set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
  add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
  add_definitions(-D_HAS_EXCEPTIONS=0)
endif()

set(HEADER "include/fcontext/fcontext.h")
set(SOURCES "source/stack.cpp")

# Currently no cross-compiling or ARM configs are supported
# Cpu Arch
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    message("x86_64 compiler detected")
    set(CPU_ARCH "x86_64")
else()
    message("i386 compiler detected")
    set(CPU_ARCH "i386")
endif()

# OS
message("System: ${CMAKE_SYSTEM_NAME}")
if (UNIX)
    if (APPLE)
        set(ASM_EXT "sysv_macho_gas.S") # OSX
    else()
        set(ASM_EXT "sysv_elf_gas.S")  # Linux/Unix
    endif()
elseif(WIN32)
    set(ASM_EXT "ms_pe_masm.asm") # Windows
endif()

set(ASM_SOURCES "asm/make_${CPU_ARCH}_${ASM_EXT}"
                "asm/jump_${CPU_ARCH}_${ASM_EXT}"
                "asm/ontop_${CPU_ARCH}_${ASM_EXT}")

add_library(fcontext STATIC ${SOURCES} ${ASM_SOURCES})

install(TARGETS fcontext DESTINATION lib)
install(FILES ${HEADER} DESTINATION include/fcontext)