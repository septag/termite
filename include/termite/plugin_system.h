#pragma once

#include "types.h"
#include "plugin_api.h"

namespace termite
{
    struct PluginT {};
    typedef PhantomType<uint16_t, PluginT, UINT16_MAX> PluginHandle;

    result_t initPluginSystem(const char* pluginPath, bx::AllocatorI* alloc);
    void shutdownPluginSystem();

    TERMITE_API void* initPlugin(PluginHandle handle, bx::AllocatorI* alloc);
    TERMITE_API void shutdownPlugin(PluginHandle handle);
    TERMITE_API int findPluginByName(const char* name, uint32_t version, PluginHandle* handles, int maxHandles, 
                                     PluginType::Enum type = PluginType::Unknown);
    TERMITE_API int findPluginByType(PluginType::Enum type, uint32_t version, PluginHandle* handles, int maxHandles);
    TERMITE_API const PluginDesc& getPluginDesc(PluginHandle handle);
} // termite