#include "stengine/plugins.h"
#include "stengine/gfx_driver.h"

using namespace st;

#ifdef STENGINE_SHARED_LIB
STPLUGIN_API pluginDesc* stPluginGetDesc()
{
    static pluginDesc desc;
    desc.name = "Bgfx"; 
    desc.description = "Bgfx wrapper driver";
    desc.engineVersion = ST_MAKE_VERSION(0, 1);
    desc.type = pluginType::Graphics;
    desc.version = ST_MAKE_VERSION(1, 0);
    return &desc;
}

STPLUGIN_API st::pluginHandle stPluginInit()
{
    static int dummy = 1;
    return &dummy;
}

STPLUGIN_API void stPluginShutdown(pluginHandle handle)
{
}
#endif