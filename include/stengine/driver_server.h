#pragma once

namespace st
{
    class gfxDriver;
    class gfxRender;

    struct drvDriver;

    enum class drvType : int
    {
        GraphicsDriver,
        Storage,
        Renderer
    };

    int drvInit();
    void drvShutdown();

    STENGINE_API drvDriver* drvRegisterGraphics(gfxDriver* driver, const char* name, uint32_t version);
    STENGINE_API gfxDriver* drvGetGraphics(drvDriver* drv);

    STENGINE_API drvDriver* drvRegisterRenderer(gfxRender* render, const char* name, uint32_t version);
    STENGINE_API gfxRender* drvGetRenderer(drvDriver* drv);

    STENGINE_API drvDriver* drvFindHandleByName(const char* name);
    STENGINE_API int drvFindHandlesByType(drvType type, drvDriver** handles = nullptr, int maxHandles = 0);
    STENGINE_API uint32_t drvGetVersion(drvDriver* drv);
    STENGINE_API const char* drvGetName(drvDriver* drv);
    STENGINE_API void drvUnregister(drvDriver* drv);
}   // namespace st