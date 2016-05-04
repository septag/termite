#include "pch.h"
#include "plugins.h"

#include "bxx/array.h"
#include "bx/os.h"
#include "bxx/path.h"
#include "bxx/logger.h"

#include <dirent.h>

#if BX_PLATFORM_WINDOWS || BX_PLATFORM_XBOXONE || BX_PLATFORM_WINRT
#   define PLUGIN_EXT "dll"
#elif BX_PLATFORM_OSX || BX_PLATFORM_IOS
#   define PLUGIN_EXT "dylib"
#else
#   define PLUGIN_EXT "so"
#endif

using namespace termite;

struct Plugin
{
    void* dlHandle;
    pluginDesc desc;
};

struct PluginSystem
{
    bx::Array<Plugin> plugins;
};

typedef pluginDesc* (*FnPluginGetDesc)();
typedef int (*FnPluginInit)(bx::AllocatorI* alloc);
typedef void (*FnPluginShutdown)();

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

        // check for dll/so/dylib extension
#ifndef BX_DL_EXT 
#   error "Platform not supported for plugins"
#endif
        if (!fileext.isEqualNoCase(BX_DL_EXT))
            return -1;

        p.dlHandle = bx::dlopen(fullpath.cstr());
        if (!p.dlHandle)    {
#if !BX_PLATFORM_WINDOWS
            BX_FATAL(dlerror());
#endif
            return -1;
        }

        FnPluginGetDesc getDescFn = (FnPluginGetDesc)bx::dlsym(p.dlHandle, "stPluginGetDesc");
        FnPluginInit initFn = (FnPluginInit)bx::dlsym(p.dlHandle, "stPluginInit");
        if (!getDescFn || !initFn) {
            bx::dlclose(p.dlHandle);
            return -1;
        }

        pluginDesc* desc = getDescFn();
        BX_BEGINP("Loading plugin '%s' (%s)", filename, desc->name);
        if (T_VERSION_MAJOR(coreGetVersion()) != T_VERSION_MAJOR(desc->engineVersion)) {
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

        int r = initFn(coreGetAlloc());
        if (r) {
            BX_END_FATAL();
            BX_WARN("Loading plugin '%s' failed: Plugin '%s' Init failed", filename, desc->name);
            bx::dlclose(p.dlHandle);
            return -2;  // Init of plugin failed
        }

        Plugin* pdst = gPlugins->plugins.push();
        pdst->dlHandle = p.dlHandle;
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
        return T_ERR_FAILED;
    }

    dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type == DT_REG) {
            loadPlugin(rootPath, ent->d_name);
        }
    }

    closedir(dir);

    return T_OK;
}

static void shutdownPlugins()
{
    for (int i = 0; i < gPlugins->plugins.getCount(); i++) {
        const Plugin& p = gPlugins->plugins[i];
        if (p.dlHandle) {
            BX_BEGINP("Unloading plugin '%s'", p.desc.name);
            FnPluginShutdown shutdownFn = (FnPluginShutdown)bx::dlsym(p.dlHandle, "stPluginShutdown");
            if (!shutdownFn) {
                BX_END_NONFATAL();
                BX_WARN("Plugin '%s' does not implement shutdown", p.desc.name);
            }
            shutdownFn();
            BX_END_OK();

            bx::dlclose(p.dlHandle);
        }
    }

    gPlugins->plugins.clear();
}

int termite::pluginInit(const char* pluginPath)
{
    if (gPlugins)
        return T_ERR_ALREADY_INITIALIZED;

    BX_TRACE("Initializing Plugin System ...");
    bx::AllocatorI* alloc = coreGetAlloc();
    gPlugins = BX_NEW(alloc, PluginSystem);
    if (!gPlugins) {
        return T_ERR_OUTOFMEM;
    }

    if (!gPlugins->plugins.create(10, 10, alloc)) {
        return T_ERR_OUTOFMEM;
    }

    if (loadPluginsInDirectory(pluginPath)) {
        return T_ERR_FAILED;
    }

    // List loaded plugins
    for (int i = 0; i < gPlugins->plugins.getCount(); i++) {
        const pluginDesc& desc = gPlugins->plugins[i].desc;
        BX_VERBOSE("Plugin => Name: '%s', Version: '%d.%d'",
                   desc.name, T_VERSION_MAJOR(desc.version), T_VERSION_MINOR(desc.version));
    }

    return T_OK;
}

void termite::pluginShutdown()
{
    if (!gPlugins)
        return;

    shutdownPlugins();

    bx::AllocatorI* alloc = coreGetAlloc();
    gPlugins->plugins.destroy();
    BX_DELETE(alloc, gPlugins);
    gPlugins = nullptr;
}

