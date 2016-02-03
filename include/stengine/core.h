#pragma once

#include "bx/bx.h"
#include "bx/allocator.h"

#include <cassert>

#include "bitmask/bitmask_operators.hpp"

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

// Versioning Macros
#define ST_MAKE_VERSION(_Major, _Minor)  (uint32_t)(((_Major & 0xffff)<<16) | (_Minor & 0xffff))
#define ST_VERSION_MAJOR(_Ver) (uint16_t)((_Ver >> 16) & 0xffff)
#define ST_VERSION_MINOR(_Ver) (uint16_t)(_Ver & 0xffff)

#define ST_HANDLE(_Name) struct _Name { uint16_t idx; }

#define ST_DEFINE_FLAG_TYPE(_Type) \
    template <> \
    struct enable_bitmask_operators<_Type> { \
        static const bool enable = true; \
    }

namespace st
{
    STENGINE_API int coreInit();
    STENGINE_API void coreShutdown();

    STENGINE_API bx::AllocatorI* coreGetAlloc();
} // namespace st

