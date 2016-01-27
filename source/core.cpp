#include "core.h"

struct Core
{
    bx::AllocatorI* alloc;

    Core()
    {
        alloc = nullptr;
    }
};

static Core* gCore = nullptr;

int st::coreInit()
{
    if (!gCore)
        return 0;

}

void st::coreShutdown()
{

}

bx::AllocatorI* st::coreGetAlloc()
{

}
