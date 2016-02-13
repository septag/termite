#include "core.h"
#include "plugins.h"

#include "bxx/array.h"
#include "bx/os.h"
#include "bxx/path.h"
#include "bxx/logger.h"

#include <dirent.h>

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
};

typedef pluginDesc* (*FnPluginGetDesc)();
typedef pluginHandle (*FnPluginInit)(bx::AllocatorI* alloc);
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

static int loadPlugin(const char* pluginRootPath, const char* filename)
{
    bx::Path fullpath(pluginRootPath);
    fullpath.join(filename);

    bx::Path fileext = fullpath.getFileExt();
    if (bx::stricmp(fileext.cstr(), PLUGIN_EXT) == 0) {
        Plugin p;

        BX_BEGINP("Loading plugin '%s'", filename);
        p.dlHandle = bx::dlopen(fullpath.cstr());
        if (!p.dlHandle) {
            BX_END_FATAL();
            BX_WARN("Opening plugin '%s' failed: Invalid shared library", filename);
            return -1;
        }

        FnPluginGetDesc getDescFn = (FnPluginGetDesc)bx::dlsym(p.dlHandle, "stPluginGetDesc");
        FnPluginInit initFn = (FnPluginInit)bx::dlsym(p.dlHandle, "stPluginInit");
        if (!getDescFn || !initFn) {
            BX_END_FATAL();
            bx::dlclose(p.dlHandle);
            return -1;
        }

        pluginDesc* desc = getDescFn();
        if (ST_VERSION_MAJOR(coreGetVersion()) != ST_VERSION_MAJOR(desc->engineVersion)) {
            BX_END_FATAL();
            BX_WARN("Loading plugin '%s' failed: Incompatible version", filename);
            bx::dlclose(p.dlHandle);
            return -1;
        }

        if (checkPrevPlugins(desc->name)) {
            BX_END_FATAL();
            BX_WARN("Loading plugin '%s' failed: Another plugin with the same name ('%s') is loaded", filename,
                    desc->name);
            bx::dlclose(p.dlHandle);
            return -1;
        }

        p.handle = initFn(coreGetAlloc());
        if (!p.handle) {
            BX_END_FATAL();
            BX_WARN("Loading plugin '%s' failed: Plugin '%s' Init failed", filename, desc->name);
            bx::dlclose(p.dlHandle);
            return -2;  // Init of plugin failed
        }

        Plugin* pdst = gPlugins->plugins.push();
        pdst->dlHandle = p.dlHandle;
        pdst->handle = p.handle;
        memcpy(&pdst->desc, desc, sizeof(pdst->desc));

        BX_END_OK();

        return 0;
    }

    return -1;
}

static int loadPluginsInDirectory(const char* rootPath)
{
    BX_VERBOSE("Scanning for plugins in directory '%s' ...", rootPath);
    DIR* dir = opendir(rootPath);
    if (!dir) {
        BX_FATAL("Could not open plugin directory '%s'", rootPath);
        return ST_ERR_FAILED;
    }

    dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type == DT_REG) {
            loadPlugin(rootPath, ent->d_name);
        }
    }

    closedir(dir);

    return ST_OK;
}

static void shutdownPlugins()
{
    for (int i = 0; i < gPlugins->plugins.getCount(); i++) {
        const Plugin& p = gPlugins->plugins[i];
        if (p.dlHandle) {
            BX_BEGINP("Unloading plugin '%s'", p.desc.name);
            if (p.handle) {
                FnPluginShutdown shutdownFn = (FnPluginShutdown)bx::dlsym(p.dlHandle, "stPluginShutdown");
                if (!shutdownFn) {
                    BX_END_NONFATAL();
                    BX_WARN("Plugin '%s' does not implement shutdown", p.desc.name);
                }
                shutdownFn(p.handle);
                BX_END_OK();
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

    BX_TRACE("Initializing Plugin System ...");
    bx::AllocatorI* alloc = coreGetAlloc();
    gPlugins = BX_NEW(alloc, PluginSystem);
    if (!gPlugins) {
        return ST_ERR_OUTOFMEM;
    }

    if (!gPlugins->plugins.create(10, 10, alloc)) {
        return ST_ERR_OUTOFMEM;
    }

    if (loadPluginsInDirectory(pluginPath)) {
        return ST_ERR_FAILED;
    }

    // List loaded plugins
    for (int i = 0; i < gPlugins->plugins.getCount(); i++) {
        const pluginDesc& desc = gPlugins->plugins[i].desc;
        BX_VERBOSE("Plugin => Name: '%s', Version: '%d.%d'",
                   desc.name, ST_VERSION_MAJOR(desc.version), ST_VERSION_MINOR(desc.version));
    }


    return ST_OK;
}

void st::pluginShutdown()
{
    if (!gPlugins)
        return;

    shutdownPlugins();

    bx::AllocatorI* alloc = coreGetAlloc();
    gPlugins->plugins.destroy();
    BX_DELETE(alloc, gPlugins);
    gPlugins = nullptr;
}

