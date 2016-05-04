#pragma once

#include "driver_mgr.h"

#ifdef termite_SHARED_LIB
#   if BX_COMPILER_MSVC
#       define TERMITE_PLUGIN_API extern "C" __declspec(dllexport) 
#   else
#       define TERMITE_PLUGIN_API extern "C" __attribute__ ((visibility("default")))
#   endif
#else
#   define TERMITE_PLUGIN_API extern "C" 
#endif

namespace termite
{
    struct pluginDesc
    {
        const char* name;
        const char* description;
        drvType type;
        uint32_t version;       // Plugin version
        uint32_t engineVersion; // Expected engine version to work with, Major/Minor combined
    };

    // Must be Implemented by Plugins
    extern "C"
    {
        // Plugins must implement this function to identify themselves to engine core
        pluginDesc* stPluginGetDesc();

        // If compatible with engine, it will be called by engine core to initialize the plugin
        // So It can register itself (or register multiple classes) with engine
        result_t stPluginInit(bx::AllocatorI* alloc);

        // Loaded plugins will be called by engine to cleanup and unregister
        void stPluginShutdown();
    }

    // Internal
    int pluginInit(const char* pluginPath);
    void pluginShutdown();
    
}   // namespace st

