#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
#else
# include <time.h>
#endif

#include <cstdio>

#include "fcontext/fcontext.h"

fcontext_t ctx;
fcontext_t ctx2;

inline void sleep(uint32_t _ms)
{
#ifdef _WIN32
    Sleep(_ms);
#else
    timespec req = { (time_t)_ms / 1000, (long)((_ms % 1000) * 1000000) };
    timespec rem = { 0, 0 };
    nanosleep(&req, &rem);
#endif
}

static void doo(transfer_t t)
{
    puts("DOO");
    sleep(1000);
    jump_fcontext(t.ctx, NULL);
}

static void foo(transfer_t t)
{
    puts("FOO");
    sleep(1000);
    jump_fcontext(ctx2, NULL);
    puts("FOO 2");
    sleep(1000);
    jump_fcontext(t.ctx, NULL);
}

int main()
{
    contextstack_t s;
    stack_create(&s, 16*1024);

    contextstack_t s2;
    stack_create(&s2);

    ctx = make_fcontext(s.sptr, s.ssize, foo);
    ctx2 = make_fcontext(s2.sptr, s2.ssize, doo);

    jump_fcontext(ctx, NULL);
    puts("END");

    stack_destroy(&s);
    stack_destroy(&s2);
    return 0;
}