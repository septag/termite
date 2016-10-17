#include "pch.h"

#include <cstring>
#include <cstdarg>
#include <string>

#include "bx/string.h"
#include "bx/mutex.h"

struct ErrorItem
{
    char* desc;
    char* source;
    int line;
};

struct ErrorReport
{
    bx::AllocatorI* alloc;
    bx::Mutex mtx;
    ErrorItem* reports[T_ERROR_MAX_STACK_SIZE];
    int numReports;
    char* fullString;

    ErrorReport(bx::AllocatorI* _alloc)
    {
        alloc = _alloc;
        numReports = 0;
        fullString = nullptr;
        memset(reports, 0x00, sizeof(ErrorItem*)*T_ERROR_MAX_STACK_SIZE);
    }
};

static ErrorReport* g_err = nullptr;

int termite::initErrorReport(bx::AllocatorI* alloc)
{
    if (g_err) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_err = BX_NEW(alloc, ErrorReport)(alloc);
    if (!g_err)
        return T_ERR_OUTOFMEM;

    return 0;
}

void termite::shutdownErrorReport()
{
    if (!g_err) {
        assert(false);
        return;
    }

    bx::AllocatorI* alloc = g_err->alloc;
    assert(alloc);

    for (int i = 0; i < T_ERROR_MAX_STACK_SIZE; i++) {
        if (g_err->reports[i])
            BX_FREE(alloc, g_err->reports[i]);
    }

    if (g_err->fullString)
        BX_FREE(alloc, g_err->fullString);

    BX_FREE(alloc, g_err);
    g_err = nullptr;
}

void termite::reportError(const char* source, int line, const char* desc)
{
    if (!g_err)
        return;
    bx::MutexScope mtx(g_err->mtx);

    if (g_err->numReports == T_ERROR_MAX_STACK_SIZE)
        return;
    bx::AllocatorI* alloc = g_err->alloc;

    size_t totalSize =
        sizeof(ErrorItem) +
        (desc ? (strlen(desc) + 1) : 0) +
        (source ? (strlen(source) + 1) : 0);
    uint8_t* buff = (uint8_t*)BX_ALLOC(alloc, totalSize);

    if (buff) {
        ErrorItem* report = (ErrorItem*)buff;
        buff += sizeof(ErrorItem);
        if (desc) {
            report->desc = (char*)buff;
            buff += strlen(desc) + 1;
            strcpy(report->desc, desc);
        } else {
            report->desc = nullptr;
        }

        if (source) {
            report->source = (char*)buff;
            strcpy(report->source, source);
        } else {
            report->source = nullptr;
        }

        report->line = line;
        g_err->reports[g_err->numReports++] = report;
    }
}

void termite::reportErrorf(const char* source, int line, const char* fmt, ...)
{
    if (fmt) {
        char text[4096];

        va_list args;
        va_start(args, fmt);
        vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);

        reportError(source, line, text);
    } else {
        reportError(source, line, nullptr);
    }
}

const char* termite::getErrorCallstack()
{
    if (!g_err)
        return "";

    bx::MutexScope mtx(g_err->mtx);
    if (!g_err->numReports)
        return "";

    size_t size = 0;
    for (int i = g_err->numReports - 1; i >= 0; i--) {
        const ErrorItem* r = g_err->reports[i];

        char line[256];
        bx::snprintf(line, sizeof(line), "- %s (Line:%d)\n", r->source ? r->source : "", r->line);
        size += strlen(line) + 1;

        g_err->fullString = (char*)BX_REALLOC(g_err->alloc, g_err->fullString, size);
        if (!g_err->fullString) {
            assert(false);
            return "";
        }

        if (i == g_err->numReports - 1)
            strcpy(g_err->fullString, line);
        else
            strcat(g_err->fullString, line);
    }

    return g_err->fullString;
}

const char* termite::getErrorString()
{
    if (!g_err)
        return "";

    bx::MutexScope mtx(g_err->mtx);
    if (!g_err->numReports)
        return "";

    size_t size = 0;
    for (int i = g_err->numReports - 1; i >= 0; i--) {
        const ErrorItem* r = g_err->reports[i];

        std::string line = std::string("- ") + (r->desc ? r->desc : "") + std::string("\n");

        size += line.length() + 1;

        g_err->fullString = (char*)BX_REALLOC(g_err->alloc, g_err->fullString, size);
        if (!g_err->fullString) {
            assert(0);
            return "";
        }
        if (i == g_err->numReports - 1)
            strcpy(g_err->fullString, line.c_str());
        else
            strcat(g_err->fullString, line.c_str());
    }

    return g_err->fullString;
}

const char* termite::getLastErrorString()
{
    if (!g_err)
        return "";

    bx::MutexScope mtx(g_err->mtx);
    if (g_err->numReports) {
        return g_err->reports[g_err->numReports - 1]->desc ? g_err->reports[g_err->numReports - 1]->desc : "";
    } else {
        return "";
    }
}

void termite::clearErrors()
{
    if (!g_err)
        return;
    bx::MutexScope mtx(g_err->mtx);

    for (int i = 0; i < g_err->numReports; i++) {
        BX_FREE(g_err->alloc, g_err->reports[i]);
        g_err->reports[i] = nullptr;
    }
    g_err->numReports = 0;
}

