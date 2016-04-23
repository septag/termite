#pragma once

#include "../bx/platform.h"
#include "terminal_colors.h"
#include <atomic>
#include <cstdio>

#define BX_LOG(_Type, _Fmt, ...) bx::logPrintf(__FILE__, __LINE__, _Type, ##__VA_ARGS__)

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

#define EXCLUDE_LIST_COUNT 6

// Export/Import API Def
#ifdef BX_SHARED_LIB
#ifdef BX_IMPLEMENT_LOGGER
#   if BX_COMPILER_MSVC
#       define BX_LOGGER_API extern "C" __declspec(dllexport) 
#   else
#       define BX_LOGGER_API extern "C" __attribute__ ((visibility ("default")))
#   endif
#else
#   if BX_COMPILER_MSVC
#       define BX_LOGGER_API extern "C" __declspec(dllimport)
#   else
#       define BX_LOGGER_API extern "C" __attribute__ ((visibility ("default")))
#   endif
#endif
#else
#   define BX_LOGGER_API extern "C" 
#endif

namespace bx
{
    enum class LogType
    {
        Text,
        Verbose,
        Fatal,
        Warning,
        Init,
        Shutdown
    };

    enum LogColor
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

    enum class LogProgressResult
    {
        Ok,
        Fatal,
        NonFatal
    };

    // This will only passed to callbacks
    enum class LogExtraParam
    {
        None = 0,
        InProgress,
        ProgressEndOk,
        ProgressEndFatal,
        ProgressEndNonFatal
    };

    enum class LogTimeFormat
    {
        Time,
        DateTime
    };

    typedef void(*LogCallbackFn)(const char* filename, int line, LogType type, const char* text, void* userData, 
                                 LogExtraParam extra, time_t tm);

    BX_LOGGER_API bool enableLogToFile(const char* filepath, const char* errFilepath = nullptr);
    BX_LOGGER_API bool enableLogToFileHandle(FILE* file, FILE* errFile = nullptr);
    BX_LOGGER_API void enableLogToCallback(LogCallbackFn callback, void* userParam);
    BX_LOGGER_API void enableLogTimestamps(LogTimeFormat timeFormat);

    BX_LOGGER_API void disableLogToFile();
    BX_LOGGER_API void disableLogToCallback();
    BX_LOGGER_API void disableLogTimestamps();

    BX_LOGGER_API void logPrint(const char* sourceFile, int line, LogType type, const char* text);
    BX_LOGGER_API void logPrintf(const char* sourceFile, int line, LogType type, const char* fmt, ...);
    BX_LOGGER_API void logBeginProgress(const char* sourceFile, int line, const char* fmt, ...);
    BX_LOGGER_API void logEndProgress(LogProgressResult result);

    BX_LOGGER_API void excludeFromLog(LogType type);
    BX_LOGGER_API void includeToLog(LogType type);
    BX_LOGGER_API void overrideLogColor(LogColor color);
}

#ifdef BX_IMPLEMENT_LOGGER
#undef BX_IMPLEMENT_LOGGER

#include <cassert>
#include <cstdio>
#include <ctime>

#if BX_PLATFORM_WINDOWS
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#endif

namespace bx
{
    struct Logger
    {
        std::atomic_bool timestamps;

        FILE* logFile;
        FILE* errFile;
        LogCallbackFn callback;
        void* userParam;

        std::atomic_bool insideProgress;
        LogTimeFormat timeFormat;

        LogType excludeList[EXCLUDE_LIST_COUNT];
        std::atomic_int numExcludes;
        std::atomic_int numErrors;
        std::atomic_int numWarnings;
        std::atomic_int numMessages;
        LogColor colorOverride;

#if BX_PLATFORM_WINDOWS
        HANDLE consoleHdl;
        WORD consoleAttrs;
#endif

        Logger()
        {
            timestamps = false;
            logFile = nullptr;
            errFile = nullptr;
            callback = nullptr;
            userParam = nullptr;
            insideProgress = false;
            timeFormat = LogTimeFormat::Time;
            numExcludes = 0;
            numErrors = 0;
            numWarnings = 0;
            numMessages = 0;
            colorOverride = LogColor::None;

#if BX_PLATFORM_WINDOWS
            consoleHdl = nullptr;
            consoleAttrs = 0;
#endif
        }
    };

    static Logger gLogger;

    bool enableLogToFile(const char* filepath, const char* errFilepath)
    {
        disableLogToFile();

        gLogger.logFile = fopen(filepath, "wt");
        if (!gLogger.logFile)
            return false;
        if (errFilepath) {
            gLogger.errFile = fopen(errFilepath, "wt");
            if (!gLogger.errFile)
                return false;
        }

        return true;
    }

    bool enableLogToFileHandle(FILE* file, FILE* errFile)
    {
        disableLogToFile();

        if (gLogger.logFile == stdout && gLogger.errFile != stderr)
            return false;

        gLogger.logFile = file;
        gLogger.errFile = errFile;

#if BX_PLATFORM_WINDOWS
        if (file == stdout || errFile == stderr) {
            gLogger.consoleHdl = GetStdHandle(STD_OUTPUT_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO coninfo;
            GetConsoleScreenBufferInfo(gLogger.consoleHdl, &coninfo);
            gLogger.consoleAttrs = coninfo.wAttributes;
        }
#endif

        return true;
    }

    void enableLogToCallback(LogCallbackFn callback, void* userParam)
    {
        assert(callback);

        gLogger.callback = callback;
        gLogger.userParam = userParam;
    }

    void enableLogTimestamps(LogTimeFormat timeFormat)
    {
        gLogger.timestamps = true;
        gLogger.timeFormat = timeFormat;
    }

    void disableLogToFile()
    {
#if BX_PLATFORM_WINDOWS
        if (gLogger.consoleHdl) {
            SetConsoleTextAttribute(gLogger.consoleHdl, gLogger.consoleAttrs);
            CloseHandle(gLogger.consoleHdl);
            gLogger.consoleHdl = nullptr;
        }
#endif

        if (gLogger.logFile) {
            fclose(gLogger.logFile);
            gLogger.logFile = nullptr;
        }

        if (gLogger.errFile) {
            fclose(gLogger.errFile);
            gLogger.errFile = nullptr;
        }
    }

    void disableLogToCallback()
    {
        gLogger.callback = nullptr;
    }

    void disableLogTimestamps()
    {
        gLogger.timestamps = false;
    }

    static void logPrintRaw(const char* filename, int line, LogType type, LogExtraParam extra, const char* text)
    {
        // Filter out mesages that are in exclude filter
        if (gLogger.numExcludes) {
            for (int i = 0, c = gLogger.numExcludes; i < c; i++) {
                if (gLogger.excludeList[i] == type)
                    return;
            }
        }

        // Add counter
        switch (type) {
        case LogType::Fatal:
            gLogger.numErrors++;   break;
        case LogType::Warning:
            gLogger.numWarnings++; break;
        default:                break;
        }

        switch (extra) {
        case LogExtraParam::ProgressEndFatal:
            gLogger.numErrors++;   break;
        case LogExtraParam::ProgressEndNonFatal:
            gLogger.numWarnings++; break;
        default:            break;
        }
        gLogger.numMessages++;


        // Timestamps
        char timestr[32];
        timestr[0] = 0;
        time_t t = 0;
        if (gLogger.timestamps) {
            t = time(nullptr);
            tm* timeinfo = localtime(&t);

            if (gLogger.timeFormat == LogTimeFormat::Time) {
                snprintf(timestr, sizeof(timestr), "%.2d:%.2d:%.2d",
                         timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
            } else if (gLogger.timeFormat == LogTimeFormat::DateTime) {
                snprintf(timestr, sizeof(timestr), "%.2d/%.2d/%.2d %.2d %.2d %.2d",
                         timeinfo->tm_mon, timeinfo->tm_mday, (timeinfo->tm_year + 1900) % 1000,
                         timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
            }
        }


        // File/Std streams
        if (gLogger.logFile) {
            const char* prefix = "";
            const char* post = "";
            bool formatted = false;
            if (gLogger.logFile == stdout) {
                formatted = true;
                // Choose color for the log line
#if !BX_PLATFORM_WINDOWS
                if (gLogger.colorOverride == LogColor::None) {
                    if (extra == LogExtraParam::None || extra == LogExtraParam::InProgress) {
                        switch (type) {
                        case LogType::Text:
                            prefix = TERM_RESET;        break;
                        case LogType::Verbose:
                            prefix = TERM_DIM;          break;
                        case LogType::Fatal:
                            prefix = TERM_RED_BOLD;      break;
                        case LogType::Warning:
                            prefix = TERM_YELLOW_BOLD;   break;
                        case LogType::Init:
                        case LogType::Shutdown:
                            prefix = TERM_CYAN;          break;
                        default:
                            prefix = TERM_RESET;        break;
                        }
                    } else {
                        switch (extra) {
                        case LogExtraParam::ProgressEndOk:
                            prefix = TERM_GREEN_BOLD;    break;
                        case LogExtraParam::ProgressEndFatal:
                            prefix = TERM_RED_BOLD;      break;
                        case LogExtraParam::ProgressEndNonFatal:
                            prefix = TERM_YELLOW_BOLD;   break;
                        default:
                            break;
                        }
                    }
                } else {
                    switch (gLogger.colorOverride) {
                    case LogColor::Black:
                        prefix = TERM_BLACK;                       break;
                    case LogColor::Cyan:
                        prefix = TERM_CYAN;                        break;
                    case LogColor::Gray:
                        prefix = TERM_DIM;                         break;
                    case LogColor::Green:
                        prefix = TERM_GREEN;                       break;
                    case LogColor::Magenta:
                        prefix = TERM_MAGENTA;                     break;
                    case LogColor::Red:
                        prefix = TERM_RED;                         break;
                    case LogColor::White:
                        prefix = TERM_WHITE;                       break;
                    case LogColor::Yellow:
                        prefix = TERM_YELLOW;                       break;
                    default:
                        break;
                    }
                }
#else
                if (gLogger.colorOverride == LogColor::None) {
                    if (extra == LogExtraParam::None || extra == LogExtraParam::InProgress) {
                        switch (type) {
                        case LogType::Text:
                            SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);        break;
                        case LogType::Verbose:
                            SetConsoleTextAttribute(gLogger.consoleHdl, gLogger.consoleAttrs);          break;
                        case LogType::Fatal:
                            SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_RED | FOREGROUND_INTENSITY);      break;
                        case LogType::Warning:
                            SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);   break;
                        case LogType::Init:
                        case LogType::Shutdown:
                            SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_BLUE | FOREGROUND_GREEN);      break;
                        default:
                            break;
                        }
                    } else {
                        switch (extra) {
                        case LogExtraParam::ProgressEndOk:
                            SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_GREEN | FOREGROUND_INTENSITY);    break;
                        case LogExtraParam::ProgressEndFatal:
                            SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_RED | FOREGROUND_INTENSITY);      break;
                        case LogExtraParam::ProgressEndNonFatal:
                            SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);   break;
                        }
                    }
                } else {
                    switch (gLogger.colorOverride) {
                    case LogColor::Black:
                        SetConsoleTextAttribute(gLogger.consoleHdl, 0);
                        break;
                    case LogColor::Cyan:
                        SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_BLUE | FOREGROUND_GREEN);
                        break;
                    case LogColor::Gray:
                        SetConsoleTextAttribute(gLogger.consoleHdl, gLogger.consoleAttrs);
                        break;
                    case LogColor::Green:
                        SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_GREEN);
                        break;
                    case LogColor::Magenta:
                        SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_RED | FOREGROUND_BLUE);
                        break;
                    case LogColor::Red:
                        SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_RED);
                        break;
                    case LogColor::White:
                        SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_RED | FOREGROUND_BLUE);
                        break;
                    case LogColor::Yellow:
                        SetConsoleTextAttribute(gLogger.consoleHdl, FOREGROUND_RED | FOREGROUND_GREEN);
                        break;
                    default:
                        break;
                    }
                }
#endif
            }

            if (extra != LogExtraParam::InProgress)
                post = "\n" TERM_RESET;
            else if (extra == LogExtraParam::InProgress)
                post = "... ";

            FILE* output = gLogger.logFile;
            if (gLogger.errFile && type == LogType::Fatal)
                output = gLogger.errFile;
            if (output) {
                if (!gLogger.timestamps || (extra != LogExtraParam::InProgress && extra != LogExtraParam::None)) {
                    fprintf(output, "%s%s%s", prefix, text, post);
                } else {
                    fprintf(output, "[%s] %s%s%s", timestr, prefix, text, post);
                }
            }
        }

        // Callback
        if (gLogger.callback)
            gLogger.callback(filename, line, type, text, gLogger.userParam, extra, t);
    }

    void logPrintf(const char* sourceFile, int line, LogType type, const char* fmt, ...)
    {
        char text[4096];

        va_list args;
        va_start(args, fmt);
        vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);

        logPrintRaw(sourceFile, line, type, LogExtraParam::None, text);
    }

    void logPrint(const char* sourceFile, int line, LogType type, const char* text)
    {
        logPrintRaw(sourceFile, line, type, LogExtraParam::None, text);
    }

    void logBeginProgress(const char* sourceFile, int line, const char* fmt, ...)
    {
        char text[4096];

        va_list args;
        va_start(args, fmt);
        vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);

        gLogger.insideProgress = true;
        logPrintRaw(sourceFile, line, LogType::Text, LogExtraParam::InProgress, text);
    }

    void logEndProgress(LogProgressResult result)
    {
        gLogger.insideProgress = false;

        LogExtraParam extra;
        const char* text;
        switch (result) {
        case LogProgressResult::Ok:
            extra = LogExtraParam::ProgressEndOk;
            text = "[   OK   ]";
            break;

        case LogProgressResult::Fatal:
            extra = LogExtraParam::ProgressEndFatal;
            text = "[ FAILED ]";
            break;

        case LogProgressResult::NonFatal:
            extra = LogExtraParam::ProgressEndNonFatal;
            text = "[ FAILED ]";
            break;

        default:
            text = "";
            extra = LogExtraParam::None;
        }

        logPrintRaw(__FILE__, __LINE__, LogType::Text, extra, text);
    }

    void excludeFromLog(LogType type)
    {
        if (gLogger.numExcludes == EXCLUDE_LIST_COUNT)
            return;

        for (int i = 0; i < gLogger.numExcludes; i++) {
            if (type == gLogger.excludeList[i])
                return;
        }

        gLogger.excludeList[gLogger.numExcludes++] = type;
    }

    void includeToLog(LogType type)
    {
        for (int i = 0; i < gLogger.numExcludes; i++) {
            if (type == gLogger.excludeList[i]) {
                for (int c = i + 1; c < gLogger.numExcludes; c++)
                    gLogger.excludeList[c - 1] = gLogger.excludeList[c];
                gLogger.numExcludes--;
                break;
            }
        }
    }

    void overrideLogColor(LogColor color)
    {
        gLogger.colorOverride = color;
    }

}
#endif  // BX_IMPLEMENT_LOGGER

