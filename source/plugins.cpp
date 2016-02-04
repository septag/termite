#include "plugins.h"

#include "bxx/array.h"
#include "bx/os.h"
#include "bxx/path.h"

#if BX_PLATFORM_WINDOWS
#   define PLUGIN_EXT "dll"
#else
#   define PLUGIN_EXT "so"
#endif

using namespace st;
using namespace bx;

struct Plugin
{
    pluginHandle handle;
    void* dlHandle;
    pluginDesc desc;
};

struct PluginSystem
{
    Array<Plugin> plugins;

    uv_fs_t scanReq;
    bool initDone;

    PluginSystem()
    {
        initDone = false;
    }
};

typedef pluginDesc* (*FnPluginGetDesc)();
typedef pluginHandle (*FnPluginInit)();
typedef void (*FnPluginShutdown)(pluginHandle handle);

static PluginSystem* gPlugins = nullptr;

static bool checkPrevPlugins(const char* name)
{
    for (int i = 0; i < gPlugins->plugins.getCount(); i++) {
        const Plugin& p = gPlugins->plugins[i];
        if (bx::stricmp(p.desc.name, name) == 0)
            return true;
    }

    return false;
}

static void uvScanDir(uv_fs_t* handle)
{
    uv_dirent_s ent;
    int result = (int)handle->result;
    if (result < 0) {
        BX_WARN("Scanning directory '%s' for plugins failed", handle->path);
        uv_fs_req_cleanup(handle);
        return;
    }

    bx::Path pluginPath(handle->path);
    while (uv_fs_scandir_next(handle, &ent) != UV_EOF) {
        bx::Path fullpath(pluginPath);
        fullpath.join(ent.name);

        bx::Path fileext = fullpath.getFileExt();
        if (bx::stricmp(fileext.cstr(), PLUGIN_EXT) == 0) {
            Plugin p;
            p.dlHandle = bx::dlopen(fullpath.cstr());
            if (!p.dlHandle) {
                BX_WARN("Opening plugin '%s' failed: Invalid shared library", fullpath.cstr());
                continue;
            }

            FnPluginGetDesc getDescFn = (FnPluginGetDesc)bx::dlsym(p.dlHandle, "stPluginGetDesc");
            FnPluginInit initFn = (FnPluginInit)bx::dlsym(p.dlHandle, "stPluginInit");
            if (!getDescFn || !initFn) {
                bx::dlclose(p.dlHandle);
                continue;
            }

            pluginDesc* desc = getDescFn();
            if (ST_VERSION_MAJOR(coreGetVersion()) != ST_VERSION_MAJOR(desc->engineVersion)) {
                BX_WARN("Loading plugin '%s' failed: Incompatible version", ent.name);
                bx::dlclose(p.dlHandle);
                continue;
            }

            if (checkPrevPlugins(desc->name)) {
                BX_WARN("Loading plugin '%s' failed: Another plugin with the same name ('%s') is loaded", ent.name, 
                        desc->name);
                bx::dlclose(p.dlHandle);
                continue;
            }

            p.handle = initFn();
            if (!p.handle) {
                BX_WARN("Loading plugin '%s' failed: Plugin '%s' Init failed", ent.name, desc->name);
                bx::dlclose(p.dlHandle);
                continue;
            }

            Plugin* pdst = gPlugins->plugins.push();
            pdst->dlHandle = p.dlHandle;
            pdst->handle = p.handle;
            memcpy(&pdst->desc, desc, sizeof(pdst->desc));

            BX_TRACE("Loaded. Plugin: '%s', Name: '%s', Version: '%d.%d'", 
                     ent.name, desc->name, ST_VERSION_MAJOR(desc->version), ST_VERSION_MINOR(desc->version));
        }
    }

    uv_fs_req_cleanup(handle);
    gPlugins->initDone = false;
}

static int loadPluginsInDirectory(const char* pluginPath)
{
    if (gPlugins->initDone)
        return ST_ERR_BUSY;
    
    gPlugins->initDone = true;
    int r;
    if ((r = uv_fs_scandir(coreGetMainLoop(), &gPlugins->scanReq, pluginPath, 0, uvScanDir))) {
        gPlugins->initDone = false;
        ST_ERROR("Scanning directory '%s' failed: %s", pluginPath, uv_strerror(r));
        return -1;
    }

    return ST_OK;
}

static void shutdownPlugins()
{
    for (int i = 0; i < gPlugins->plugins.getCount(); i++) {
        const Plugin& p = gPlugins->plugins[i];
        if (p.dlHandle) {
            BX_TRACE("Unloading plugin '%s'", p.desc.name);
            if (p.handle) {
                FnPluginShutdown shutdownFn = (FnPluginShutdown)bx::dlsym(p.dlHandle, "stPluginShutdown");
                if (!shutdownFn) {
                    BX_WARN("Plugin '%s' does not implement shutdown", p.desc.name);
                }
                shutdownFn(p.handle);
            }

            bx::dlclose(p.dlHandle);
        }
    }

    gPlugins->plugins.clear();
}

int st::pluginInit(const char* pluginPath)
{
    if (gPlugins)
        return ST_ERR_ALREADY_INITIALIZED;

    bx::AllocatorI* alloc = coreGetAlloc();
    gPlugins = BX_NEW(alloc, PluginSystem);
    if (!gPlugins)
        return ST_ERR_OUTOFMEM;

    if (!gPlugins->plugins.create(10, 10, alloc))
        return ST_ERR_OUTOFMEM;

    if (loadPluginsInDirectory(pluginPath)) {
        return ST_ERR_FAILED;
    }

    return ST_OK;
}

void st::pluginShutdown()
{
    if (!gPlugins)
        return;

    bx::AllocatorI* alloc = coreGetAlloc();
    shutdownPlugins();
    gPlugins->plugins.destroy();
    BX_DELETE(alloc, gPlugins);
    gPlugins = nullptr;
}

int st::pluginIsInit()
{
    assert(gPlugins);
    return gPlugins->initDone;
}
