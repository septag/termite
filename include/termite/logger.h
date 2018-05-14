#pragma once

#include "bx/platform.h"
#include "bxx/terminal_colors.h"

#ifdef BX_TRACE
#   undef BX_TRACE
#endif
#ifdef BX_WARN
#   undef BX_WARN
#endif

#define BX_TRACE(_Fmt, ...) tee::debug::printf(__FILE__, __LINE__, tee::LogType::Text, _Fmt, ##__VA_ARGS__)
#define BX_VERBOSE(_Fmt, ...) tee::debug::printf(__FILE__, __LINE__, tee::LogType::Verbose, _Fmt, ##__VA_ARGS__)
#define BX_FATAL(_Fmt, ...) tee::debug::printf(__FILE__, __LINE__, tee::LogType::Fatal, _Fmt, ##__VA_ARGS__)
#define BX_WARN(_Fmt, ...) tee::debug::printf(__FILE__, __LINE__, tee::LogType::Warning, _Fmt, ##__VA_ARGS__)
#define BX_BEGINP(_Fmt, ...) tee::debug::beginProgress(__FILE__, __LINE__, _Fmt, ##__VA_ARGS__)
#define BX_END_OK() tee::debug::endProgress(tee::LogProgressResult::Ok)
#define BX_END_FATAL() tee::debug::endProgress(tee::LogProgressResult::Fatal)
#define BX_END_NONFATAL() tee::debug::endProgress(tee::LogProgressResult::NonFatal)

namespace tee
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

    namespace debug {
        TEE_API void setLogTag(const char* tag);
        TEE_API bool setLogToFile(const char* filepath, const char* errFilepath = nullptr);
        /// Note: Logging to terminal, disables logging to file
        TEE_API bool setLogToTerminal();
        TEE_API void setLogToCallback(LogCallbackFn callback, void* userParam);
        TEE_API void setLogTimestamps(LogTimeFormat::Enum timeFormat);

        TEE_API void disableLogToFile();
        TEE_API void disableLogToCallback();
        TEE_API void disableLogTimestamps();

        TEE_API void print(const char* sourceFile, int line, LogType::Enum type, const char* text);
        TEE_API void printf(const char* sourceFile, int line, LogType::Enum type, const char* fmt, ...);
        TEE_API void beginProgress(const char* sourceFile, int line, const char* fmt, ...);
        TEE_API void endProgress(LogProgressResult::Enum result);

        TEE_API void excludeFromLog(LogType::Enum type);
        TEE_API void includeToLog(LogType::Enum type);
        TEE_API void overrideLogColor(LogColor::Enum color);
    }
}   // namespace tee


