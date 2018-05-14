#pragma once

#include "bx/allocator.h"
#include "logger.h"
#include "bx/debug.h"

// Maximum error stack size that is allocated inside error handler
// Which means that more throws than the maximum number will not be saved unless they are 'Clear'ed
// Define your own stack size to override this value
#ifndef TEE_ERROR_MAX_STACK_SIZE
#   define TEE_ERROR_MAX_STACK_SIZE 32
#endif

#define TEE_ERROR(_Fmt, ...) tee::err::reportf(__FILE__, __LINE__, _Fmt, ##__VA_ARGS__)

#if defined(_DEBUG) || termite_DEV
#ifdef BX_CHECK
#   undef BX_CHECK
#endif
#define BX_CHECK(_condition, _fmt, ...) \
    if (!BX_IGNORE_C4127(_condition)) { \
         bx::debugPrintf(_fmt, ##__VA_ARGS__);    \
    }
#endif

namespace tee
{
    namespace err {
        TEE_API void report(const char* source, int line, const char* desc);
        TEE_API void reportf(const char* source, int line, const char* fmt, ...);
        TEE_API const char* getCallback();
        TEE_API const char* getString();
        TEE_API const char* getLastString();
        TEE_API void clear();
    }   // namespace ecs
} // namespace tee


