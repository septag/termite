#include "pch.h"

#include "bx/readerwriter.h"
#include "bx/os.h"
#include "bx/cpu.h"
#include "bxx/path.h"
#include "bxx/inifile.h"
#include "bxx/pool.h"
#include "bxx/lock.h"
#include "bx/crtimpl.h"
#include "bxx/array.h"
#include "bxx/string.h"

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
#include "sound_driver.h"
#include "lang.h"

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
#include <chrono>

#include "remotery/Remotery.h"
#include "lz4/lz4.h"
#include "tiny-AES128-c/aes.h"

#define MEM_POOL_BUCKET_SIZE 256
#define IMGUI_VIEWID 255
#define NANOVG_VIEWID 254
#define LOG_STRING_SIZE 256
#define RANDOM_NUMBER_POOL 10000

#define T_ENC_SIGN 0x54454e43        // TENC
#define T_ENC_VERSION T_MAKE_VERSION(1, 0)       

#include "bxx/rapidjson_allocator.h"

using namespace termite;

typedef std::chrono::high_resolution_clock TClock;
typedef std::chrono::high_resolution_clock::time_point TClockTimePt;

// default AES encryption keys
static const uint8_t kAESKey[] = {0x32, 0xBF, 0xE7, 0x76, 0x41, 0x21, 0xF6, 0xA5, 0xEE, 0x70, 0xDC, 0xC8, 0x73, 0xBC, 0x9E, 0x37};
static const uint8_t kAESIv[] = {0x0A, 0x2D, 0x76, 0x63, 0x9F, 0x28, 0x10, 0xCD, 0x24, 0x22, 0x26, 0x68, 0xC1, 0x5A, 0x82, 0x5A};

//
struct FrameData
{
    uint64_t frame;
    double frameTime;
    double fps;
    double elapsedTime;
    double avgFrameTime;
    TClockTimePt lastFrameTimePt;
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

#pragma pack(push, 1)
struct EncodeHeader
{
    uint32_t sign;
    uint32_t version;
    int decodeSize;
    int uncompSize;
};
#pragma pack(pop)

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

struct ConsoleCommand
{
    size_t cmdHash;
    std::function<void(int, const char**)> callback;
    ConsoleCommand() : cmdHash(0) {}
};

struct Core
{
    UpdateCallback updateFn;
    Config conf;
    RendererApi* renderer;
    FrameData frameData;
    double timeMultiplier;
    bx::Pool<HeapMemoryImpl> memPool;
    bx::Lock memPoolLock;
    GfxDriverApi* gfxDriver;
    IoDriverDual* ioDriver;
    PhysDriver2DApi* phys2dDriver;
    SoundDriverApi* sndDriver;
    PageAllocator tempAlloc;
    GfxDriverEvents gfxDriverEvents;
    LogCache* gfxLogCache;
    int numGfxLogCache;

    std::random_device randDevice;
    std::mt19937 randEngine;
    int* randomPoolInt;
    float* randomPoolFloat;
    volatile int32_t randomIntOffset;
    volatile int32_t randomFloatOffset;

    Remotery* rmt;
    bx::Array<ConsoleCommand> consoleCmds;

    bool init;
    bool gfxReset;

    Core() :
        tempAlloc(T_MID_TEMP),
        randEngine(randDevice())
    {
        gfxDriver = nullptr;
        phys2dDriver = nullptr;
        sndDriver = nullptr;
        updateFn = nullptr;
        renderer = nullptr;
        ioDriver = nullptr;
        timeMultiplier = 1.0;
        gfxLogCache = nullptr;
        numGfxLogCache = 0;
        rmt = nullptr;
        init = false;
        gfxReset = false;
        memset(&frameData, 0x00, sizeof(frameData));
        randomFloatOffset = randomIntOffset = 0;
        randomPoolFloat = nullptr;
        randomPoolInt = nullptr;
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

// Define rapidjson static allocator 
bx::AllocatorI* rapidjson::BxAllocatorStatic::Alloc = g_alloc;

#if BX_PLATFORM_ANDROID
static JavaVM* g_javaVM = nullptr;
static jclass g_activityClass = 0;
static jobject g_activityObj = 0;

extern "C" JNIEXPORT void JNICALL Java_com_termite_util_Platform_termiteInitEngineVars(JNIEnv* env, jclass cls, 
                                                                                       jobject obj, jstring dataDir, 
                                                                                       jstring cacheDir)
{
    BX_UNUSED(cls);

    env->GetJavaVM(&g_javaVM);
    g_activityClass = (jclass)env->NewGlobalRef(env->GetObjectClass(obj));
    g_activityObj = env->NewGlobalRef(obj);
    
    const char* str = env->GetStringUTFChars(dataDir, nullptr);
    bx::strlcpy(g_dataDir.getBuffer(), str, sizeof(g_dataDir));
    env->ReleaseStringUTFChars(dataDir, str);

    str = env->GetStringUTFChars(cacheDir, nullptr);
    bx::strlcpy(g_cacheDir.getBuffer(), str, sizeof(g_cacheDir));
    env->ReleaseStringUTFChars(cacheDir, str);
}


extern "C" JNIEXPORT void JNICALL Java_com_termite_util_Platform_termiteSetGraphicsReset(JNIEnv* env, jclass cls)
{
    if (g_core)
        g_core->gfxReset = true;
}

JavaMethod termite::androidFindMethod(const char* methodName, const char* methodSig, const char* classPath,
                                      JavaMethodType::Enum type)
{
    assert(g_javaVM);

    JavaMethod m;
    memset(&m, 0x00, sizeof(m));

    if (type == JavaMethodType::Method) {
        g_javaVM->AttachCurrentThread(&m.env, nullptr);
    } else if (type == JavaMethodType::StaticMethod) {
        g_javaVM->GetEnv((void**)&m.env, JNI_VERSION_1_6);
    }

    if (m.env == nullptr) {
        BX_WARN("Calling Java method '%s' failed: Cannot retreive Java Env", methodName);
        return m;
    }

    if (classPath == nullptr) {
        m.cls = g_activityClass;
        m.obj = g_activityObj;
    } else {
        m.cls = m.env->FindClass(classPath);
        if (m.cls == 0) {
            BX_WARN("Calling Java method '%s' failed: Cannot find class '%s'", methodName, classPath);
            return m;
        }
    }


    if (type == JavaMethodType::Method) {
        m.methodId = m.env->GetMethodID(m.cls, methodName, methodSig);
    } else if (type == JavaMethodType::StaticMethod) {
        m.methodId = m.env->GetStaticMethodID(m.cls, methodName, methodSig);
    }
    if (m.methodId == 0) {
        BX_WARN("Finding Java method '%s' failed: Cannot find method in class '%s'", methodName, classPath);
        return m;
    }

    return m;
}
#endif

static void* remoteryMallocCallback(void* mm_context, rmtU32 size)
{
    return BX_ALLOC(g_alloc, size);
}

static void remoteryFreeCallback(void* mm_context, void* ptr)
{
    BX_FREE(g_alloc, ptr);
}

static void* remoteryReallocCallback(void* mm_context, void* ptr, rmtU32 size)
{
    return BX_REALLOC(g_alloc, ptr, size);
}

static void remoteryInputHandlerCallback(const char* text, void* context)
{
    assert(g_core);

    const int maxArgs = 16;
    bx::String64 args[maxArgs];
    char cmdText[2048];
    bx::strlcpy(cmdText, text, sizeof(cmdText));
    char* token = strtok(cmdText, " ");
    int numArgs = 0;
    while (token && numArgs < maxArgs) {
        args[numArgs++] = token;
        token = strtok(nullptr, " ");
    }

    if (numArgs > 0) {
        // find and execute command
        size_t cmdHash = tinystl::hash_string(args[0].cstr(), args[0].getLength());
        for (int i = 0; i < g_core->consoleCmds.getCount(); i++) {
            if (g_core->consoleCmds[i].cmdHash == cmdHash) {
                const char* cargs[maxArgs];
                for (int k = 0; k < numArgs; k++)
                    cargs[k] = args[k].cstr();

                g_core->consoleCmds[i].callback(numArgs, cargs);
                break;
            }
        }
    }
}

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

    // Random pools
    g_core->randomPoolInt = (int*)BX_ALLOC(g_alloc, sizeof(int)*RANDOM_NUMBER_POOL);
    g_core->randomPoolFloat = (float*)BX_ALLOC(g_alloc, sizeof(float)*RANDOM_NUMBER_POOL);
    if (!g_core->randomPoolInt || !g_core->randomPoolFloat)
        return T_ERR_OUTOFMEM;
    restartRandom();    // fill random values

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

        initFontSystem(g_alloc, vec2f(float(conf.refScreenWidth), float(conf.refScreenHeight)));
        registerFontToResourceLib();

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
        if (T_FAILED(initImGui(IMGUI_VIEWID, g_core->gfxDriver, g_alloc, conf.keymap,
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

    // Sound device
    if (conf.soundName[0] != 0) {
        r = findPluginByName(conf.soundName, 0, &pluginHandle, 1, PluginType::SoundDriver);
        if (r > 0) {
            g_core->sndDriver = (SoundDriverApi*)initPlugin(pluginHandle, g_alloc);
        }

        if (!g_core->sndDriver) {
            T_ERROR("Core init failed: Could not detect Sound driver: %s", conf.soundName);
            return T_ERR_FAILED;
        }

        const PluginDesc& desc = getPluginDesc(pluginHandle);
        BX_BEGINP("Initializing Sound Driver: %s v%d.%d", desc.name, T_VERSION_MAJOR(desc.version), T_VERSION_MINOR(desc.version));
        if (T_FAILED(g_core->sndDriver->init(conf.audioFreq, conf.audioChannels, conf.audioBufferSize))) {
            BX_END_FATAL();
            T_ERROR("Core init failed: Could not initialize Sound driver");
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

#if RMT_ENABLED
    BX_BEGINP("Initializing Remotery");
    rmtSettings* rsettings = rmt_Settings();
    rsettings->malloc = remoteryMallocCallback;
    rsettings->free = remoteryFreeCallback;
    rsettings->realloc = remoteryReallocCallback;
#  if termite_DEV
    g_core->consoleCmds.create(64, 64, g_alloc);
    rsettings->input_handler = remoteryInputHandlerCallback;
#endif

    if (rmt_CreateGlobalInstance(&g_core->rmt) != RMT_ERROR_NONE) {
        BX_END_NONFATAL();
    }
    BX_END_OK();
#endif

    registerLangToResourceLib();

    g_core->init = true;
    return 0;
}

void termite::shutdown(ShutdownCallback callback, void* userData)
{
    if (!g_core) {
        assert(false);
        return;
    }

#if RMT_ENABLED
    if (g_core->rmt)
        rmt_DestroyGlobalInstance(g_core->rmt);
    for (int i = 0; i < g_core->consoleCmds.getCount(); ++i) {
        ConsoleCommand* cmd = g_core->consoleCmds.itemPtr(i);
        cmd->~ConsoleCommand();
    }
    g_core->consoleCmds.destroy();
#endif

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
    shutdownFontSystem();
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

    if (g_core->sndDriver) {
        BX_BEGINP("Shutting down Sound Driver");
        g_core->sndDriver->shutdown();
        g_core->sndDriver = nullptr;
        BX_END_OK();
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

    if (g_core->randomPoolFloat)
        BX_FREE(g_alloc, g_core->randomPoolFloat);
    if (g_core->randomPoolInt)
        BX_FREE(g_alloc, g_core->randomPoolInt);

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
    rmt_BeginCPUSample(DoFrame, 0);
    g_core->tempAlloc.free();

    FrameData& fd = g_core->frameData;
    if (fd.frame == 0)
        fd.lastFrameTimePt = TClock::now();

    TClockTimePt frameTimePt = TClock::now();
    std::chrono::duration<double> dt_fp = frameTimePt - fd.lastFrameTimePt;
    double dt = g_core->timeMultiplier * dt_fp.count();
    float fdt = float(dt);

    if (g_core->gfxDriver) {
        ImGui::GetIO().DeltaTime = float(dt_fp.count());
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    rmt_BeginCPUSample(Game_Update, 0);
    if (g_core->updateFn)
        g_core->updateFn(fdt);
    rmt_EndCPUSample(); // Game_Update
    
    runEventDispatcher(fdt);

    rmt_BeginCPUSample(ImGui_Render, 0);
    if (g_core->gfxDriver) {
        ImGui::Render();
        ImGui::GetIO().MouseWheel = 0;
    }
    rmt_EndCPUSample(); // ImGuiRender

    if (g_core->renderer)
        g_core->renderer->render(nullptr);

    rmt_BeginCPUSample(Async_Loop, 0);
    if (g_core->ioDriver->async)
        g_core->ioDriver->async->runAsyncLoop();
    rmt_EndCPUSample(); // Async_Loop

    rmt_BeginCPUSample(Gfx_DrawFrame, 0);
    if (g_core->gfxDriver)
        g_core->gfxDriver->frame();
    rmt_EndCPUSample(); // Gfx_DrawFrame

    fd.frame++;
    fd.elapsedTime += dt;
    fd.frameTime = dt;
    fd.lastFrameTimePt = frameTimePt;
    int frameTimeIter = fd.frame % BX_COUNTOF(fd.frameTimes);
    fd.frameTimes[frameTimeIter] = dt;
    fd.avgFrameTime = calcAvgFrameTime(fd);
    double fpsTime = fd.elapsedTime - fd.fpsTime;
    if (frameTimeIter == 0 && fpsTime != 0) {
        fd.fps = BX_COUNTOF(fd.frameTimes) / fpsTime;
        fd.fpsTime = fd.elapsedTime;
    }
    rmt_EndCPUSample(); // DoFrame
}

void termite::pause()
{
    g_core->timeMultiplier = 0;
}

void termite::resume()
{
    g_core->timeMultiplier = 1.0;
    g_core->frameData.lastFrameTimePt = TClock::now();
}

bool termite::isPaused()
{
    return g_core->timeMultiplier == 0;
}

void termite::resetTempAlloc()
{
    g_core->tempAlloc.free();
}

void termite::resetBackbuffer(uint16_t width, uint16_t height)
{
    if (g_core->gfxDriver)
        g_core->gfxDriver->reset(width, height, g_core->conf.gfxDriverFlags);
    g_core->conf.gfxWidth = width;
    g_core->conf.gfxHeight = height;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(width), float(height));
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

uint64_t termite::getFrameIndex()
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

MemoryBlock* termite::readTextFile(const char* absFilepath)
{
    bx::CrtFileReader file;
    bx::Error err;
    if (!file.open(absFilepath, &err))
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

MemoryBlock* termite::readBinaryFile(const char* absFilepath)
{
    bx::CrtFileReader file;
    bx::Error err;
    if (!file.open(absFilepath, &err))
        return nullptr;
    uint32_t size = (uint32_t)file.seek(0, bx::Whence::End);
    if (size == 0) {
        file.close();
        return nullptr;
    }

    MemoryBlock* mem = createMemoryBlock(size, g_alloc);
    if (!mem) {
        file.close();
        return nullptr;
    }

    file.seek(0, bx::Whence::Begin);
    file.read(mem->data, size, &err);
    file.close();

    return mem;
}

bool termite::saveBinaryFile(const char* absFilepath, const MemoryBlock* mem)
{
    bx::CrtFileWriter file;
    bx::Error err;
    if (mem->size == 0 || !file.open(absFilepath, false, &err)) {
        return false;
    }

    int wb = file.write(mem->data, mem->size, &err);
    file.close();
    return wb == mem->size ? true : false;
}

MemoryBlock* termite::encodeMemoryAES128(const MemoryBlock* mem, bx::AllocatorI* alloc, const uint8_t* key, const uint8_t* iv)
{
    assert(mem);
    if (key == nullptr)
        key = kAESKey;
    if (iv == nullptr)
        iv = kAESIv;
    if (alloc == nullptr)
        alloc = g_alloc;

    EncodeHeader header;
    header.sign = T_ENC_SIGN;
    header.version = T_ENC_VERSION;
    
    // Compress LZ4
    int maxSize = BX_ALIGN_16(LZ4_compressBound(mem->size));
    void* compressedBuff = BX_ALLOC(alloc, maxSize);
    if (!compressedBuff)
        return nullptr;
    int compressSize = LZ4_compress_default((const char*)mem->data, (char*)compressedBuff, mem->size, maxSize);
    int alignedCompressSize = BX_ALIGN_16(compressSize);
    assert(alignedCompressSize <= maxSize);
    memset((uint8_t*)compressedBuff + compressSize, 0x00, alignedCompressSize - compressSize);

    // AES Encode
    MemoryBlock* encMem = createMemoryBlock(alignedCompressSize + sizeof(header), alloc);
    if (encMem) {
        AES_CBC_encrypt_buffer((uint8_t*)encMem->data + sizeof(header), (uint8_t*)compressedBuff, alignedCompressSize, key, iv);

        // finalize header and slap it at the begining of the output buffer
        header.decodeSize = compressSize;
        header.uncompSize = (int)mem->size;
        *((EncodeHeader*)encMem->data) = header;
    }

    BX_FREE(alloc, compressedBuff);
    return encMem;
}

MemoryBlock* termite::decodeMemoryAES128(const MemoryBlock* mem, bx::AllocatorI* alloc, const uint8_t* key, const uint8_t* iv)
{
    assert(mem);
    if (key == nullptr)
        key = kAESKey;
    if (iv == nullptr)
        iv = kAESIv;
    if (alloc == nullptr)
        alloc = g_alloc;

    // AES Decode
    const EncodeHeader header = *((const EncodeHeader*)mem->data);
    if (header.sign != T_ENC_SIGN ||
        header.version != T_ENC_VERSION)
    {
        return nullptr;
    }

    uint8_t* encBuff = (uint8_t*)mem->data + sizeof(header);
    uint32_t encSize = mem->size - sizeof(header);
    assert(encSize % 16 == 0);

    void* decBuff = BX_ALLOC(alloc, encSize);
    if (!decBuff)
        return nullptr;
    AES_CBC_decrypt_buffer((uint8_t*)decBuff, encBuff, encSize, key, iv);
    
    // Uncompress LZ4
    MemoryBlock* rmem = createMemoryBlock(header.uncompSize, alloc);
    if (rmem)
        LZ4_decompress_safe((const char*)decBuff, (char*)rmem->data, header.decodeSize, header.uncompSize);

    BX_FREE(alloc, decBuff);
    return rmem;
}

void termite::cipherXOR(uint8_t* outputBuff, const uint8_t* inputBuff, size_t buffSize, const uint8_t* key, size_t keySize)
{
    assert(buffSize > 0);
    assert(keySize > 0);
    for (size_t i = 0; i < buffSize; i++) {
        outputBuff[i] = inputBuff[i] ^ key[i % keySize];
    }
}

void termite::restartRandom()
{
    std::uniform_int_distribution<int> idist(0, INT_MAX);
    int* randomInt = g_core->randomPoolInt;
    for (int i = 0; i < RANDOM_NUMBER_POOL; i++) {
        randomInt[i] = idist(g_core->randEngine);
    }

    std::uniform_real_distribution<float> fdist(0, 1.0f);
    float* randomFloat = g_core->randomPoolFloat;
    for (int i = 0; i < RANDOM_NUMBER_POOL; i++) {
        randomFloat[i] = fdist(g_core->randEngine);
    }

    g_core->randomFloatOffset = g_core->randomIntOffset = 0;
}

float termite::getRandomFloatUniform(float a, float b)
{
    assert(a <= b);
    int off = g_core->randomFloatOffset;
    float r = (g_core->randomPoolFloat[off]*(b - a)) + a;
    bx::atomicExchange<int32_t>(&g_core->randomFloatOffset, (off + 1) % RANDOM_NUMBER_POOL);
    return r;
}

int termite::getRandomIntUniform(int a, int b)
{
    assert(a <= b);
    int off = g_core->randomIntOffset;
    int r = (g_core->randomPoolInt[off] % (b - a + 1)) + a;
    bx::atomicExchange<int32_t>(&g_core->randomIntOffset, (off + 1) % RANDOM_NUMBER_POOL);
    return r;
}

float termite::getRandomFloatNormal(float mean, float sigma)
{
    std::normal_distribution<float> dist(mean, sigma);
    return dist(g_core->randEngine);
}

void termite::inputSendChars(const char* chars)
{
	ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharactersUTF8(chars);
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

RendererApi* termite::getRenderer() T_THREAD_SAFE
{
    return g_core->renderer;
}

SoundDriverApi* termite::getSoundDriver() T_THREAD_SAFE
{
    return g_core->sndDriver;
}

PhysDriver2DApi* termite::getPhys2dDriver() T_THREAD_SAFE
{
    return g_core->phys2dDriver;
}

uint32_t termite::getEngineVersion() T_THREAD_SAFE
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

bool termite::needGfxReset() T_THREAD_SAFE
{
    return g_core->gfxReset;
}

void termite::shutdownGraphics()
{
    // Unload all graphics resources
    unloadAllResources("texture");

    // Unload subsystems
    shutdownSpriteSystemGraphics();
    shutdownImGui();
    shutdownDebugDraw();
    shutdownVectorGfx();
    shutdownFontSystemGraphics();
    shutdownModelLoader();
    shutdownTextureLoader();
    shutdownGfxUtils();

    if (g_core->phys2dDriver) {
        g_core->phys2dDriver->shutdownGraphicsObjects();
    }

    // Shutdown driver
    if (g_core->gfxDriver) {
        g_core->gfxDriver->shutdown();
        g_core->gfxDriver = nullptr;
        dumpGfxLog();
    }
}

bool termite::resetGraphics(const GfxPlatformData* platform)
{
    int r;
    PluginHandle pluginHandle;
    const Config& conf = g_core->conf;

    r = findPluginByName(conf.gfxName, 0, &pluginHandle, 1, PluginType::GraphicsDriver);
    if (r > 0) {
        g_core->gfxDriver = (GfxDriverApi*)initPlugin(pluginHandle, g_alloc);
    }

    if (!g_core->gfxDriver) {
        T_ERROR("Core init failed: Could not detect Graphics driver: %s", conf.gfxName);
        return false;
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
        return false;
    }
    BX_END_OK();
    dumpGfxLog();

    // Initialize Renderer with Gfx Driver
    if (g_core->renderer) {
        BX_BEGINP("Initializing Renderer");
        if (T_FAILED(g_core->renderer->init(g_alloc, g_core->gfxDriver))) {
            BX_END_FATAL();
            T_ERROR("Core init failed: Could not initialize Renderer");
            return false;
        }
        BX_END_OK();
    }

    // Init and Register graphics resource loaders
    initTextureLoader(g_core->gfxDriver, g_alloc);
    registerTextureToResourceLib();

    initModelLoader(g_core->gfxDriver, g_alloc);
    registerModelToResourceLib();

    initFontSystemGraphics();

    // VectorGraphics
    if (initVectorGfx(g_alloc, g_core->gfxDriver)) {
        T_ERROR("Initializing Vector Graphics failed");
        return false;
    }

    // Debug graphics
    if (initDebugDraw(g_alloc, g_core->gfxDriver)) {
        T_ERROR("Initializing Editor Draw failed");
        return false;
    }

    // Graphics Utilities
    if (initGfxUtils(g_core->gfxDriver)) {
        T_ERROR("Initializing Graphics Utilities failed");
        return false;
    }

    // ImGui initialize
    if (T_FAILED(initImGui(IMGUI_VIEWID, g_core->gfxDriver, g_alloc, conf.keymap,
                           conf.uiIniFilename, platform ? platform->nwh : nullptr))) {
        T_ERROR("Initializing ImGui failed");
        return false;
    }

    if (!initSpriteSystemGraphics(g_core->gfxDriver)) {
        T_ERROR("Initializing Sprite System failed");
        return false;
    }

    if (g_core->phys2dDriver) {
        g_core->phys2dDriver->initGraphicsObjects();
    }

    // Reload resources
    reloadResourceType("texture");

    g_core->gfxReset = false;
    return true;
}

void termite::registerConsoleCommand(const char* name, std::function<void(int, const char**)> callback)
{
#if termite_DEV && RMT_ENABLED
    assert(g_core);
    ConsoleCommand* cmd = new(g_core->consoleCmds.push()) ConsoleCommand;
    cmd->cmdHash = tinystl::hash_string(name, strlen(name));
    cmd->callback = callback;
#endif
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
