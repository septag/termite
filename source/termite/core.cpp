#include "pch.h"

#include "bx/readerwriter.h"
#include "bx/os.h"
#include "bx/cpu.h"
#include "bx/timer.h"
#include "bxx/inifile.h"
#include "bxx/pool.h"
#include "bxx/lock.h"

#include "plugins.h"
#include "gfx_defines.h"
#include "driver_mgr.h"

#define STB_LEAKCHECK_IMPLEMENTATION
#include "bxx/leakcheck_allocator.h"

#define BX_IMPLEMENT_LOGGER
#ifdef termite_SHARED_LIB
#   define BX_SHARED_LIB
#endif
#include "bxx/logger.h"

#include "gfx_render.h"
#include "datastore.h"
#include "datastore_driver.h"
#include "job_dispatcher.h"

#define MEM_POOL_BUCKET_SIZE 256

using namespace termite;

struct FrameData
{
    int64_t frame;
    double frameTime;
    double fps;
    double elapsedTime;
    int64_t lastFrameTick;
};

struct MemoryBlockImpl
{
    termite::MemoryBlock m;
    volatile int32_t refcount;
    bx::AllocatorI* alloc;

    MemoryBlockImpl()
    {
        m.data = nullptr;
        m.size = 0;
        refcount = 1;
        alloc = nullptr;
    }
};

struct Core
{
    coreFnUpdate updateFn;
    coreConfig conf;
    gfxRender* renderer;
    FrameData frameData;
    double hpTimerFreq;
    bx::Pool<MemoryBlockImpl> memPool;
    bx::Lock memPoolLock;
    dsDataStore* dataStore;
    dsDriver* dataDriver;
    dsDriver* blockingDataDriver;

    Core()
    {
        updateFn = nullptr;
        renderer = nullptr;
        hpTimerFreq = 0;
        dataStore = nullptr;
        dataDriver = nullptr;
        blockingDataDriver = nullptr;
        memset(&frameData, 0x00, sizeof(frameData));
    }
};

#ifdef _DEBUG
static bx::LeakCheckAllocator g_allocStub;
#else
static bx::CrtAllocator g_allocStub;
#endif
static bx::AllocatorI* g_alloc = &g_allocStub;

static Core* g_core = nullptr;

static void callbackConf(const char* key, const char* value, void* userParam)
{
    coreConfig* conf = (coreConfig*)userParam;

    if (bx::stricmp(key, "Plugin_Path") == 0)
        bx::strlcpy(conf->pluginPath, value, sizeof(conf->pluginPath));
    else if (bx::stricmp(key, "gfx_DeviceId") == 0)
        sscanf(value, "%hu", &conf->gfxDeviceId);
    else if (bx::stricmp(key, "gfx_Width") == 0)
        sscanf(value, "%hu", &conf->gfxWidth);
    else if (bx::stricmp(key, "gfx_Height") == 0)
        sscanf(value, "%hu", &conf->gfxHeight);
    else if (bx::stricmp(key, "gfx_VSync") == 0)
        conf->gfxDriverFlags |= bx::toBool(value) ? uint32_t(gfxResetFlag::VSync) : 0;
}

coreConfig* termite::coreLoadConfig(const char* confFilepath)
{
    assert(g_core);

    coreConfig* conf = BX_NEW(g_alloc, coreConfig);
    if (!parseIniFile(confFilepath, callbackConf, conf, g_alloc)) {
        BX_WARN("Loading config file '%s' failed: Loading default config");
    }

    return conf;
}

void termite::coreFreeConfig(coreConfig* conf)
{
    assert(conf);

    BX_DELETE(g_alloc, conf);
}

int termite::coreInit(const coreConfig& conf, coreFnUpdate updateFn, const gfxPlatformData* platform)
{
    if (g_core) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_core = BX_NEW(g_alloc, Core);
    if (!g_core)
        return T_ERR_OUTOFMEM;

    memcpy(&g_core->conf, &conf, sizeof(g_core->conf));

    g_core->updateFn = updateFn;

    // Error handler
    if (errInit(g_alloc)) {
        return T_ERR_FAILED;
    }

    // Memory pool for MemoryBlock objects
    if (!g_core->memPool.create(MEM_POOL_BUCKET_SIZE, g_alloc))
        return T_ERR_OUTOFMEM;

    // Initialize Driver server
    if (drvInit()) {
        T_ERROR("Core init failed: Driver Manager failed");
        return T_ERR_FAILED;
    }

    // Initialize plugins
    if (pluginInit(conf.pluginPath)) {
        T_ERROR("Core init failed: PluginSystem failed");
        return T_ERR_FAILED;
    }

    // Initialize and blocking data driver for permanent resources
    drvHandle blockingDataDriverHandle = drvFindHandleByName("BlockingDiskDriver");
    if (blockingDataDriverHandle == STDRV_INVALID_HANDLE) {
        T_ERROR("Core init failed: Could not find a blocking disk driver");
        return T_ERR_FAILED;
    }
    g_core->blockingDataDriver = drvGetDataStoreDriver(blockingDataDriverHandle);
    if (g_core->blockingDataDriver) {
        char curPath[256];
        bx::pwd(curPath, sizeof(curPath));
        if (g_core->blockingDataDriver->init(g_alloc, curPath, nullptr)) {
            T_ERROR("Core init failed: Initializing BlockingDiskDriver failed");
            return T_ERR_FAILED;
        }
    }

    // Initialize DataStore with the proper driver
    drvHandle dataDriverHandle = drvFindHandleByName("AsyncDiskDriver");
    if (dataDriverHandle == STDRV_INVALID_HANDLE) {
        T_ERROR("Core init failed: Could not find disk driver");
        return T_ERR_FAILED;
    }
    g_core->dataDriver = drvGetDataStoreDriver(dataDriverHandle);
    if (g_core->dataDriver) {
        char curPath[256];
        bx::pwd(curPath, sizeof(curPath));
        if (g_core->dataDriver->init(g_alloc, curPath, nullptr)) {
            T_ERROR("Core init failed: Initializing AsyncDiskDriver failed");
            return T_ERR_FAILED;
        }
    }

    g_core->dataStore = dsCreate(dsInitFlag::HotLoading, g_core->dataDriver);
    if (!g_core->dataStore) {
        T_ERROR("Core init failed: Creating default DataStore failed");
        return T_ERR_FAILED;
    }

    // Find Renderer plugins and run them
    drvHandle renderDriverHandle;
    int r = drvFindHandlesByType(drvType::Renderer, &renderDriverHandle, 1);
    if (platform && r) {
        g_core->renderer = drvGetRenderer(renderDriverHandle);

        BX_TRACE("Found Renderer: %s v%d.%d", drvGetName(renderDriverHandle), 
                 T_VERSION_MAJOR(drvGetVersion(renderDriverHandle)), T_VERSION_MINOR(drvGetVersion(renderDriverHandle)));

        drvHandle graphicsDriver;
        r = drvFindHandlesByType(drvType::GraphicsDriver, &graphicsDriver, 1);
        if (!r) {
            T_ERROR("No Graphics driver found");
            return T_ERR_FAILED;
        }
        
        BX_TRACE("Found Graphics Driver: %s v%d.%d", drvGetName(graphicsDriver),
                 T_VERSION_MAJOR(drvGetVersion(graphicsDriver)), T_VERSION_MINOR(drvGetVersion(graphicsDriver)));
        if (g_core->renderer->init(g_alloc, drvGetGraphicsDriver(graphicsDriver), platform, conf.keymap)) {
            T_ERROR("Renderer Init failed");
            return T_ERR_FAILED;
        }
    }

    // Initialize job dispatcher
    if (jobInit(g_alloc, 0, 0, 0, 0, false)) {
        BX_FATAL("Job Dispatcher init failed");
        return T_ERR_FAILED;
    }

    g_core->hpTimerFreq = bx::getHPFrequency();

    return 0;
}

void termite::coreShutdown()
{
    if (!g_core) {
        assert(false);
        return;
    }

    jobShutdown();

    if (g_core->renderer) {
        g_core->renderer->shutdown();
        g_core->renderer = nullptr;
    }

    if (g_core->dataStore) {
        dsDestroy(g_core->dataStore);
        g_core->dataStore = nullptr;
    }

    if (g_core->dataDriver) {
        g_core->dataDriver->shutdown();
        g_core->dataDriver = nullptr;
    }

    if (g_core->blockingDataDriver) {
        g_core->blockingDataDriver->shutdown();
        g_core->blockingDataDriver = nullptr;
    }

    pluginShutdown();
    drvShutdown();

    g_core->memPool.destroy();

    errShutdown();
    BX_DELETE(g_alloc, g_core);
    g_core = nullptr;

#ifdef _DEBUG
    stb_leakcheck_dumpmem();
#endif
}

void termite::coreFrame()
{
    FrameData& fd = g_core->frameData;

    if (fd.lastFrameTick == 0)
        fd.lastFrameTick = bx::getHPCounter();

    int64_t frameTick = bx::getHPCounter();
    double dt = double(frameTick - fd.lastFrameTick)/g_core->hpTimerFreq;

    if (g_core->updateFn)
        g_core->updateFn((float)dt);

    if (g_core->dataDriver)
        g_core->dataDriver->runAsyncLoop();

    if (g_core->renderer) {
        g_core->renderer->frame();
        g_core->renderer->render();
    }

    fd.frame++;
    fd.elapsedTime += dt;
    fd.frameTime = dt;
    fd.fps = (double)fd.frame / fd.elapsedTime;
    fd.lastFrameTick = frameTick;
}

uint32_t termite::coreGetVersion()
{
    return T_MAKE_VERSION(0, 1);
}

bx::AllocatorI* termite::coreGetAlloc() T_THREAD_SAFE
{
    return g_alloc;
}

const coreConfig& termite::coreGetConfig()
{
    return g_core->conf;
}

dsDataStore* termite::coreGetDefaultDataStore()
{
    return g_core->dataStore;
}

double termite::coreGetFrameTime()
{
    return g_core->frameData.frameTime;
}

double termite::coreGetElapsedTime()
{
    return g_core->frameData.elapsedTime;
}

double termite::coreGetFps()
{
    return g_core->frameData.fps;
}

termite::MemoryBlock* termite::coreCreateMemory(uint32_t size, bx::AllocatorI* alloc)
{
    g_core->memPoolLock.lock();
    MemoryBlockImpl* mem = g_core->memPool.newInstance();
    g_core->memPoolLock.unlock();
    if (!alloc)
        alloc = g_alloc;
    mem->m.data = (uint8_t*)BX_ALLOC(alloc, size);
    if (!mem->m.data)
        return nullptr;
    mem->m.size = size;
    mem->alloc = alloc;
    return (termite::MemoryBlock*)mem;
}

termite::MemoryBlock* termite::coreRefMemoryPtr(void* data, uint32_t size)
{
    g_core->memPoolLock.lock();
    MemoryBlockImpl* mem = g_core->memPool.newInstance();
    g_core->memPoolLock.unlock();
    mem->m.data = (uint8_t*)data;
    mem->m.size = size;
    return (MemoryBlock*)mem;
}

termite::MemoryBlock* termite::coreCopyMemory(const void* data, uint32_t size, bx::AllocatorI* alloc)
{
    g_core->memPoolLock.lock();
    MemoryBlockImpl* mem = g_core->memPool.newInstance();
    g_core->memPoolLock.unlock();
    if (!alloc)
        alloc = g_alloc;
    mem->m.data = (uint8_t*)BX_ALLOC(alloc, size);
    if (!mem->m.data)
        return nullptr;
    memcpy(mem->m.data, data, size);
    mem->m.size = size; 
    mem->alloc = alloc;
    return (MemoryBlock*)mem;
}

termite::MemoryBlock* termite::coreRefMemory(termite::MemoryBlock* mem)
{
    MemoryBlockImpl* m = (MemoryBlockImpl*)mem;
    bx::atomicFetchAndAdd(&m->refcount, 1);
    return mem;
}

void termite::coreReleaseMemory(termite::MemoryBlock* mem)
{
    MemoryBlockImpl* m = (MemoryBlockImpl*)mem;
    if (bx::atomicDec(&m->refcount) == 0) {
        if (m->alloc) {
            BX_FREE(m->alloc, m->m.data);
            m->m.data = nullptr;
            m->m.size = 0;
        }

        bx::LockScope(g_core->memPoolLock);
        g_core->memPool.deleteInstance(m);
    }
}

termite::MemoryBlock* termite::coreReadFile(const char* filepath)
{
    if (!g_core->blockingDataDriver)
        return nullptr;

    return g_core->blockingDataDriver->read(filepath);
}

void termite::coreSendInputChars(const char* chars)
{
    if (g_core->renderer)
        g_core->renderer->sendImInputChars(chars);
}

void termite::coreSendInputKeys(const bool keysDown[512], bool shift, bool alt, bool ctrl)
{
    if (g_core->renderer)
        g_core->renderer->sendImInputKeys(keysDown, shift, alt, ctrl);
}

void termite::coreSendInputMouse(float mousePos[2], int mouseButtons[3], float mouseWheel)
{
    if (g_core->renderer) {
        g_core->renderer->sendImInputMouse(mousePos, mouseButtons, mouseWheel);
    }
}

