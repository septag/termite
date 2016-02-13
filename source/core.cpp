#include "core.h"

#include "bx/readerwriter.h"
#include "bxx/inifile.h"
#include "bx/os.h"

#include "plugins.h"
#include "driver_server.h"

#define STB_LEAKCHECK_IMPLEMENTATION
#include "bxx/leakcheck_allocator.h"

#define BX_IMPLEMENT_LOGGER
#ifdef STENGINE_SHARED_LIB
#   define BX_SHARED_LIB
#endif
#include "bxx/logger.h"

#include "gfx_render.h"

using namespace st;
using namespace bx;

struct Core
{
    coreFnUpdate updateFn;
    coreConfig conf;
    gfxRender* renderer;

    Core()
    {
        updateFn = nullptr;
        renderer = nullptr;
    }
};

#ifdef _DEBUG
static bx::LeakCheckAllocator gAlloc;
#else
static bx::CrtAllocator gAlloc;
#endif

static Core* gCore = nullptr;

static void callbackConf(const char* key, const char* value, void* userParam)
{
    coreConfig* conf = (coreConfig*)userParam;

    if (bx::stricmp(key, "PluginPath") == 0)
        bx::strlcpy(conf->pluginPath, value, sizeof(conf->pluginPath));
    else if (bx::stricmp(key, "gfxDeviceId") == 0)
        sscanf(value, "%hu", &conf->gfxDeviceId);
    else if (bx::stricmp(key, "gfxWidth") == 0)
        sscanf(value, "%hu", &conf->gfxWidth);
    else if (bx::stricmp(key, "gfxHeight") == 0)
        sscanf(value, "%hu", &conf->gfxHeight);
}

coreConfig* st::coreLoadConfig(const char* confFilepath)
{
    assert(gCore);

    coreConfig* conf = BX_NEW(&gAlloc, coreConfig);
    if (!parseIniFile(confFilepath, callbackConf, conf, &gAlloc)) {
        BX_WARN("Loading config file '%s' failed: Loading default config");
    }

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

    gCore->updateFn = updateFn;

    // Initialize Driver server
    if (drvInit()) {
        ST_ERROR("Core init failed: Driver Server failed");
        return ST_ERR_FAILED;
    }

    // Initialize plugins
    if (pluginInit(conf.pluginPath)) {
        ST_ERROR("Core init failed: PluginSystem failed");
        return ST_ERR_FAILED;
    }

    // Find Renderer plugins and run them
    drvDriver* rendererDriver;
    int r = drvFindHandlesByType(drvType::Renderer, &rendererDriver, 1);
    if (r) {
        gCore->renderer = drvGetRenderer(rendererDriver);

        BX_TRACE("Found Renderer: %s v%d.%d", drvGetName(rendererDriver), 
                 ST_VERSION_MAJOR(drvGetVersion(rendererDriver)), ST_VERSION_MINOR(drvGetVersion(rendererDriver)));

        drvDriver* graphicsDriver;
        r = drvFindHandlesByType(drvType::GraphicsDriver, &graphicsDriver, 1);
        if (!r) {
            ST_ERROR("No Graphics driver found");
            return ST_ERR_FAILED;
        }
        
        BX_TRACE("Found Graphics Driver: %s v%d.%d", drvGetName(graphicsDriver),
                 ST_VERSION_MAJOR(drvGetVersion(graphicsDriver)), ST_VERSION_MINOR(drvGetVersion(graphicsDriver)));
        gCore->renderer->init(&gAlloc, drvGetGraphics(graphicsDriver), conf.sdlWindow);
    }

    return 0;
}

void st::coreShutdown()
{
    if (!gCore) {
        assert(false);
        return;
    }

    if (gCore->renderer) {
        gCore->renderer->shutdown();
        gCore->renderer = nullptr;
    }

    pluginShutdown();
    drvShutdown();

    BX_DELETE(&gAlloc, gCore);
    gCore = nullptr;

#ifdef _DEBUG
    stb_leakcheck_dumpmem();
#endif
}

void st::coreFrame()
{
    if (gCore->updateFn)
        gCore->updateFn();

    if (gCore->renderer)
        gCore->renderer->render();
}

uint32_t st::coreGetVersion()
{
    return ST_MAKE_VERSION(0, 1);
}

bx::AllocatorI* st::coreGetAlloc()
{
    return &gAlloc;
}

const coreConfig& st::coreGetConfig()
{
    return gCore->conf;
}

