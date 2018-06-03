#include "pch.h"

#include <string.h>
#include <stdarg.h>
#include <string>

#include "bx/string.h"
#include "bx/mutex.h"

#include "error_report.h"
#include "internal.h"

namespace tee {
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
        ErrorItem* reports[TEE_ERROR_MAX_STACK_SIZE];
        int numReports;
        char* fullString;

        ErrorReport(bx::AllocatorI* _alloc)
        {
            alloc = _alloc;
            numReports = 0;
            fullString = nullptr;
            bx::memSet(reports, 0x00, sizeof(ErrorItem*)*TEE_ERROR_MAX_STACK_SIZE);
        }
    };

    static ErrorReport* gErr = nullptr;

    bool err::init(bx::AllocatorI* alloc)
    {
        if (gErr) {
            BX_ASSERT(false);
            return false;
        }

        gErr = BX_NEW(alloc, ErrorReport)(alloc);
        if (!gErr)
            return false;

        return true;
    }

    void err::shutdown()
    {
        if (!gErr) {
            BX_ASSERT(false);
            return;
        }

        bx::AllocatorI* alloc = gErr->alloc;
        BX_ASSERT(alloc);

        for (int i = 0; i < TEE_ERROR_MAX_STACK_SIZE; i++) {
            if (gErr->reports[i])
                BX_FREE(alloc, gErr->reports[i]);
        }

        if (gErr->fullString)
            BX_FREE(alloc, gErr->fullString);

        BX_FREE(alloc, gErr);
        gErr = nullptr;
    }

    void err::report(const char* source, int line, const char* desc)
    {
        if (!gErr)
            return;
        bx::MutexScope mtx(gErr->mtx);

        if (gErr->numReports == TEE_ERROR_MAX_STACK_SIZE)
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

    void err::reportf(const char* source, int line, const char* fmt, ...)
    {
        if (fmt) {
            char text[4096];

            va_list args;
            va_start(args, fmt);
            vsnprintf(text, sizeof(text), fmt, args);
            va_end(args);

            err::report(source, line, text);
        } else {
            err::report(source, line, nullptr);
        }
    }

    const char* err::getCallback()
    {
        if (!gErr)
            return "";

        bx::MutexScope mtx(gErr->mtx);
        if (!gErr->numReports)
            return "";

        size_t size = 0;
        for (int i = gErr->numReports - 1; i >= 0; i--) {
            const ErrorItem* r = gErr->reports[i];

            char line[256];
            bx::snprintf(line, sizeof(line), "- %s (Line:%d)\n", r->source ? r->source : "", r->line);
            size += strlen(line) + 1;

            gErr->fullString = (char*)BX_REALLOC(gErr->alloc, gErr->fullString, size);
            if (!gErr->fullString) {
                BX_ASSERT(false);
                return "";
            }

            if (i == gErr->numReports - 1)
                strcpy(gErr->fullString, line);
            else
                strcat(gErr->fullString, line);
        }

        return gErr->fullString;
    }

    const char* err::getString()
    {
        if (!gErr)
            return "";

        bx::MutexScope mtx(gErr->mtx);
        if (!gErr->numReports)
            return "";

        size_t size = 0;
        for (int i = gErr->numReports - 1; i >= 0; i--) {
            const ErrorItem* r = gErr->reports[i];

            std::string line = std::string("- ") + (r->desc ? r->desc : "") + std::string("\n");

            size += line.length() + 1;

            gErr->fullString = (char*)BX_REALLOC(gErr->alloc, gErr->fullString, size);
            if (!gErr->fullString) {
                BX_ASSERT(0);
                return "";
            }
            if (i == gErr->numReports - 1)
                strcpy(gErr->fullString, line.c_str());
            else
                strcat(gErr->fullString, line.c_str());
        }

        return gErr->fullString;
    }

    const char* err::getLastString()
    {
        if (!gErr)
            return "";

        bx::MutexScope mtx(gErr->mtx);
        if (gErr->numReports) {
            return gErr->reports[gErr->numReports - 1]->desc ? gErr->reports[gErr->numReports - 1]->desc : "";
        } else {
            return "";
        }
    }

    void err::clear()
    {
        if (!gErr)
            return;
        bx::MutexScope mtx(gErr->mtx);

        for (int i = 0; i < gErr->numReports; i++) {
            BX_FREE(gErr->alloc, gErr->reports[i]);
            gErr->reports[i] = nullptr;
        }
        gErr->numReports = 0;
    }

}   // namespace tee