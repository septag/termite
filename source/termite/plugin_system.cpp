#include "pch.h"
#include "plugin_system.h"

#include "bx/os.h"
#include "bxx/array.h"
#include "bxx/path.h"
#include "bxx/logger.h"

#include <dirent.h>

using namespace termite;

struct Plugin
{
    PluginDesc desc;
    bx::Path filepath;
    void* dllHandle;
    PluginApi_v0* api;
};

struct PluginSystem
{
    bx::Array<Plugin> plugins;
    bx::AllocatorI* alloc;

    explicit PluginSystem(bx::AllocatorI* _alloc) : alloc(_alloc)
    {
    }
};

static PluginSystem* g_pluginSys = nullptr;

#ifdef termite_STATIC_LIB
#if BX_PLATFORM_ANDROID
void* initAndroidAssetDriver(bx::AllocatorI* alloc, GetApiFunc getApi);
PluginDesc* getAndroidAssetDriverDesc();
void shutdownAndroidAssetDriver();
#elif BX_PLATFORM_IOS
void* initDiskLiteDriver(bx::AllocatorI* alloc, GetApiFunc getApi);
PluginDesc* getDiskLiteDriverDesc();
void shutdownDiskLiteDriver();
#else
PluginDesc* getDiskDriverDesc();
void* initDiskDriver(bx::AllocatorI* alloc, GetApiFunc getApi);
void shutdownDiskDriver();
#endif

PluginDesc* getBgfxDriverDesc();
void* initBgfxDriver(bx::AllocatorI* alloc, GetApiFunc getApi);
void shutdownBgfxDriver();

PluginDesc* getBox2dDriverDesc();
void* initBox2dDriver(bx::AllocatorI* alloc, GetApiFunc getApi);
void shutdownBox2dDriver();

static void loadStaticPlugins()
{
    // IoDriver
    static PluginApi_v0 ioApi;
    Plugin* p = g_pluginSys->plugins.push();
    memset(p, 0x00, sizeof(*p));
#if BX_PLATFORM_ANDROID
    memcpy(&p->desc, getAndroidAssetDriverDesc(), sizeof(p->desc));
    ioApi.getDesc = getAndroidAssetDriverDesc;
    ioApi.init = initAndroidAssetDriver;
    ioApi.shutdown = shutdownAndroidAssetDriver;
#elif BX_PLATFORM_IOS
    memcpy(&p->desc, getDiskLiteDriverDesc(), sizeof(p->desc));
    ioApi.getDesc = getDiskLiteDriverDesc;
    ioApi.init = initDiskLiteDriver;
    ioApi.shutdown = shutdownDiskLiteDriver;
#else
    memcpy(&p->desc, getDiskDriverDesc(), sizeof(p->desc));
    ioApi.getDesc = getDiskDriverDesc;
    ioApi.init = initDiskDriver;
    ioApi.shutdown = shutdownDiskDriver;
#endif
    p->api = &ioApi;

    // BgfxDriver
    static PluginApi_v0 bgfxApi;
    p = g_pluginSys->plugins.push();
    memset(p, 0x00, sizeof(*p));
    memcpy(&p->desc, getBgfxDriverDesc(), sizeof(p->desc));
    bgfxApi.getDesc = getBgfxDriverDesc;
    bgfxApi.init = initBgfxDriver;
    bgfxApi.shutdown = shutdownBgfxDriver;
    p->api = &bgfxApi;

    // Box2D driver
    static PluginApi_v0 box2dApi;
    p = g_pluginSys->plugins.push();
    memset(p, 0x00, sizeof(*p));
    memcpy(&p->desc, getBox2dDriverDesc(), sizeof(p->desc));
    box2dApi.getDesc = getBox2dDriverDesc;
    box2dApi.init = initBox2dDriver;
    box2dApi.shutdown = shutdownBox2dDriver;
    p->api = &box2dApi;
}
#endif

static result_t loadPlugin(const bx::Path& pluginPath, void** pDllHandle, PluginApi_v0** pApi)
{
    bx::Path pluginExt = pluginPath.getFileExt();
    if (!pluginExt.isEqualNoCase(BX_DL_EXT))
        return -1;

    void* dllHandle = bx::dlopen(pluginPath.cstr());
    if (!dllHandle)
        return -1;  // Invalid DLL

    GetApiFunc getPluginApi = (GetApiFunc)bx::dlsym(dllHandle, "termiteGetPluginApi");
    if (!getPluginApi) {
        bx::dlclose(dllHandle);
        return -1;  // Not a plugin
    }

    PluginApi_v0* pluginApi = (PluginApi_v0*)getPluginApi(ApiId::Plugin, 0);
    if (!pluginApi) {
        bx::dlclose(dllHandle);
        return -1;  // Incompatible plugin
    }

    *pDllHandle = dllHandle;
    *pApi = pluginApi;

    return 0;
}

result_t termite::initPluginSystem(const char* pluginPath, bx::AllocatorI* alloc)
{
    if (g_pluginSys) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    BX_TRACE("Initializing Plugin System ...");
    g_pluginSys = BX_NEW(alloc, PluginSystem)(alloc);
    if (!g_pluginSys)
        return T_ERR_OUTOFMEM;
    
    if (!g_pluginSys->plugins.create(10, 20, alloc))
        return T_ERR_OUTOFMEM;
    
#ifdef termite_STATIC_LIB
    loadStaticPlugins();
#else
    // Enumerate plugins in the 'pluginPath' directory
    BX_VERBOSE("Scanning for plugins in directory '%s' ...", pluginPath);
    DIR* dir = opendir(pluginPath);
    if (!dir) {
        BX_FATAL("Could not open plugin directory '%s'", pluginPath);
        return T_ERR_FAILED;
    }

    dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type == DT_REG) {
            bx::Path filepath(pluginPath);
            filepath.join(ent->d_name);
            
            PluginDesc desc;
            if (T_OK(validatePlugin(filepath, &desc))) {
                // Save in plugin library
                Plugin* p = g_pluginSys->plugins.push();
                memset(p, 0x00, sizeof(*p));
                memcpy(&p->desc, &desc, sizeof(desc));
                p->filepath = filepath;
            }
        }
    }
    closedir(dir);
#endif

    // List enumerated plugins
    for (int i = 0; i < g_pluginSys->plugins.getCount(); i++) {
        const PluginDesc& desc = g_pluginSys->plugins[i].desc;
        BX_VERBOSE("Found Plugin => Name: '%s', Version: '%d.%d'",
            desc.name, T_VERSION_MAJOR(desc.version), T_VERSION_MINOR(desc.version));
    }

    return 0;
}

void termite::shutdownPluginSystem()
{
    if (!g_pluginSys)
        return;

    // Shutdown live plugins
    for (int i = 0; i < g_pluginSys->plugins.getCount(); i++) {
        const Plugin& p = g_pluginSys->plugins[i];
        if (p.api) {
            shutdownPlugin(PluginHandle(uint16_t(i)));
        }
    }

    g_pluginSys->plugins.destroy();
    BX_DELETE(g_pluginSys->alloc, g_pluginSys);
    g_pluginSys = nullptr;
}

void* termite::initPlugin(PluginHandle handle, bx::AllocatorI* alloc)
{
    assert(handle.isValid());

    Plugin& p = g_pluginSys->plugins[handle.value];
    if (p.api) {
        return p.api->init(alloc, getEngineApi);
    } else {
        loadPlugin(p.filepath, &p.dllHandle, &p.api);
        if (!p.api)
            return nullptr;
        return p.api->init(alloc, getEngineApi);
    }
}

void termite::shutdownPlugin(PluginHandle handle)
{
    assert(handle.isValid());

    Plugin& p = g_pluginSys->plugins[handle.value];
    if (p.api)
        p.api->shutdown();
    if (p.dllHandle)
        bx::dlclose(p.dllHandle);
    p.dllHandle = nullptr;
    p.api = nullptr;
}

int termite::findPluginByName(const char* name, uint32_t version, PluginHandle* handles, int maxHandles, 
                              PluginType::Enum type)
{
    int index = 0;
    for (int i = 0, c = g_pluginSys->plugins.getCount(); i < c && index < maxHandles; i++) {
        const Plugin& p = g_pluginSys->plugins[i];
        if (bx::stricmp(name, p.desc.name) == 0 && (type == PluginType::Unknown || type == p.desc.type)) {
            handles[index++] = PluginHandle(uint16_t(i));
        }
    }
    return index;
}

int termite::findPluginByType(PluginType::Enum type, uint32_t version, PluginHandle* handles, int maxHandles)
{
    int index = 0;
    for (int i = 0, c = g_pluginSys->plugins.getCount(); i < c && index < maxHandles; i++) {
        const Plugin& p = g_pluginSys->plugins[i];
        if (type == p.desc.type) {
            handles[index++] = PluginHandle(uint16_t(i));
        }
    }
    return index;
}

const PluginDesc& termite::getPluginDesc(PluginHandle handle)
{
    assert(handle.isValid());
    return g_pluginSys->plugins[handle.value].desc;
}
