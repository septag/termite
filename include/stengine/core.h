#pragma once

#include "bx/bx.h"
#include "bx/allocator.h"

// Export/Import API Def
#ifdef STENGINE_SHARED_LIB
#ifdef STENGINE_EXPORTS
#   if BX_COMPILER_MSVC
#       define STENGINE_API extern "C" __declspec(dllexport) 
#   else
#       define STENGINE_API extern "C" __attribute__ ((dllexport))
#   endif
#else
#   if BX_COMPILER_MSVC
#       define STENGINE_API extern "C" __declspec(dllimport)
#   else
#       define STENGINE_API extern "C" __attribute__ ((dllimport))
#   endif
#endif
#else
#   define STENGINE_API extern "C" 
#endif

namespace st
{
    STENGINE_API int coreInit();
    STENGINE_API void coreShutdown();

    STENGINE_API bx::AllocatorI* coreGetAlloc();
} // namespace st

