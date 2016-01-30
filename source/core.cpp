#include "core.h"
#include "error_report.h"

#include "bxx/leakcheck_allocator.h"

struct Core
{
    Core()
    {
    }
};

#ifdef _DEBUG
static bx::LeakCheckAllocator gAlloc;
#else
static bx::CrtAllocator gAlloc;
#endif

static Core* gCore = nullptr;

int st::coreInit()
{
    if (gCore) {
        assert(false);
        return -1;
    }

    gCore = BX_NEW(&gAlloc, Core);
    if (!gCore)
        return -1;
        
    return 0;
}

void st::coreShutdown()
{
    if (!gCore) {
        assert(false);
        return;
    }

    BX_DELETE(&gAlloc, gCore);
    gCore = nullptr;
}

bx::AllocatorI* st::coreGetAlloc()
{
    return &gAlloc;
}
