#include <assert.h>
#include <math.h>

#include "fcontext.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_LEAN
#include <Windows.h>

/* x86_64
 * test x86_64 before i386 because icc might
 * define __i686__ for x86_64 too */
#if defined(__x86_64__) || defined(__x86_64) \
    || defined(__amd64__) || defined(__amd64) \
    || defined(_M_X64) || defined(_M_AMD64)
/* Windows seams not to provide a constant or function
 * telling the minimal stacksize */
# define MIN_STACKSIZE  8 * 1024
#else
# define MIN_STACKSIZE  4 * 1024
#endif

static size_t getPageSize()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
}

static size_t getMinSize()
{
    return MIN_STACKSIZE;
}

static size_t getMaxSize()
{
    return  1 * 1024 * 1024 * 1024; /* 1GB */
}

static size_t getDefaultSize()
{
    return 64 * 1024;   /* 64Kb */
}

#elif _POSIX_VERSION
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#if !defined (SIGSTKSZ)
# define SIGSTKSZ (8 * 1024)
# define UDEF_SIGSTKSZ
#endif

static size_t getPageSize()
{
    /* conform to POSIX.1-2001 */
    return (size_t)sysconf(_SC_PAGESIZE);
}

static size_t getMinSize()
{
    return SIGSTKSZ;
}

static size_t getMaxSize()
{
    rlimit limit;
    getrlimit(&limit);

    return (size_t)limit.rlim_max;
}

static size_t getDefaultSize()
{
    size_t size;
    size_t maxSize;
    rlimit limit;

    getrlimit(&limit);

    size = 8 * getMinSize();
    if (RLIM_INFINITY == limit.rlim_max)
        return size;
    maxSize = getMaxSize();
    return maxSize < size ? maxSize : size;
}
#endif

/* Stack allocation and protection*/
int stack_create(stack_t* s, size_t size)
{
    size_t pages;
    size_t size_;
    void* vp;
    DWORD old_options;

    if (size == 0)
        size = getDefaultSize();

    assert(size >= getMinSize());
    assert(size <= getMaxSize());

    memset(s, 0x00, sizeof(s));

    pages = (size_t)floorf(float(size) / float(getPageSize()));
    assert(pages >= 2);     /* at least two pages must fit into stack (one page is guard-page) */

    size_ = pages * getPageSize();
    assert(size_ != 0 && size != 0);
    assert(size_ <= size);

#ifdef _WIN32
    vp = VirtualAlloc(0, size_, MEM_COMMIT, PAGE_READWRITE);
    if (!vp)
        return 0;

    VirtualProtect(vp, getPageSize(), PAGE_READWRITE | PAGE_GUARD, &old_options);
#elif _POSIX_VERSION
# if defined(MAP_ANON)
    vp = mmap(0, size_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
# else
    vp = mmap(0, size_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
# endif
    if (vp == MAP_FAILED)
        return 0;

    mprotect(vp, getPageSize(), PROT_NONE);
#endif

    s->sptr = (char*)vp + size_;
    s->ssize = size_;
    return 1;
}

void stack_destroy(stack_t* s)
{
    void* vp;

    assert(s->ssize >= getMinSize());
    assert(s->ssize <= getMaxSize());

    vp = (char*)s->sptr - s->ssize;

#ifdef _WIN32
    VirtualFree(vp, 0, MEM_RELEASE);
#elif _POSIX_VERSION
    munmap(vp, s->ssize);
#endif

    memset(s, 0x00, sizeof(stack_t));
}
