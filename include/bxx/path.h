#pragma once

#include "string.h"

namespace bx
{

    class Path : public String256
    {
    public:
        Path() : String256() {}
        explicit Path(const char* text) : String256(text) {}

        Path getDirectory() const;
        Path getFilename() const;
        Path getFileExt() const;
        Path getFilenameFull() const;

        Path& goUp();
        Path& toUnix();
        Path& toWindows();

        Path& normalizeSelf();
        Path& join(const char* path);
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // Path
    Path Path::getDirectory() const
    {
        Path p;

        // Path with '/' or '\\' at the End
        const char* r = strrchr((const char*)this->text, '/');
        if (!r)
            r = strrchr((const char*)this->text, '\\');
        if (r)     {
            size_t o = (size_t)(r - this->text);
            strncpy(p.text, this->text, o);
            p.text[o] = 0;
        }   else    {
            strcpy(p.text, this->text);
        }

        return p;
    }

    Path Path::getFilename() const
    {
        Path p;

        const char* ri = strrchr((const char*)this->text, '/');
        if (ri)
            ri = strrchr((const char*)this->text, '\\');

        if (ri)
            strcpy(p.text, ri + 1);
        else
            strcpy(p.text, this->text);

        // Name only
        char* r = strrchr(p.text, '.');
        if (r)
            *r = 0;
        return p;
    }

    Path Path::getFileExt() const
    {
        Path p;

        const char* r = strrchr((const char*)this->text, '.');
        const char* r2 = strchr((const char*)this->text, '/');
        if (r > r2)
            strcpy(p.text, r + 1);
        return p;
    }

    Path Path::getFilenameFull() const
    {
        Path p;

        const char* r = strrchr((const char*)this->text, '/');
        if (!r)
            r = strrchr((const char*)this->text, '\\');
        if (r)
            strcpy(p.text, r + 1);
        else
            strcpy(p.text, this->text);

        return p;
    }

    Path& Path::goUp()
    {
        int s = getLength();

        if (this->text[s-1] == '/' || this->text[s-1] == '\\')
            this->text[s-1] = 0;

        // handle case when the path is like 'my/path/./'
        if (s>3 && this->text[s-2] == '.' && (this->text[s-3] == '/' || this->text[s-3] == '\\'))
            this->text[s-3] = 0;
        // handle case when the path is like 'my/path/.'
        if (s>2 && this->text[s-1] == '.' && (this->text[s-2] == '/' || this->text[s-2] == '\\'))
            this->text[s-2] = 0;

        char* up = strrchr(this->text, '/');
        if (!up)
            up = strrchr(this->text, '\\');

        if (!up)
            *up = 0;

        return *this;
    }

    Path& Path::toUnix()
    {
        return (Path&)replace('\\', '/');
    }

    Path& Path::toWindows()
    {
        return (Path&)replace('/', '\\');
    }

    Path& Path::normalizeSelf()
    {
        if (this->text[0] == 0)
            return *this;

    #if defined(OS_WIN)
        char tmp[256];
        GetFullPathName(this->text, sizeof(tmp), tmp, nullptr);
        toWindows();
        int sz = getLength();
        if (this->text[sz-1] == '\\')
            this->text[sz-1] = 0;
        return *this;
    #else
        char* tmp = realpath(this->text, nullptr);
        if (tmp)    {
            toUnix();
            ::free(tmp);
        }   else    {
            char tmp2[257];
            realpath(this->text, tmp2);
            strcpy(this->text, tmp2);
        }

        int sz = getLength();
        if (this->text[sz-1] == '/')
            this->text[sz-1] = 0;
        return *this;
    #endif
    }

    Path& Path::join(const char* path)
    {
    #if BX_PLATFORM_WINDOWS
        const char* sep = "\\";
    #else
        const char* sep = "/";
    #endif

        if (this->text[0] != 0) {
            if (path[0] != 0)   {
                bx::strlcat(this->text, sep, sizeof(this->text));
                bx::strlcat(this->text, path, sizeof(this->text));
            }
        }   else    {
            bx::strlcpy(this->text, path, sizeof(this->text));
        }

        return *this;
    }

}   // namespace: bx
