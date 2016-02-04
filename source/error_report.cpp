#include "core.h"

#include <cstring>
#include <cstdarg>
#include <string>

struct ErrorItem
{
    char* desc;
    char* source;
    int line;
};

struct ErrorReport
{
    bx::AllocatorI* alloc;
    ErrorItem* reports[ST_ERROR_MAX_STACK_SIZE];
    int numReports;
    char* fullString;

    ErrorReport(bx::AllocatorI* _alloc)
    {
        alloc = _alloc;
        numReports = 0;
        fullString = nullptr;
        memset(reports, 0x00, sizeof(ErrorItem*)*ST_ERROR_MAX_STACK_SIZE);
    }
};

static ErrorReport* gErr = nullptr;

int st::errInit(bx::AllocatorI* alloc)
{
    if (gErr) {
        assert(false);
        return ST_ERR_ALREADY_INITIALIZED;
    }

    gErr = BX_NEW(alloc, ErrorReport)(alloc);
    if (!gErr)
        return ST_ERR_OUTOFMEM;

    return 0;
}

void st::errShutdown()
{
    if (!gErr) {
        assert(false);
        return;
    }

    bx::AllocatorI* alloc = gErr->alloc;
    assert(alloc);

    for (int i = 0; i < ST_ERROR_MAX_STACK_SIZE; i++) {
        if (gErr->reports[i])
            BX_FREE(alloc, gErr->reports[i]);
    }

    if (gErr->fullString)
        BX_FREE(alloc, gErr->fullString);

    BX_FREE(alloc, gErr);
    gErr = nullptr;
}

void st::errReport(const char* source, int line, const char* desc)
{
    if (gErr->numReports == ST_ERROR_MAX_STACK_SIZE)
        return;
    bx::AllocatorI* alloc = gErr->alloc;

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
        gErr->reports[gErr->numReports++] = report;
    }
}

void st::errReportf(const char* source, int line, const char* fmt, ...)
{
    if (fmt) {
        char text[4096];

        va_list args;
        va_start(args, fmt);
        vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);

        errReport(source, line, text);
    } else {
        errReport(source, line, nullptr);
    }
}

const char* st::errGetCallstack()
{
    if (!gErr->numReports)
        return "";

    size_t size = 0;
    for (int i = gErr->numReports - 1; i >= 0; i--) {
        const ErrorItem* r = gErr->reports[i];

        std::string line = std::string("- ") + (r->source ? r->source : "") + std::string("(Line:") +
            std::to_string(r->line) + std::string(")\n");

        size += line.length() + 1;

        gErr->fullString = (char*)BX_REALLOC(gErr->alloc, gErr->fullString, size);
        if (!gErr->fullString) {
            assert(false);
            return "";
        }

        if (i == gErr->numReports - 1)
            strcpy(gErr->fullString, line.c_str());
        else
            strcat(gErr->fullString, line.c_str());
    }

    return gErr->fullString;
}

const char* st::errGetString()
{
    if (!gErr->numReports)
        return "";

    size_t size = 0;
    for (int i = gErr->numReports - 1; i >= 0; i--) {
        const ErrorItem* r = gErr->reports[i];

        std::string line = std::string("- ") + (r->desc ? r->desc : "") + std::string("\n");

        size += line.length() + 1;

        gErr->fullString = (char*)BX_REALLOC(gErr->alloc, gErr->fullString, size);
        if (!gErr->fullString) {
            assert(0);
            return "";
        }
        if (i == gErr->numReports - 1)
            strcpy(gErr->fullString, line.c_str());
        else
            strcat(gErr->fullString, line.c_str());
    }

    return gErr->fullString;
}

const char* st::errGetLastString()
{
    if (gErr->numReports) {
        return gErr->reports[gErr->numReports - 1]->desc ? gErr->reports[gErr->numReports - 1]->desc : "";
    } else {
        return "";
    }
}

void st::errClear()
{
    for (int i = 0; i < gErr->numReports; i++) {
        BX_FREE(gErr->alloc, gErr->reports[i]);
        gErr->reports[i] = nullptr;
    }
    gErr->numReports = 0;
}

