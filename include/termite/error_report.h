#pragma once

#include "bx/allocator.h"

// Maximum error stack size that is allocated inside error handler
// Which means that more throws than the maximum number will not be saved unless they are 'Clear'ed
// Define your own stack size to override this value
#ifndef ST_ERROR_MAX_STACK_SIZE
#   define T_ERROR_MAX_STACK_SIZE 32
#endif

#define T_ERROR(_Fmt, ...) termite::errReportf(__FILE__, __LINE__, _Fmt, ##__VA_ARGS__)

#define T_OK 0
#define T_ERR_FAILED -1
#define T_ERR_OUTOFMEM -2
#define T_ERR_ALREADY_INITIALIZED -3
#define T_ERR_BUSY -4
#define T_ERR_NOT_INITIALIZED -5

namespace termite
{
    // Internal
    int errInit(bx::AllocatorI* alloc);
    void errShutdown();

    // Public
    TERMITE_API void errReport(const char* source, int line, const char* desc);
    TERMITE_API void errReportf(const char* source, int line, const char* fmt, ...);
    TERMITE_API const char* errGetCallstack();
    TERMITE_API const char* errGetString();
    TERMITE_API const char* errGetLastString();
    TERMITE_API void errClear();
} // namespace st


