#include "pch.h"

#include "bx/readerwriter.h"
#include "bx/os.h"
#include "bx/cpu.h"
#include "bx/timer.h"
#include "bxx/path.h"
#include "bxx/inifile.h"
#include "bxx/pool.h"
#include "bxx/lock.h"
#include "bx/crtimpl.h"

#include "gfx_defines.h"
#include "gfx_font.h"
#include "gfx_utils.h"
#include "gfx_texture.h"
#include "gfx_vg.h"
#include "gfx_debugdraw.h"
#include "gfx_model.h"
#include "gfx_render.h"
#include "gfx_sprite.h"
#include "resource_lib.h"
#include "io_driver.h"
#include "job_dispatcher.h"
#include "memory_pool.h"
#include "plugin_system.h"
#include "component_system.h"
#include "event_dispatcher.h"
#include "physics_2d.h"
#include "command_system.h"

#ifdef termite_SDL2
#  include <SDL.h>
#  include "sdl_utils.h"
#endif

#include "../imgui_impl/imgui_impl.h"

#define STB_LEAKCHECK_IMPLEMENTATION
#define STB_LEAKCHECK_MULTITHREAD
#include "bxx/leakcheck_allocator.h"

#include "bxx/path.h"

#define BX_IMPLEMENT_LOGGER
#ifdef termite_SHARED_LIB
#   define BX_SHARED_LIB
#endif
#include "bxx/logger.h"

#define BX_IMPLEMENT_JSON
#include "bxx/json.h"

#include <dirent.h>
#include <random>

#define MEM_POOL_BUCKET_SIZE 256
#define IMGUI_VIEWID 255
#define NANOVG_VIEWID 254
#define LOG_STRING_SIZE 256

using namespace termite;
//
struct FrameData
{
    int64_t frame;
    double frameTime;
    double fps;
    double elapsedTime;
    double avgFrameTime;
    int64_t lastFrameTick;
    double frameTimes[32];
    double fpsTime;
};

struct HeapMemoryImpl
{
    termite::MemoryBlock m;
    volatile int32_t refcount;
    bx::AllocatorI* alloc;

    HeapMemoryImpl()
    {
        m.data = nullptr;
        m.size = 0;
        refcount = 1;
        alloc = nullptr;
    }
};

class GfxDriverEvents : public GfxDriverEventsI
{
private:
    bx::Lock m_lock;

public:
    void onFatal(GfxFatalType::Enum type, const char* str) override;
    void onTraceVargs(const char* filepath, int line, const char* format, va_list argList) override;

    uint32_t onCacheReadSize(uint64_t id) override
    {
        return 0;
    }

    bool onCacheRead(uint64_t id, void* data, uint32_t size) override
    {
        return false;
    }

    void onCacheWrite(uint64_t id, const void* data, uint32_t size) override
    {
    }

    void onScreenShot(const char *filePath, uint32_t width, uint32_t height, uint32_t pitch, 
                      const void *data, uint32_t size, bool yflip) override
    {
    }

    void onCaptureBegin(uint32_t width, uint32_t height, uint32_t pitch, TextureFormat::Enum fmt, bool yflip) override
    {
    }

    void onCaptureEnd() override
    {
    }

    void onCaptureFrame(const void* data, uint32_t size) override
    {
    }
};

struct LogCache
{
    bx::LogType::Enum type;
    char text[LOG_STRING_SIZE];
};

struct Core
{
    UpdateCallback updateFn;
    Config conf;
    RendererApi* renderer;
    FrameData frameData;
    double hpTimerFreq;
    double timeMultiplier;
    bx::Pool<HeapMemoryImpl> memPool;
    bx::Lock memPoolLock;
    GfxDriverApi* gfxDriver;
    IoDriverDual* ioDriver;
    PhysDriver2DApi* phys2dDriver;
    PageAllocator tempAlloc;
    GfxDriverEvents gfxDriverEvents;
    LogCache* gfxLogCache;
    int numGfxLogCache;

    std::random_device randDevice;
    std::mt19937 randEngine;
    bool init;

    Core() :
        tempAlloc(T_MID_TEMP),
        randEngine(randDevice())
    {
        gfxDriver = nullptr;
        phys2dDriver = nullptr;
        updateFn = nullptr;
        renderer = nullptr;
        hpTimerFreq = 0;
        ioDriver = nullptr;
        timeMultiplier = 1.0;
        gfxLogCache = nullptr;
        numGfxLogCache = 0;
        init = false;
        memset(&frameData, 0x00, sizeof(frameData));
    }
};

#ifdef _DEBUG
static bx::LeakCheckAllocator g_allocStub;
#else
static bx::CrtAllocator g_allocStub;
#endif
static bx::AllocatorI* g_alloc = &g_allocStub;

static bx::Path g_dataDir;
static bx::Path g_cacheDir;
static Core* g_core = nullptr;

#if BX_PLATFORM_ANDROID
#include <jni.h>
extern "C" JNIEXPORT void JNICALL Java_com_termite_utils_PlatformUtils_termiteInitPaths(JNIEnv* env, jclass cls, 
                                                                                        jstring dataDir, jstring cacheDir)
{
    BX_UNUSED(cls);
    
    const char* str = env->GetStringUTFChars(dataDir, nullptr);
    bx::strlcpy(g_dataDir.getBuffer(), str, sizeof(g_dataDir));
    env->ReleaseStringUTFChars(dataDir, str);

    str = env->GetStringUTFChars(cacheDir, nullptr);
    bx::strlcpy(g_cacheDir.getBuffer(), str, sizeof(g_cacheDir));
    env->ReleaseStringUTFChars(cacheDir, str);
}
#endif

static void callbackConf(const char* key, const char* value, void* userParam)
{
    Config* conf = (Config*)userParam;

    if (bx::stricmp(key, "Plugin_Path") == 0)
        bx::strlcpy(conf->pluginPath, value, sizeof(conf->pluginPath));
    else if (bx::stricmp(key, "gfx_DeviceId") == 0)
        sscanf(value, "%hu", &conf->gfxDeviceId);
    else if (bx::stricmp(key, "gfx_Width") == 0)
        sscanf(value, "%hu", &conf->gfxWidth);
    else if (bx::stricmp(key, "gfx_Height") == 0)
        sscanf(value, "%hu", &conf->gfxHeight);
    else if (bx::stricmp(key, "gfx_VSync") == 0)
        conf->gfxDriverFlags |= bx::toBool(value) ? uint32_t(GfxResetFlag::VSync) : 0;
}

Config* termite::loadConfig(const char* confFilepath)
{
    assert(g_core);

    Config* conf = BX_NEW(g_alloc, Config);
    if (!parseIniFile(confFilepath, callbackConf, conf, g_alloc)) {
        BX_WARN("Loading config file '%s' failed: Loading default config");
    }

    return conf;
}

void termite::freeConfig(Config* conf)
{
    assert(conf);

    BX_DELETE(g_alloc, conf);
}

static void loadFontsInDirectory(const char* baseDir, const char* rootDir)
{
    BX_VERBOSE("Scanning for fonts in directory '%s' ...", rootDir);
    bx::Path fullDir(baseDir);
    fullDir.join(rootDir);

    DIR* dir = opendir(fullDir.cstr());
    if (!dir) {
        BX_WARN("Could not open fonts directory '%s'", rootDir);
        return;
    }

    dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type == DT_REG) {
            bx::Path fntFilepath(rootDir);
            fntFilepath.join(ent->d_name);
            if (fntFilepath.getFileExt().isEqualNoCase("fnt"))
                registerFont(fntFilepath.cstr(), fntFilepath.getFilename().cstr());
        }
    }
    closedir(dir);

    return;
}

result_t termite::initialize(const Config& conf, UpdateCallback updateFn, const GfxPlatformData* platform)
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

    // Set Data and Cache Dir paths
#if !BX_PLATFORM_ANDROID
    bx::strlcpy(g_dataDir.getBuffer(), conf.dataUri, sizeof(g_dataDir));
    g_dataDir.normalizeSelf();
    g_cacheDir = bx::getTempDir();        
#endif

    // Error handler
    if (initErrorReport(g_alloc)) {
        return T_ERR_FAILED;
    }

    // Memory pool for MemoryBlock objects
    if (!g_core->memPool.create(MEM_POOL_BUCKET_SIZE, g_alloc))
        return T_ERR_OUTOFMEM;

    if (initMemoryPool(g_alloc, conf.pageSize*1024, conf.maxPagesPerPool))
        return T_ERR_OUTOFMEM;

    // Initialize plugins system and enumerate plugins
    if (initPluginSystem(conf.pluginPath, g_alloc)) {
        T_ERROR("Engine init failed: PluginSystem failed");
        return T_ERR_FAILED;
    }

    int r;
    PluginHandle pluginHandle;

    // IO
#if BX_PLATFORM_ANDROID
    const char* ioDriverName = "AssetIO";
#elif BX_PLATFORM_IOS
    const char* ioDriverName = "DiskIO_Lite";
#else
    const char* ioDriverName = "DiskIO";
#endif
    r = findPluginByName(conf.ioName[0] ? conf.ioName : ioDriverName , 0, &pluginHandle, 1, PluginType::IoDriver);
    if (r > 0) {
        g_core->ioDriver = (IoDriverDual*)initPlugin(pluginHandle, g_alloc);
        if (!g_core->ioDriver) {
            T_ERROR("Engine init failed: Could not find IO driver");
            return T_ERR_FAILED;
        }

        // Initialize IO
        // If data path is chosen, set is as root path
        // If not, use the current directory as the root path
        char curPath[256];
        const char* uri;
        if (conf.dataUri[0]) {
            uri = conf.dataUri;
        } else {
            bx::pwd(curPath, sizeof(curPath));
            uri = curPath;
        }

        const PluginDesc& desc = getPluginDesc(pluginHandle);
        BX_BEGINP("Initializing IO Driver: %s v%d.%d", desc.name, T_VERSION_MAJOR(desc.version), T_VERSION_MINOR(desc.version));
        if (T_FAILED(g_core->ioDriver->blocking->init(g_alloc, uri, nullptr, nullptr)) ||
            T_FAILED(g_core->ioDriver->async->init(g_alloc, uri, nullptr, nullptr))) 
        {
            BX_END_FATAL();
            T_ERROR("Engine init failed: Initializing IoDriver failed");
            return T_ERR_FAILED;
        }
        BX_END_OK();
    }

    if (!g_core->ioDriver) {
        T_ERROR("Engine init failed: No IoDriver is detected");
        return T_ERR_FAILED;
    }

    BX_BEGINP("Initializing Resource Library");
    if (T_FAILED(initResourceLib(BX_ENABLED(termite_DEV) ? ResourceLibInitFlag::HotLoading : 0, g_core->ioDriver->async, g_alloc))) 
    {
        T_ERROR("Core init failed: Creating default ResourceLib failed");
        return T_ERR_FAILED;
    }
    BX_END_OK();

    // Renderer
    if (conf.rendererName[0] != 0) {
        r = findPluginByName(conf.rendererName, 0, &pluginHandle, 1, PluginType::Renderer);
        if (r > 0) {
            g_core->renderer = (RendererApi*)initPlugin(pluginHandle, g_alloc);
            const PluginDesc& desc = getPluginDesc(pluginHandle);
            BX_TRACE("Found Renderer: %s v%d.%d", desc.name, T_VERSION_MAJOR(desc.version), T_VERSION_MINOR(desc.version));

            if (!platform) {
                T_ERROR("Core init failed: PlatformData is not provided for Renderer");
                return T_ERR_FAILED;
            }
        } 
    }

    // Graphics Device
    if (conf.gfxName[0] != 0)    {
        r = findPluginByName(conf.gfxName, 0, &pluginHandle, 1, PluginType::GraphicsDriver);
        if (r > 0) {
            g_core->gfxDriver = (GfxDriverApi*)initPlugin(pluginHandle, g_alloc);
        }

        if (!g_core->gfxDriver) {
            T_ERROR("Core init failed: Could not detect Graphics driver: %s", conf.gfxName);
            return T_ERR_FAILED;
        }

        const PluginDesc& desc = getPluginDesc(pluginHandle);
        BX_BEGINP("Initializing Graphics Driver: %s v%d.%d", desc.name, T_VERSION_MAJOR(desc.version),
                  T_VERSION_MINOR(desc.version));
        if (platform)
            g_core->gfxDriver->setPlatformData(*platform);
        if (T_FAILED(g_core->gfxDriver->init(conf.gfxDeviceId, &g_core->gfxDriverEvents, g_alloc))) {
            BX_END_FATAL();
            dumpGfxLog();
            T_ERROR("Core init failed: Could not initialize Graphics driver");
            return T_ERR_FAILED;
        }
        BX_END_OK();
        dumpGfxLog();

        // Initialize Renderer with Gfx Driver
        if (g_core->renderer) {
            BX_BEGINP("Initializing Renderer");
            if (T_FAILED(g_core->renderer->init(g_alloc, g_core->gfxDriver))) {
                BX_END_FATAL();
                T_ERROR("Core init failed: Could not initialize Renderer");
                return T_ERR_FAILED;
            }
            BX_END_OK();
        }

        // Init and Register graphics resource loaders
        initTextureLoader(g_core->gfxDriver, g_alloc);
        registerTextureToResourceLib();

        initModelLoader(g_core->gfxDriver, g_alloc);
        registerModelToResourceLib();

        // Font library
        if (initFontLib(g_alloc, g_core->ioDriver->blocking)) {
            T_ERROR("Initializing font library failed");
            return T_ERR_FAILED;
        }

        if ((conf.engineFlags & InitEngineFlags::ScanFontsDirectory) == InitEngineFlags::ScanFontsDirectory)
            loadFontsInDirectory(g_core->ioDriver->blocking->getUri(), "fonts");

        // VectorGraphics
        if (initVectorGfx(g_alloc, g_core->gfxDriver)) {
            T_ERROR("Initializing Vector Graphics failed");
            return T_ERR_FAILED;
        }

        // Debug graphics
        if (initDebugDraw(g_alloc, g_core->gfxDriver)) {
            T_ERROR("Initializing Editor Draw failed");
            return T_ERR_FAILED;
        }

        // Graphics Utilities
        if (initGfxUtils(g_core->gfxDriver)) {
            T_ERROR("Initializing Graphics Utilities failed");
            return T_ERR_FAILED;
        }

        // ImGui initialize
        if (T_FAILED(initImGui(IMGUI_VIEWID, conf.gfxWidth, conf.gfxHeight, g_core->gfxDriver, g_alloc, conf.keymap,
                               conf.uiIniFilename, platform ? platform->nwh : nullptr))) {
            T_ERROR("Initializing ImGui failed");
            return T_ERR_FAILED;
        }

        if (T_FAILED(initSpriteSystem(g_core->gfxDriver, g_alloc))) {
            T_ERROR("Initializing Sprite System failed");
            return T_ERR_FAILED;
        }
        registerSpriteSheetToResourceLib();
    }

    // Physics2D Driver
    if (conf.phys2dName[0] != 0) {
        r = findPluginByName(conf.phys2dName, 0, &pluginHandle, 1, PluginType::Physics2dDriver);
        if (r > 0) {
            g_core->phys2dDriver = (PhysDriver2DApi*)initPlugin(pluginHandle, g_alloc);
        }

        if (!g_core->phys2dDriver) {
            T_ERROR("Core init failed: Could not detect Physics driver: %s", conf.phys2dName);
            return T_ERR_FAILED;
        }

        const PluginDesc& desc = getPluginDesc(pluginHandle);
        BX_BEGINP("Initializing Physics2D Driver: %s v%d.%d", desc.name, T_VERSION_MAJOR(desc.version),
                  T_VERSION_MINOR(desc.version));
        if (T_FAILED(g_core->phys2dDriver->init(g_alloc, BX_ENABLED(termite_DEV) ? PhysFlags2D::EnableDebug : 0,
                                                NANOVG_VIEWID))) 
        {
            BX_END_FATAL();
            T_ERROR("Core init failed: Could not initialize Physics2D driver");
            return T_ERR_FAILED;
        }
        BX_END_OK();
    }

    // Job Dispatcher
    if ((conf.engineFlags & InitEngineFlags::EnableJobDispatcher) == InitEngineFlags::EnableJobDispatcher) {
        BX_BEGINP("Initializing Job Dispatcher");
        if (initJobDispatcher(g_alloc, conf.maxSmallFibers, conf.smallFiberSize*1024, conf.maxBigFibers, 
                              conf.bigFiberSize*1024, 
                              (conf.engineFlags & InitEngineFlags::LockThreadsToCores) == InitEngineFlags::LockThreadsToCores)) 
        {
            T_ERROR("Core init failed: Job Dispatcher init failed");
            BX_END_FATAL();
            return T_ERR_FAILED;
        }
        BX_END_OK();
        BX_TRACE("%d Worker threads spawned", getNumWorkerThreads());
    }

	// Component System
	BX_BEGINP("Initializing Component System");
	if (T_FAILED(initComponentSystem(g_alloc))) {
		T_ERROR("Core init failed: Could not initialize Component-System");
		BX_END_FATAL();
		return T_ERR_FAILED;
	}
	BX_END_OK();

    BX_BEGINP("Initializing Event Dispatcher");
    if (T_FAILED(initEventDispatcher(g_alloc))) {
        T_ERROR("Core init fialed: Could not initialize Event Dispatcher");
        BX_END_FATAL();
        return T_ERR_FAILED;
    }
    BX_END_OK();

#ifdef termite_SDL2
    BX_BEGINP("Initializing SDL2 utils");
    if (T_FAILED(initSdlUtils(g_alloc))) {
        T_ERROR("Core init failed: Could not initialize SDL2 utils");
        BX_END_FATAL();
        return T_ERR_FAILED;
    }
    BX_END_OK();
#endif

#if termite_DEV
    BX_BEGINP("Initializing Command System");
    if (T_FAILED(initCommandSystem(conf.cmdHistorySize, g_alloc))) {
        T_ERROR("Core init failed: Could not initialize Command System");
        BX_END_FATAL();
        return T_ERR_FAILED;
    }
    BX_END_OK();
#endif

    g_core->hpTimerFreq = bx::getHPFrequency();
    g_core->init = true;

    return 0;
}

void termite::shutdown(ShutdownCallback callback, void* userData)
{
    if (!g_core) {
        assert(false);
        return;
    }

#if termite_DEV
    BX_BEGINP("Shutting down Command System");
    shutdownCommandSystem();
    BX_END_OK();
#endif

#ifdef termite_SDL2
    BX_BEGINP("Shutting down SDL2 utils");
    shutdownSdlUnits();
    BX_END_OK();
#endif

    BX_BEGINP("Shutting down Event Dispatcher");
    shutdownEventDispatcher();
    BX_END_OK();

	BX_BEGINP("Shutting down Component System");
	shutdownComponentSystem();
	BX_END_OK();

	BX_BEGINP("Shutting down Job Dispatcher");
    shutdownJobDispatcher();
	BX_END_OK();

    if (g_core->phys2dDriver) {
        BX_BEGINP("Shutting down Physics2D Driver");
        g_core->phys2dDriver->shutdown();
        g_core->phys2dDriver = nullptr;
        BX_END_OK();
    }

    BX_BEGINP("Shutting down Graphics Subsystems");
    shutdownSpriteSystem();
	shutdownImGui();
    shutdownDebugDraw();
    shutdownVectorGfx();
    shutdownFontLib();
    shutdownModelLoader();
    shutdownTextureLoader();
    shutdownGfxUtils();
    BX_END_OK();

    if (g_core->renderer) {
        BX_BEGINP("Shutting down Renderer");
        g_core->renderer->shutdown();
        g_core->renderer = nullptr;
        BX_END_OK();
    }

    if (g_core->gfxDriver) {
        BX_BEGINP("Shutting down Graphics Driver");
        g_core->gfxDriver->shutdown();
        g_core->gfxDriver = nullptr;
        BX_END_OK();
        dumpGfxLog();
    }

    shutdownResourceLib();
    
    // User Shutdown happens before IO and memory stuff
    // In order for user to clean-up any memory or save stuff
    if (callback) {
        callback(userData);
    }

    if (g_core->ioDriver) {
        BX_BEGINP("Shutting down IO Driver");
        g_core->ioDriver->blocking->shutdown();
        g_core->ioDriver->async->shutdown();
        g_core->ioDriver = nullptr;
        BX_END_OK();
    }

    BX_BEGINP("Shutting down Plugin system");
    shutdownPluginSystem();
    BX_END_OK();

    if (g_core->gfxLogCache) {
        BX_FREE(g_alloc, g_core->gfxLogCache);
        g_core->gfxLogCache = nullptr;
        g_core->numGfxLogCache = 0;
    }

    BX_BEGINP("Destroying Memory pools");
    g_core->memPool.destroy();
    shutdownMemoryPool();
    BX_END_OK();

    shutdownErrorReport();
    BX_DELETE(g_alloc, g_core);
    g_core = nullptr;

#ifdef _DEBUG
    stb_leakcheck_dumpmem();
#endif
}

static double calcAvgFrameTime(const FrameData& fd)
{
    double sum = 0;
    for (int i = 0; i < BX_COUNTOF(fd.frameTimes); i++) {
        sum += fd.frameTimes[i];
    }
    sum /= double(BX_COUNTOF(fd.frameTimes));
    return sum;
}

void termite::doFrame()
{
    g_core->tempAlloc.free();

    FrameData& fd = g_core->frameData;
    if (fd.lastFrameTick == 0)
        fd.lastFrameTick = bx::getHPCounter();

    int64_t frameTick = bx::getHPCounter();
    double dt = g_core->timeMultiplier * double(frameTick - fd.lastFrameTick)/g_core->hpTimerFreq;
    float fdt = float(dt);

    if (g_core->gfxDriver) {
        ImGui::GetIO().DeltaTime = fdt;
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    if (g_core->updateFn)
        g_core->updateFn(fdt);
    
    runEventDispatcher(fdt);

    if (g_core->gfxDriver) {
        ImGui::Render();
        ImGui::GetIO().MouseWheel = 0;
    }

    if (g_core->renderer)
        g_core->renderer->render(nullptr);

    if (g_core->ioDriver->async)
        g_core->ioDriver->async->runAsyncLoop();

    if (g_core->gfxDriver)
        g_core->gfxDriver->frame();

    fd.frame++;
    fd.elapsedTime += dt;
    fd.frameTime = dt;
    fd.lastFrameTick = frameTick;
    int frameTimeIter = fd.frame % BX_COUNTOF(fd.frameTimes);
    fd.frameTimes[frameTimeIter] = dt;
    fd.avgFrameTime = calcAvgFrameTime(fd);
    double fpsTime = fd.elapsedTime - fd.fpsTime;
    if (frameTimeIter == 0 && fpsTime != 0) {
        fd.fps = BX_COUNTOF(fd.frameTimes) / fpsTime;
        fd.fpsTime = fd.elapsedTime;
    }
}

void termite::pause()
{
    g_core->timeMultiplier = 0;
}

void termite::resume()
{
    g_core->timeMultiplier = 1.0;
    g_core->frameData.lastFrameTick = bx::getHPCounter();
}

bool termite::isPaused()
{
    return g_core->timeMultiplier == 0;
}

double termite::getFrameTime()
{
    return g_core->frameData.frameTime;
}

double termite::getElapsedTime()
{
    return g_core->frameData.elapsedTime;
}

double termite::getFps()
{
    return g_core->frameData.fps;
}

double termite::getSmoothFrameTime()
{
    return g_core->frameData.avgFrameTime;
}

int64_t termite::getFrameIndex()
{
    return g_core->frameData.frame;
}

termite::MemoryBlock* termite::createMemoryBlock(uint32_t size, bx::AllocatorI* alloc)
{
    g_core->memPoolLock.lock();
    HeapMemoryImpl* mem = g_core->memPool.newInstance();
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

termite::MemoryBlock* termite::refMemoryBlockPtr(const void* data, uint32_t size)
{
    g_core->memPoolLock.lock();
    HeapMemoryImpl* mem = g_core->memPool.newInstance();
    g_core->memPoolLock.unlock();
    mem->m.data = (uint8_t*)const_cast<void*>(data);
    mem->m.size = size;
    return (MemoryBlock*)mem;
}

termite::MemoryBlock* termite::copyMemoryBlock(const void* data, uint32_t size, bx::AllocatorI* alloc)
{
    g_core->memPoolLock.lock();
    HeapMemoryImpl* mem = g_core->memPool.newInstance();
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

termite::MemoryBlock* termite::refMemoryBlock(termite::MemoryBlock* mem)
{
    HeapMemoryImpl* m = (HeapMemoryImpl*)mem;
    bx::atomicFetchAndAdd(&m->refcount, 1);
    return mem;
}

void termite::releaseMemoryBlock(termite::MemoryBlock* mem)
{
    HeapMemoryImpl* m = (HeapMemoryImpl*)mem;
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

MemoryBlock* termite::readTextFile(const char* filepath)
{
    const char* rootPath = g_core->ioDriver->blocking ? g_core->ioDriver->blocking->getUri() : "";
    bx::Path fullpath(rootPath);
    fullpath.join(filepath);

    bx::CrtFileReader file;
    bx::Error err;
    if (!file.open(fullpath.cstr(), &err))
        return nullptr;
    uint32_t size = (uint32_t)file.seek(0, bx::Whence::End);

    MemoryBlock* mem = createMemoryBlock(size + 1, g_alloc);
    if (!mem) {
        file.close();
        return nullptr;
    }

    file.seek(0, bx::Whence::Begin);
    file.read(mem->data, size, &err);
    ((char*)mem->data)[size] = 0;
    file.close();

    return mem;
}

float termite::getRandomFloatUniform(float a, float b)
{
    std::uniform_real_distribution<float> dist(a, b);
    return dist(g_core->randEngine);
}

int termite::getRandomIntUniform(int a, int b)
{
    std::uniform_int_distribution<int> dist(a, b);
    return dist(g_core->randEngine);
}

float termite::getRandomFloatNormal(float mean, float sigma)
{
    std::normal_distribution<float> dist(mean, sigma);
    return dist(g_core->randEngine);
}

void termite::inputSendChars(const char* chars)
{
	ImGuiIO& io = ImGui::GetIO();

	int i = 0;
	char c;
	while ((c = chars[i++]) > 0) {
        if (c > 0 && c <  0x7f)
		    io.AddInputCharacter((ImWchar)c);
		i++;
	}
}

void termite::inputSendKeys(const bool keysDown[512], bool shift, bool alt, bool ctrl)
{
	ImGuiIO& io = ImGui::GetIO();
	memcpy(io.KeysDown, keysDown, 512*sizeof(bool));
	io.KeyShift = shift;
	io.KeyAlt = alt;
	io.KeyCtrl = ctrl;
}

void termite::inputSendMouse(float mousePos[2], int mouseButtons[3], float mouseWheel)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MousePos = ImVec2(mousePos[0], mousePos[1]);

	io.MouseDown[0] = mouseButtons[0] ? true : false;
	io.MouseDown[1] = mouseButtons[1] ? true : false;
	io.MouseDown[2] = mouseButtons[2] ? true : false;

	io.MouseWheel += mouseWheel;
}

GfxDriverApi* termite::getGfxDriver()
{
    return g_core->gfxDriver;
}

IoDriverApi* termite::getBlockingIoDriver() T_THREAD_SAFE
{
    return g_core->ioDriver->blocking;
}

IoDriverApi* termite::getAsyncIoDriver() T_THREAD_SAFE
{
    return g_core->ioDriver->async;
}

RendererApi* termite::getRenderer()
{
    return g_core->renderer;
}

PhysDriver2DApi* termite::getPhys2dDriver() T_THREAD_SAFE
{
    return g_core->phys2dDriver;
}

uint32_t termite::getEngineVersion()
{
    return T_MAKE_VERSION(0, 1);
}

bx::AllocatorI* termite::getHeapAlloc() T_THREAD_SAFE
{
    return g_alloc;
}

bx::AllocatorI* termite::getTempAlloc() T_THREAD_SAFE
{
    return &g_core->tempAlloc;
}

const Config& termite::getConfig() T_THREAD_SAFE
{
    return g_core->conf;
}

const char* termite::getCacheDir() T_THREAD_SAFE
{
    return g_cacheDir.cstr();
}

const char* termite::getDataDir() T_THREAD_SAFE
{
    return g_dataDir.cstr();
}

void termite::dumpGfxLog() T_THREAD_SAFE
{
    if (g_core->gfxLogCache) {
        for (int i = 0, c = g_core->numGfxLogCache; i < c; i++) {
            const LogCache& l = g_core->gfxLogCache[i];
            bx::logPrint(__FILE__, __LINE__, l.type, l.text);
        }

        BX_FREE(g_alloc, g_core->gfxLogCache);
        g_core->gfxLogCache = nullptr;
        g_core->numGfxLogCache = 0;
    }
}

void GfxDriverEvents::onFatal(GfxFatalType::Enum type, const char* str)
{
    char strTrimed[LOG_STRING_SIZE];
    bx::strlcpy(strTrimed, str, sizeof(strTrimed));
    strTrimed[strlen(strTrimed) - 1] = 0;

    if (g_core->numGfxLogCache < 1000) {
        m_lock.lock();
        g_core->gfxLogCache = (LogCache*)BX_REALLOC(g_alloc, g_core->gfxLogCache, sizeof(LogCache) * (++g_core->numGfxLogCache));
        g_core->gfxLogCache[g_core->numGfxLogCache-1].type = bx::LogType::Fatal;
        strcpy(g_core->gfxLogCache[g_core->numGfxLogCache-1].text, strTrimed);
        m_lock.unlock();
    }
}

void GfxDriverEvents::onTraceVargs(const char* filepath, int line, const char* format, va_list argList)
{
    char text[LOG_STRING_SIZE];
    vsnprintf(text, sizeof(text), format, argList);
    text[strlen(text) - 1] = 0;
    if (g_core->numGfxLogCache < 1000) {
        m_lock.lock();
        g_core->gfxLogCache = (LogCache*)BX_REALLOC(g_alloc, g_core->gfxLogCache, sizeof(LogCache) * (++g_core->numGfxLogCache));
        g_core->gfxLogCache[g_core->numGfxLogCache-1].type = bx::LogType::Verbose;
        strcpy(g_core->gfxLogCache[g_core->numGfxLogCache-1].text, text);
        m_lock.unlock();
    }

}
