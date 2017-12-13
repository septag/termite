#pragma once

#include <cstdio>

#include "../bx/platform.h"
#include "terminal_colors.h"

#if __APPLE__
#include <time.h>
#endif

#ifdef BX_TRACE
#   undef BX_TRACE
#endif
#ifdef BX_WARN
#   undef BX_WARN
#endif

#define BX_TRACE(_Fmt, ...) bx::logPrintf(__FILE__, __LINE__, bx::LogType::Text, _Fmt, ##__VA_ARGS__)
#define BX_VERBOSE(_Fmt, ...) bx::logPrintf(__FILE__, __LINE__, bx::LogType::Verbose, _Fmt, ##__VA_ARGS__)
#define BX_FATAL(_Fmt, ...) bx::logPrintf(__FILE__, __LINE__, bx::LogType::Fatal, _Fmt, ##__VA_ARGS__)
#define BX_WARN(_Fmt, ...) bx::logPrintf(__FILE__, __LINE__, bx::LogType::Warning, _Fmt, ##__VA_ARGS__)
#define BX_BEGINP(_Fmt, ...) bx::logBeginProgress(__FILE__, __LINE__, _Fmt, ##__VA_ARGS__)
#define BX_END_OK() bx::logEndProgress(bx::LogProgressResult::Ok)
#define BX_END_FATAL() bx::logEndProgress(bx::LogProgressResult::Fatal)
#define BX_END_NONFATAL() bx::logEndProgress(bx::LogProgressResult::NonFatal)

namespace bx
{
    struct LogType
    {
        enum Enum
        {
            Text,
            Verbose,
            Fatal,
            Warning,
            Debug
        };
    };

    struct LogColor
    {
        enum Enum
        {
            None = 0,
            Green,
            Red,
            Gray,
            Cyan,
            Yellow,
            Magenta,
            Black,
            White
        };
    };

    struct LogProgressResult
    {
        enum Enum
        {
            Ok,
            Fatal,
            NonFatal
        };
    };

    // This will only passed to callbacks
    struct LogExtraParam
    {
        enum Enum
        {
            None = 0,
            InProgress,
            ProgressEndOk,
            ProgressEndFatal,
            ProgressEndNonFatal
        };
    };

    struct LogTimeFormat
    {
        enum Enum
        {
            Time,
            DateTime
        };
    };

    typedef void(*LogCallbackFn)(const char* filename, int line, LogType::Enum type, const char* text, void* userData, 
                                 LogExtraParam::Enum extra, time_t tm);

    void setLogTag(const char* tag);
    bool enableLogToFile(const char* filepath, const char* errFilepath = nullptr);
    bool enableLogToFileHandle(FILE* file, FILE* errFile = nullptr);
    void enableLogToCallback(LogCallbackFn callback, void* userParam);
    void enableLogTimestamps(LogTimeFormat::Enum timeFormat);

    void disableLogToFile();
    void disableLogToCallback();
    void disableLogTimestamps();

    void logPrint(const char* sourceFile, int line, LogType::Enum type, const char* text);
    void logPrintf(const char* sourceFile, int line, LogType::Enum type, const char* fmt, ...);
    void logBeginProgress(const char* sourceFile, int line, const char* fmt, ...);
    void logEndProgress(LogProgressResult::Enum result);

    void excludeFromLog(LogType::Enum type);
    void includeToLog(LogType::Enum type);
    void overrideLogColor(LogColor::Enum color);
}


