#pragma once

#include "types.h"
#include "plugin_api.h"

namespace tee
{
    struct PluginT {};
    typedef PhantomType<uint16_t, PluginT, UINT16_MAX> PluginHandle;

    TEE_API void* initPlugin(PluginHandle handle, bx::AllocatorI* alloc);
    TEE_API void shutdownPlugin(PluginHandle handle);
    TEE_API int findPlugins(const char* name, PluginHandle* handles, int maxHandles,
                            PluginType::Enum filterType = PluginType::Unknown, uint32_t minVersion = 0);
    TEE_API int findPlugins(PluginType::Enum type, PluginHandle* handles, int maxHandles, uint32_t minVersion = 0);

    TEE_API PluginHandle findPlugin(const char* name, PluginType::Enum filterType = PluginType::Unknown, uint32_t minVersion = 0);
    TEE_API PluginHandle findPlugin(PluginType::Enum type, PluginType::Enum filterType = PluginType::Unknown, uint32_t minVersion = 0);

    TEE_API const PluginDesc& getPluginDesc(PluginHandle handle);

    // Used mostly for initializing custom plugin in static build systems
    // @param pApi Plugin Api that must remain in memory and the functions should be assigned
    TEE_API bool addCustomPlugin(const PluginDesc* desc, PluginApi* pApi);
} // termite