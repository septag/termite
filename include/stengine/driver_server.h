#pragma once

#include "core.h"

namespace st
{
    class gfxDriver;

    enum srvDriverType : int
    {
        Graphics,
        Storage
    };

    ST_HANDLE(srvHandle);

    int srvInit();
    void srvShutdown();

    STENGINE_API srvHandle srvRegisterGraphicsDriver(gfxDriver* driver, const char* name = nullptr);
    STENGINE_API void srvUnregisterGraphicsDriver(srvHandle handle);
    STENGINE_API gfxDriver* srvGetGraphicsDriver(srvHandle handle);

    STENGINE_API srvHandle srvFindDriverHandleByName(const char* name);
    STENGINE_API int srvFindDriverHandleByType(srvDriverType type, srvHandle* handles, int maxHandles);
}   // namespace st