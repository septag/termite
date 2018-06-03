#include "pch.h"
#include "plugin_system.h"
#include "internal.h"

#include "bx/os.h"
#include "bxx/array.h"
#include "bxx/path.h"

#include <dirent.h>

// default Plugin initialization for static linking builds
#ifdef termite_STATIC_LIB
// Fwd declerations, these functions are declared inside static plugins
void* initDiskDriver(bx::AllocatorI* alloc, tee::GetApiFunc getApi);
tee::PluginDesc* getDiskDriverDesc();
void shutdownDiskDriver();

tee::PluginDesc* getBgfxDriverDesc();
void* initBgfxDriver(bx::AllocatorI* alloc, tee::GetApiFunc getApi);
void shutdownBgfxDriver();

tee::PluginDesc* getBox2dDriverDesc();
void* initBox2dDriver(bx::AllocatorI* alloc, tee::GetApiFunc getApi);
void shutdownBox2dDriver();

tee::PluginDesc* getSdlMixerDriverDesc();
void* initSdlMixerDriver(bx::AllocatorI* alloc, tee::GetApiFunc getApi);
void shutdownSdlMixerDriver();

static void loadStaticPlugins()
{
    // IoDriver
    static tee::PluginApi ioApi;
    ioApi.init = initDiskDriver;
    ioApi.shutdown = shutdownDiskDriver;
    ioApi.getDesc = getDiskDriverDesc;
    addCustomPlugin(getDiskDriverDesc(), &ioApi);

    // BgfxDriver
    static tee::PluginApi bgfxApi;
    bgfxApi.init = initBgfxDriver;
    bgfxApi.shutdown = shutdownBgfxDriver;
    bgfxApi.getDesc = getBgfxDriverDesc;
    addCustomPlugin(getBgfxDriverDesc(), &bgfxApi);

    // Box2D driver
    static tee::PluginApi box2dApi;
    box2dApi.init = initBox2dDriver;
    box2dApi.shutdown = shutdownBox2dDriver;
    box2dApi.getDesc = getBox2dDriverDesc;
    addCustomPlugin(getBox2dDriverDesc(), &box2dApi);

    // Sound driver
    static tee::PluginApi soundApi;
    soundApi.init = initSdlMixerDriver;
    soundApi.shutdown = shutdownSdlMixerDriver;
    soundApi.getDesc = getSdlMixerDriverDesc;
    addCustomPlugin(getSdlMixerDriverDesc(), &soundApi);
}
#endif

namespace tee {
    struct Plugin
    {
        PluginDesc desc;
        bx::Path filepath;
        void* dllHandle;
        PluginApi* api;
    };

    struct PluginSystem
    {
        bx::Array<Plugin> plugins;
        bx::AllocatorI* alloc;

        explicit PluginSystem(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
        }
    };

    static PluginSystem* gPluginSys = nullptr;

    static bool loadPlugin(const bx::Path& pluginPath, void** pDllHandle, PluginApi** pApi)
    {
        bx::Path pluginExt = pluginPath.getFileExt();
        if (!pluginExt.isEqualNoCase(BX_DL_EXT))
            return false;

        void* dllHandle = bx::dlopen(pluginPath.cstr());
        if (!dllHandle) {
            //puts(dlerror());
            return false;  // Invalid DLL
        }

        GetApiFunc getPluginApi = (GetApiFunc)bx::dlsym(dllHandle, "termiteGetPluginApi");
        if (!getPluginApi) {
            bx::dlclose(dllHandle);
            return false;  // Not a plugin
        }

        PluginApi* pluginApi = (PluginApi*)getPluginApi(ApiId::Plugin, 0);
        if (!pluginApi) {
            bx::dlclose(dllHandle);
            return false;  // Incompatible plugin
        }

        *pDllHandle = dllHandle;
        *pApi = pluginApi;

        return true;
    }

    static bool validatePlugin(const bx::Path& pluginPath, PluginDesc* desc)
    {
        void* dllHandle;
        PluginApi* api;
        if (!loadPlugin(pluginPath, &dllHandle, &api)) {
            return false;
        }

        memcpy(desc, api->getDesc(), sizeof(PluginDesc));
        bx::dlclose(dllHandle);

        return true;
    }

    bool initPluginSystem(const char* pluginPath, bx::AllocatorI* alloc)
    {
        if (gPluginSys) {
            BX_ASSERT(false);
            return false;
        }

        BX_TRACE("Initializing Plugin System ...");
        gPluginSys = BX_NEW(alloc, PluginSystem)(alloc);
        if (!gPluginSys)
            return false;

        if (!gPluginSys->plugins.create(10, 20, alloc))
            return false;

#ifdef termite_STATIC_LIB
        loadStaticPlugins();
#else
        // Enumerate plugins in the 'pluginPath' directory
        BX_VERBOSE("Scanning for plugins in directory '%s' ...", pluginPath);
        DIR* dir = opendir(pluginPath);
        if (!dir) {
            BX_FATAL("Could not open plugin directory '%s'", pluginPath);
            return false;
        }

        dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_type == DT_REG) {
                bx::Path filepath(pluginPath);
                filepath.join(ent->d_name);

                PluginDesc desc;
                if (validatePlugin(filepath, &desc)) {
                    // Save in plugin library
                    Plugin* p = gPluginSys->plugins.push();
                    bx::memSet(p, 0x00, sizeof(*p));
                    memcpy(&p->desc, &desc, sizeof(desc));
                    p->filepath = filepath;
                }
            }
        }
        closedir(dir);
#endif

        // List enumerated plugins
        for (int i = 0; i < gPluginSys->plugins.getCount(); i++) {
            const PluginDesc& desc = gPluginSys->plugins[i].desc;
            BX_VERBOSE("Found Plugin => Name: '%s', Version: '%d.%d'",
                       desc.name, TEE_VERSION_MAJOR(desc.version), TEE_VERSION_MINOR(desc.version));
        }

        return true;
    }

    void shutdownPluginSystem()
    {
        if (!gPluginSys)
            return;

        // Shutdown live plugins
        for (int i = 0; i < gPluginSys->plugins.getCount(); i++) {
            const Plugin& p = gPluginSys->plugins[i];
            if (p.api) {
                shutdownPlugin(PluginHandle(uint16_t(i)));
            }
        }

        gPluginSys->plugins.destroy();
        BX_DELETE(gPluginSys->alloc, gPluginSys);
        gPluginSys = nullptr;
    }

    void* initPlugin(PluginHandle handle, bx::AllocatorI* alloc)
    {
        BX_ASSERT(handle.isValid(), "Plugin Handle is invalid");

        Plugin& p = gPluginSys->plugins[handle.value];
        if (p.api) {
            return p.api->init(alloc, getEngineApi);
        } else {
            loadPlugin(p.filepath, &p.dllHandle, &p.api);
            if (!p.api)
                return nullptr;
            return p.api->init(alloc, getEngineApi);
        }
    }

    void shutdownPlugin(PluginHandle handle)
    {
        BX_ASSERT(handle.isValid());

        Plugin& p = gPluginSys->plugins[handle.value];
        if (p.api)
            p.api->shutdown();
        if (p.dllHandle)
            bx::dlclose(p.dllHandle);
        p.dllHandle = nullptr;
        p.api = nullptr;
    }

    int findPlugins(const char* name, PluginHandle* handles, int maxHandles, PluginType::Enum type, uint32_t minVersion)
    {
        int index = 0;
        for (int i = 0, c = gPluginSys->plugins.getCount(); i < c && index < maxHandles; i++) {
            const Plugin& p = gPluginSys->plugins[i];
            if (bx::strCmpI(name, p.desc.name) == 0 && 
                (type == PluginType::Unknown || type == p.desc.type) &&
                (minVersion == 0 || minVersion >= p.desc.version))
            {
                handles[index++] = PluginHandle(uint16_t(i));
            }
        }
        return index;
    }

    int findPlugins(PluginType::Enum type, PluginHandle* handles, int maxHandles, uint32_t minVersion)
    {
        int index = 0;
        for (int i = 0, c = gPluginSys->plugins.getCount(); i < c && index < maxHandles; i++) {
            const Plugin& p = gPluginSys->plugins[i];
            if (type == p.desc.type&&
                (minVersion == 0 || minVersion >= p.desc.version))
            {
                handles[index++] = PluginHandle(uint16_t(i));
            }
        }
        return index;
    }

    PluginHandle findPlugin(const char* name, PluginType::Enum filterType /*= PluginType::Unknown*/, uint32_t minVersion /*= 0*/)
    {
        PluginHandle handle;
        if (findPlugins(name, &handle, 1, filterType, minVersion) > 0)
            return handle;
        else
            return PluginHandle();
    }

    PluginHandle findPlugin(PluginType::Enum type, PluginType::Enum filterType /*= PluginType::Unknown*/, uint32_t minVersion /*= 0*/)
    {
        PluginHandle handle;
        if (findPlugins(type, &handle, 1, minVersion) > 0)
            return handle;
        else
            return PluginHandle();
    }

    const PluginDesc& getPluginDesc(PluginHandle handle)
    {
        BX_ASSERT(handle.isValid());
        return gPluginSys->plugins[handle.value].desc;
    }


    bool addCustomPlugin(const PluginDesc* desc, PluginApi* pApi)
    {
        Plugin* p = gPluginSys->plugins.push();
        if (!p)
            return false;
        bx::memSet(p, 0x00, sizeof(*p));
        memcpy(&p->desc, desc, sizeof(PluginDesc));
        p->api = pApi;
        return true;
    }
} // namespace tee