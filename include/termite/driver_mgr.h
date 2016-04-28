#pragma once

#define STDRV_INVALID_HANDLE nullptr

namespace termite
{
    class gfxDriver;
    class gfxRender;
    class dsDriver;

    struct drvDriver;
    typedef struct drvDriver* drvHandle;

    enum class drvType : int
    {
        GraphicsDriver,
        DataStoreDriver,
        Renderer
    };

    int drvInit();
    void drvShutdown();

    TERMITE_API gfxDriver* drvGetGraphicsDriver(drvHandle drv);
    TERMITE_API gfxRender* drvGetRenderer(drvHandle drv);
    TERMITE_API dsDriver* drvGetDataStoreDriver(drvHandle drv);

    TERMITE_API drvHandle drvRegister(drvType type, const char* name, uint32_t version, void* driver);
    TERMITE_API drvHandle drvFindHandleByName(const char* name);
    TERMITE_API int drvFindHandlesByType(drvType type, drvHandle* handles = nullptr, int maxHandles = 0);
    TERMITE_API drvHandle drvFindHandleByPtr(void* driver);
    TERMITE_API uint32_t drvGetVersion(drvHandle drv);
    TERMITE_API const char* drvGetName(drvHandle drv);
    TERMITE_API void drvUnregister(drvHandle drv);
}   // namespace st