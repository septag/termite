#pragma once

#include "bx/allocator.h"

// Maximum error stack size that is allocated inside error handler
// Which means that more throws than the maximum number will not be saved unless they are 'Clear'ed
// Define your own stack size to override this value
#ifndef ST_ERROR_MAX_STACK_SIZE
#   define ST_ERROR_MAX_STACK_SIZE 32
#endif

#define ST_ERROR(_Fmt, ...) st::errReportf(__FILE__, __LINE__, _Fmt, ##__VA_ARGS__)

#define ST_OK 0
#define ST_ERR_FAILED -1
#define ST_ERR_OUTOFMEM -2
#define ST_ERR_ALREADY_INITIALIZED -3
#define ST_ERR_BUSY -4

namespace st
{
    // Internal
    int errInit(bx::AllocatorI* alloc);
    void errShutdown();

    // Public
    STENGINE_API void errReport(const char* source, int line, const char* desc);
    STENGINE_API void errReportf(const char* source, int line, const char* fmt, ...);
    STENGINE_API const char* errGetCallstack();
    STENGINE_API const char* errGetString();
    STENGINE_API const char* errGetLastString();
    STENGINE_API void errClear();
} // namespace st


