#pragma once

#include "bx/readerwriter.h"
#include "bx/string.h"

namespace bx 
{
    typedef void (*PfnIniKeyValueCallback)(const char* key, const char* value, void* userParam);

    static bool parseIniFile(const char* iniFilepath, PfnIniKeyValueCallback callback, void* userParam, 
                             bx::AllocatorI* alloc)
    {
        assert(alloc);

        bx::CrtFileReader reader;
        if (reader.open(iniFilepath))
            return false;

        int32_t size = (int32_t)reader.seek(0, bx::Whence::End);
        char* contents = (char*)BX_ALLOC(alloc, size + 1);
        if (!contents) {
            reader.close();
            return false;
        }

        reader.seek(0, bx::Whence::Begin);
        reader.read(contents, size);
        contents[size] = 0;
        reader.close();

        char line[256];
        const char* lineBegin = contents;
        while (true) {
            const char* lineEnd = bx::streol(lineBegin);
            size_t lineSize = (size_t)(lineEnd - lineBegin);
            if (lineSize != 0) {
                bx::strlcpy(line, lineBegin, bx::uint32_min((uint32_t)lineSize+1, sizeof(line) - 1));
            } else {
                bx::strlcpy(line, lineBegin, sizeof(line));
            }

            char* equal = strchr(line, '=');
            if (equal) {
                char key[256];
                char value[256];

                *equal = 0;
                bx::strlcpy(key, line, sizeof(key));
                bx::strlcpy(value, equal + 1, sizeof(value));

                callback(bx::strws(key), bx::strws(value), userParam);
            }

            const char* nextLine = bx::strnl(lineBegin);   // Proceed to next line
            if (nextLine == lineBegin)
                break;
            lineBegin = nextLine;
        }

        BX_FREE(alloc, contents);

        return true;
    }

}   // namespace: bx