#pragma once

#include "bx/bx.h"
#include "bx/allocator.h"
#include "bxx/logger.h"

#include <cassert>

#include "bitmask/bitmask_operators.hpp"

#include "uv.h"

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

#include "error_report.h"

// Versioning Macros
#define ST_MAKE_VERSION(_Major, _Minor)  (uint32_t)(((_Major & 0xffff)<<16) | (_Minor & 0xffff))
#define ST_VERSION_MAJOR(_Ver) (uint16_t)((_Ver >> 16) & 0xffff)
#define ST_VERSION_MINOR(_Ver) (uint16_t)(_Ver & 0xffff)

// Handles
#define ST_HANDLE(_Name) struct _Name { uint16_t idx; }
namespace st { static const uint16_t sInvalidHandle = UINT16_MAX; }
#define ST_INVALID_HANDLE  {st::sInvalidHandle}

// Use this macro to define flag enum types
#define ST_DEFINE_FLAG_TYPE(_Type) \
    template <> \
    struct enable_bitmask_operators<_Type> { \
        static const bool enable = true; \
    }

namespace st
{
    struct coreConfig
    {
        int updateInterval;
        char pluginPath[128];

        coreConfig()
        {
            updateInterval = 0;
            pluginPath[0] = 0;
        }
    };

    typedef void(*coreFnUpdate)();

    // Public
    STENGINE_API coreConfig* coreLoadConfig(const char* confFilepath);
    STENGINE_API void coreFreeConfig(coreConfig* conf);

    STENGINE_API int coreInit(const coreConfig& conf, coreFnUpdate updateFn);
    STENGINE_API void coreShutdown();
    STENGINE_API void coreRun();
    STENGINE_API uint32_t coreGetVersion();

    STENGINE_API bx::AllocatorI* coreGetAlloc();
    STENGINE_API uv_loop_t* coreGetMainLoop();
} // namespace st

