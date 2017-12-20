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
        Path& joinUnix(const char* path);
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
        if (r) {
            *r = 0;
            r = strrchr(p.text, '.');
            if (r)
                *r = 0;
        }
        return p;
    }

    inline Path Path::getFileExt() const
    {
        Path p;
        int len = getLength();
        if (len > 0) {
            const char* start = strrchr((const char*)this->text, '/');
            if (!start)
                start = strrchr((const char*)this->text, '\\');
            if (!start)
                start = (const char*)this->text;
            const char* end = &this->text[len-1];
            for (const char* e = start; e < end; ++e) {
                if (*e != '.')
                    continue;
                strcpy(p.text, e + 1);
                break;
            }
        }        
        
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

        if (up)
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
                if (this->text[strlen(this->text)-1] != sep[0])
                    bx::strCat(this->text, sizeof(this->text), sep);
                bx::strCat(this->text, sizeof(this->text), path[0] != sep[0] ? path : path + 1);
            }
        }   else    {
            bx::strCopy(this->text, sizeof(this->text), path);
        }

        return *this;
    }

    inline Path& Path::joinUnix(const char* path)
    {
        const char* sep = "/";
        if (this->text[0] != 0) {
            if (path[0] != 0) {
                bx::strCat(this->text, sizeof(this->text), sep);
                bx::strCat(this->text, sizeof(this->text), path);
            }
        } else {
            bx::strCopy(this->text, sizeof(this->text), path);
        }

        return *this;
    }

#if BX_PLATFORM_WINDOWS
    inline bx::Path getTempDir()
    {
        bx::Path r;
        GetTempPath(sizeof(r), r.getBuffer());

        /* remove \\ from the end of the path */
        int sz = r.getLength();
        if (sz > 0 && r[sz - 1] == '\\')
            r[sz - 1] = 0;
        return r;
    }
#elif BX_PLATFORM_LINUX || BX_PLATFORM_RPI || BX_PLATFORM_OSX
    inline bx::Path getTempDir()
    {
        return bx::Path("/tmp");
    }
#elif BX_PLATFORM_IOS
    inline bx::Path getTempDir()
    {
        return bx::Path("tmp");
    }
#else
    inline bx::Path getTempDir()
    {
        assert(false);
        return bx::Path();
    }
#endif

}   // namespace: bx
