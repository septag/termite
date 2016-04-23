#pragma once

#include "string.h"
#include "../bx/platform.h"

#if !BX_PLATFORM_WINDOWS
#  include <sys/types.h>
#  include <sys/stat.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#endif

namespace bx
{
    enum class PathType
    {
        Invalid = 0,
        Directory,
        File
    };

    class Path : public String256
    {
    public:
        Path() : String256() {}
        explicit Path(const char* text) : String256(text) {}

        Path& operator=(const Path& str)
        {
            return (Path&)String256::operator=((const String256&)str);
        }

        Path& operator=(const char* str)
        {
            return (Path&)String256::operator=(str);
        }

        Path getDirectory() const;
        Path getFilename() const;
        Path getFileExt() const;
        Path getFilenameFull() const;

        Path& goUp();
        Path& toUnix();
        Path& toWindows();

        Path& normalizeSelf();
        Path& join(const char* path);
        PathType getType();        
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // Path
    inline Path Path::getDirectory() const
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

    inline Path Path::getFilename() const
    {
        Path p;

        const char* ri = strrchr((const char*)this->text, '/');
        if (!ri)
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

    inline Path Path::getFileExt() const
    {
        Path p;

        const char* r = strrchr((const char*)this->text, '.');
        const char* r2 = strchr((const char*)this->text, '/');
        if (r > r2)
            strcpy(p.text, r + 1);
        return p;
    }

    inline  Path Path::getFilenameFull() const
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

    inline Path& Path::goUp()
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

    inline Path& Path::toUnix()
    {
        return (Path&)replace('\\', '/');
    }

    inline Path& Path::toWindows()
    {
        return (Path&)replace('/', '\\');
    }

    inline Path& Path::normalizeSelf()
    {
        if (this->text[0] == 0)
            return *this;

#if BX_PLATFORM_WINDOWS
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

    inline Path& Path::join(const char* path)
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


    inline bx::PathType Path::getType()
    {
#if BX_PLATFORM_WINDOWS
        DWORD atts = GetFileAttributes(text);
        if (atts & FILE_ATTRIBUTE_DIRECTORY)
            return PathType::Directory;
        else if (atts == INVALID_FILE_ATTRIBUTES)
            return PathType::Invalid;
        else
            return PathType::File;
#else
        struct stat s;
        if (stat(text, &s) == 0) {
            if (s.st_mode & S_IFDIR)
                return PathType::Directory;
            else if ((s.st_mode & (S_IFREG | S_IFLNK)))
                return PathType::File;
            else
                return PathType::Invalid;

        } else {
            return PathType::Invalid;
        }
#endif
    }

}   // namespace: bx
