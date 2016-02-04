#define BX_IMPLEMENT_LOGGER
#define STB_LEAKCHECK_IMPLEMENTATION

#include "core.h"
#include "bx/readerwriter.h"
#include "bxx/inifile.h"
#include "bxx/leakcheck_allocator.h"
#include "bx/os.h"

#include "plugins.h"

using namespace st;
using namespace bx;

struct Core
{
    uv_loop_t mainLoop;
    uv_timer_t updateTimer;
    coreFnUpdate updateFn;
    coreConfig conf;
    uv_async_t wakeupReq;
    uv_check_t checkReq;
    uv_prepare_t prepareReq;
    uv_idle_t idleReq;

    Core()
    {
        updateFn = nullptr;
    }
};

#ifdef _DEBUG
static bx::LeakCheckAllocator gAlloc;
#else
static bx::CrtAllocator gAlloc;
#endif

static Core* gCore = nullptr;

namespace libuvMemoryProxy
{
    static void* malloc(size_t size)
    {
        return BX_ALLOC(&gAlloc, size);
    }

    static void* realloc(void* ptr, size_t size)
    {
        return BX_REALLOC(&gAlloc, ptr, size);
    }

    static void free(void* ptr)
    {
        BX_FREE(&gAlloc, ptr);
    }

    static void* calloc(size_t count, size_t size)
    {
        void* p = BX_ALLOC(&gAlloc, size*count);
        if (p)
            memset(p, 0x00, size*count);
        return p;
    }
};

static void update()
{
}

static void callbackConf(const char* key, const char* value, void* userParam)
{
    coreConfig* conf = (coreConfig*)userParam;

    if (bx::stricmp(key, "UpdateInterval") == 0)
        sscanf(value, "%d", &conf->updateInterval);
    else if (bx::stricmp(key, "PluginPath") == 0)
        bx::strlcpy(conf->pluginPath, value, sizeof(conf->pluginPath));
}

coreConfig* st::coreLoadConfig(const char* confFilepath)
{
    assert(gCore);

    coreConfig* conf = BX_NEW(&gAlloc, coreConfig);
    if (!parseIniFile(confFilepath, callbackConf, conf, &gAlloc)) {
        BX_WARN("Loading config file '%s' failed: Loading default config");
    }

    // Sanity checks
    if (conf->updateInterval < 0)
        conf->updateInterval = 0;

    return conf;
}

void st::coreFreeConfig(coreConfig* conf)
{
    assert(conf);

    BX_DELETE(&gAlloc, conf);
}

int st::coreInit(const coreConfig& conf, coreFnUpdate updateFn)
{
    if (gCore) {
        assert(false);
        return ST_ERR_ALREADY_INITIALIZED;
    }

    gCore = BX_NEW(&gAlloc, Core);
    if (!gCore)
        return ST_ERR_OUTOFMEM;

    memcpy(&gCore->conf, &conf, sizeof(gCore->conf));
    bx::enableLogToFileHandle(stdout, stderr);

    gCore->updateFn = updateFn;

    // initialize libuv and Override allocations for libuv
    uv_replace_allocator(libuvMemoryProxy::malloc, libuvMemoryProxy::realloc, libuvMemoryProxy::calloc, 
                         libuvMemoryProxy::free);
    uv_loop_init(&gCore->mainLoop);
    uv_timer_init(&gCore->mainLoop, &gCore->updateTimer);
    uv_async_init(&gCore->mainLoop, &gCore->wakeupReq, nullptr);
    uv_check_init(&gCore->mainLoop, &gCore->checkReq);
    uv_idle_init(&gCore->mainLoop, &gCore->idleReq);
    uv_prepare_init(&gCore->mainLoop, &gCore->prepareReq);

    // Initialize plugins
    if (pluginInit(conf.pluginPath)) {
        ST_ERROR("Core init failed: PluginSystem failed");
        return ST_ERR_FAILED;
    }

    return 0;
}

void st::coreShutdown()
{
    if (!gCore) {
        assert(false);
        return;
    }

    uv_idle_stop(&gCore->idleReq);
    uv_prepare_stop(&gCore->prepareReq);
    uv_check_stop(&gCore->checkReq);
    uv_timer_stop(&gCore->updateTimer);
    uv_stop(&gCore->mainLoop);
    uv_loop_close(&gCore->mainLoop);

    pluginShutdown();

    BX_DELETE(&gAlloc, gCore);
    gCore = nullptr;

#ifdef _DEBUG
    stb_leakcheck_dumpmem();
#endif
}

static void uvFrameTimer(uv_timer_t* handle)
{
    assert(gCore);

    if (gCore->updateFn)
        gCore->updateFn();
}

static void uvFrameIdle(uv_idle_t* handle)
{
    assert(gCore);
    if (gCore->updateFn)
        gCore->updateFn();
}

static void uvFramePrepare(uv_prepare_t* handle)
{
    assert(gCore);
}

static void uvFrameCheck(uv_check_t* handle)
{
    assert(gCore);
}

void st::coreRun()
{
    assert(gCore);

    uv_check_start(&gCore->checkReq, uvFrameCheck);
    uv_prepare_start(&gCore->prepareReq, uvFramePrepare);   // Do update in prepare callback

    // Run the event loop
    if (gCore->conf.updateInterval == 0) {
        uv_idle_start(&gCore->idleReq, uvFrameIdle);
    } else {
        uv_timer_start(&gCore->updateTimer, uvFrameTimer, gCore->conf.updateInterval, gCore->conf.updateInterval);
    }

    uv_run(&gCore->mainLoop, UV_RUN_DEFAULT);
}

uint32_t st::coreGetVersion()
{
    return ST_MAKE_VERSION(0, 1);
}

bx::AllocatorI* st::coreGetAlloc()
{
    return &gAlloc;
}

uv_loop_t* st::coreGetMainLoop()
{
    return &gCore->mainLoop;
}

