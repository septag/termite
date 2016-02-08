#pragma once

#include "core.h"
#include "driver_server.h"

#ifdef STENGINE_SHARED_LIB
#   if BX_COMPILER_MSVC
#       define STPLUGIN_API extern "C" __declspec(dllexport) 
#   else
#       define STPLUGIN_API extern "C" __attribute__ ((dllexport))
#   endif
#else
#   define STPLUGIN_API extern "C" 
#endif

namespace st
{
    struct pluginDesc
    {
        const char* name;
        const char* description;
        srvDriverType type;
        uint32_t version;       // Plugin version
        uint32_t engineVersion; // Expected engine version to work with, Major/Minor combined
    };

    typedef void* pluginHandle;

    // Must be Implemented by Plugins
    extern "C"
    {
        // Plugins must implement this function to identify themselves to engine core
        pluginDesc* stPluginGetDesc();

        // If compatible with engine, it will be called by engine core to initialize the plugin
        // So It can register itself (or register multiple classes) with engine
        pluginHandle stPluginInit(bx::AllocatorI* alloc);

        // Loaded plugins will be called by engine to cleanup and unregister
        void stPluginShutdown(pluginHandle handle);
    }

    // Internal
    int pluginInit(const char* pluginPath);
    void pluginShutdown();
    
    // Public
    STENGINE_API int pluginIsInit();

}   // namespace st

