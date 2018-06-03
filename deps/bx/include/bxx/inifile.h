#pragma once

#include "bx/file.h"
#include "bx/string.h"

namespace bx 
{
    typedef void (*PfnIniKeyValueCallback)(const char* key, const char* value, void* userParam);

    static bool parseIniFile(const char* iniFilepath, PfnIniKeyValueCallback callback, void* userParam, 
                             bx::AllocatorI* alloc)
    {
        BX_ASSERT(alloc);

        bx::FileReader reader;
        bx::Error err;
        if (!reader.open(iniFilepath, &err))
            return false;

        int32_t size = (int32_t)reader.seek(0, bx::Whence::End);
        char* contents = (char*)BX_ALLOC(alloc, size + 1);
        if (!contents) {
            reader.close();
            return false;
        }

        reader.seek(0, bx::Whence::Begin);
        reader.read(contents, size, &err);
        contents[size] = 0;
        reader.close();

        char line[256];
        const char* lineBegin = contents;
        while (true) {
            const char* lineEnd = bx::streol(lineBegin);
            size_t lineSize = (size_t)(lineEnd - lineBegin);
            char* equal;
            const char* nextLine;

            if (lineSize != 0) {
                bx::strCopy(line, (int32_t)bx::uint32_min((uint32_t)lineSize+1, sizeof(line) - 1), lineBegin);
            } else {
                bx::strCopy(line, sizeof(line), lineBegin);
            }

            if (line[0] == '#') {
                goto skipToNextLine;
            }

            equal = strchr(line, '=');
            if (equal) {
                char key[256];
                char value[256];

                *equal = 0;
                bx::strCopy(key, sizeof(key), line);
                bx::strCopy(value, sizeof(value), equal + 1);

                callback(bx::strws(key), bx::strws(value), userParam);
            }

        skipToNextLine:
            nextLine = bx::strnl(lineBegin);   // Proceed to next line
            if (nextLine == lineBegin)
                break;
            lineBegin = nextLine;
        }

        BX_FREE(alloc, contents);

        return true;
    }

}   // namespace: bx