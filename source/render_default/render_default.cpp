#include "stengine/core.h"
#include "stengine/plugins.h"
#include "stengine/gfx_render.h"
#include "stengine/gfx_driver.h"

#include "bxx/logger.h"

using namespace st;

class RenderDefault : public gfxRender
{
private:
    bx::AllocatorI* m_alloc;
    gfxDriver* m_driver;

public:
    RenderDefault()
    {
        m_alloc = nullptr;
        m_driver = nullptr;
    }

    int init(bx::AllocatorI* alloc, gfxDriver* driver, void* sdlWindow) override
    {
        m_alloc = alloc;
        m_driver = driver;

        const coreConfig& conf = coreGetConfig();

        if (sdlWindow)
            driver->setSDLWindow(sdlWindow);

        BX_BEGINP("Initializing Graphics Driver");
        if (!driver->init(conf.gfxDeviceId, nullptr, alloc)) {
            BX_END_FATAL();
            BX_FATAL("Init Graphics Driver failed");
            return ST_ERR_FAILED;
        }

        //driver->reset(conf.gfxWidth, conf.gfxHeight, gfxResetFlag::None);
        driver->reset(conf.gfxWidth, conf.gfxHeight, gfxResetFlag::None);
        driver->setViewClear(0, gfxClearFlag::Color | gfxClearFlag::Depth, 0x303030ff, 1.0f, 0);
        driver->setDebug(gfxDebugFlag::Text);
        BX_END_OK();

        return 0;
    }

    void shutdown() override
    {
        if (m_driver) {
            BX_BEGINP("Shutting down Graphics Driver");
            m_driver->shutdown();
            m_driver = nullptr;
            BX_END_OK();
        }
    }

    void render() override
    {
        m_driver->touch(0);
        m_driver->setViewRect(0, 0, 0, gfxBackbufferRatio::Equal);
        m_driver->dbgTextClear(0, true);
        m_driver->dbgTextPrintf(1, 1, 0x03, "Hello World");
        m_driver->frame();
    }
};

#ifdef STENGINE_SHARED_LIB
#define MY_NAME "RenderDefault"
#define MY_VERSION ST_MAKE_VERSION(1, 0)
static st::drvDriver* gMyDriver = nullptr;
struct bx::AllocatorI* gAlloc = nullptr;

STPLUGIN_API pluginDesc* stPluginGetDesc()
{
    static pluginDesc desc;
    desc.name = MY_NAME;
    desc.description = "Default Simple Forward Renderer";
    desc.engineVersion = ST_MAKE_VERSION(0, 1);
    desc.type = drvType::Renderer;
    desc.version = ST_MAKE_VERSION(1, 0);
    return &desc;
}

STPLUGIN_API st::pluginHandle stPluginInit(bx::AllocatorI* alloc)
{
    assert(alloc);

    gAlloc = alloc;
    RenderDefault* renderer = BX_NEW(alloc, RenderDefault);
    if (renderer) {
        gMyDriver = drvRegisterRenderer(renderer, MY_NAME, MY_VERSION);
        if (gMyDriver == nullptr) {
            BX_DELETE(alloc, renderer);
            gAlloc = nullptr;
            return nullptr;
        }
    }

    return renderer;
}

STPLUGIN_API void stPluginShutdown(pluginHandle handle)
{
    assert(handle);
    assert(gAlloc);

    if (gMyDriver != nullptr)
        drvUnregister(gMyDriver);
    BX_DELETE(gAlloc, handle);
    gAlloc = nullptr;
    gMyDriver = nullptr;
}
#endif