#include "log_format_proxy.h"

#include <cstdio>
#include <cstdarg>

#include "bxx/json.h"
#include "bxx/logger.h"

static bx::DefaultAllocator g_alloc;

void termite::LogFormatProxy::fatal(const char* fmt, ...)
{
    char text[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    switch (m_options) {
    case LogProxyOptions::Json:
    {
        bx::JsonNodeAllocator alloc(&g_alloc);
        bx::JsonNode* jroot = bx::createJsonNode(&alloc, nullptr, bx::JsonType::Object);
        bx::JsonNode* jerr = bx::createJsonNode(&alloc, "fatal")->setString(text);
        jroot->addChild(jerr);
        char* jsonText = bx::makeJson(jroot, &g_alloc, true);
        jroot->destroy();
        if (jsonText) {
            bx::logPrint(__FILE__, __LINE__, bx::LogType::Fatal, jsonText);
            BX_FREE(&g_alloc, jsonText);
        }
        break;
    }

    case LogProxyOptions::Text:
    {
        bx::logPrint(__FILE__, __LINE__, bx::LogType::Fatal, text);
        break;
    }
    }
}

void termite::LogFormatProxy::warn(const char* fmt, ...)
{
    char text[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    switch (m_options) {
    case LogProxyOptions::Json:
    {
        bx::JsonNodeAllocator alloc(&g_alloc);
        bx::JsonNode* jroot = bx::createJsonNode(&alloc);
        bx::JsonNode* jerr = bx::createJsonNode(&alloc, "warning")->setString(text);
        jroot->addChild(jerr);
        char* jsonText = bx::makeJson(jroot, &g_alloc, true);
        jroot->destroy();
        if (jsonText) {
            bx::logPrint(__FILE__, __LINE__, bx::LogType::Warning, jsonText);
            BX_FREE(&g_alloc, jsonText);
        }
        break;
    }

    case LogProxyOptions::Text:
    {
        bx::logPrint(__FILE__, __LINE__, bx::LogType::Warning, text);
        break;
    }
    }
}

void termite::LogFormatProxy::text(const char* fmt, ...)
{
    char text[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    switch (m_options) {
    case LogProxyOptions::Json:
    {
        bx::JsonNodeAllocator alloc(&g_alloc);
        bx::JsonNode* jroot = bx::createJsonNode(&alloc);
        bx::JsonNode* jerr = bx::createJsonNode(&alloc, "text")->setString(text);
        jroot->addChild(jerr);
        char* jsonText = bx::makeJson(jroot, &g_alloc, true);
        jroot->destroy();
        if (jsonText) {
            bx::logPrint(__FILE__, __LINE__, bx::LogType::Text, jsonText);
            BX_FREE(&g_alloc, jsonText);
        }
        break;
    }

    case LogProxyOptions::Text:
    {
        bx::logPrint(__FILE__, __LINE__, bx::LogType::Text, text);
        break;
    }
    }
}
