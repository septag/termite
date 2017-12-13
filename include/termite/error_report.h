#pragma once

#include "bx/allocator.h"
#include "bxx/logger.h"

// Maximum error stack size that is allocated inside error handler
// Which means that more throws than the maximum number will not be saved unless they are 'Clear'ed
// Define your own stack size to override this value
#ifndef T_ERROR_MAX_STACK_SIZE
#   define T_ERROR_MAX_STACK_SIZE 32
#endif

#define T_ERROR(_Fmt, ...) termite::reportErrorf(__FILE__, __LINE__, _Fmt, ##__VA_ARGS__)

#define T_ERR_FAILED -1
#define T_ERR_OUTOFMEM -2
#define T_ERR_ALREADY_INITIALIZED -3
#define T_ERR_BUSY -4
#define T_ERR_NOT_INITIALIZED -5
#define T_ERR_ALREADY_EXISTS -6
#define T_ERR_IO_FAILED -7

#if defined(_DEBUG) || termite_DEV
#ifdef BX_CHECK
#   undef BX_CHECK
#endif
#define BX_CHECK(_condition, _fmt, ...) \
    if (!BX_IGNORE_C4127(_condition)) { \
         bx::logPrintf(__FILE__, __LINE__, bx::LogType::Fatal, _fmt, ##__VA_ARGS__);    \
    }
#endif

namespace termite
{
    int initErrorReport(bx::AllocatorI* alloc);
    void shutdownErrorReport();

    TERMITE_API void reportError(const char* source, int line, const char* desc);
    TERMITE_API void reportErrorf(const char* source, int line, const char* fmt, ...);
    TERMITE_API const char* getErrorCallstack();
    TERMITE_API const char* getErrorString();
    TERMITE_API const char* getLastErrorString();
    TERMITE_API void clearErrors();
} // namespace termite


