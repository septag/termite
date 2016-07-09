#include "termite/core.h"
#include "termite/vec_math.h"
#include "termite/gfx_render.h"
#include "termite/gfx_driver.h"

#define T_CORE_API
#include "termite/plugin_api.h"

#include "bxx/logger.h"

using namespace termite;

static CoreApi_v0* g_core = nullptr;

class StockRenderer : public RendererI
{
private:
    bx::AllocatorI* m_alloc;
    GfxApi* m_driver;

public:
    StockRenderer()
    {
        m_alloc = nullptr;
        m_driver = nullptr;
    }

    result_t init(bx::AllocatorI* alloc, GfxApi* driver) override
    {
        m_alloc = alloc;
        m_driver = driver;

        const Config& conf = g_core->getConfig();
        driver->reset(conf.gfxWidth, conf.gfxHeight, GfxResetFlag(conf.gfxDriverFlags));

        return 0;
    }

    void shutdown() override
    {
    }

    void render(const void* renderData) override
    {
        m_driver->touch(0);
        m_driver->setViewClear(0, GfxClearFlag::Color | GfxClearFlag::Depth, 0x303030ff, 1.0f, 0);
        m_driver->setViewRectRatio(0, 0, 0, BackbufferRatio::Equal);
    }
};

#ifdef termite_SHARED_LIB
static StockRenderer g_stockRenderer;

static PluginDesc* getStockRendererDesc()
{
    static PluginDesc desc;
    strcpy(desc.name, "StockRenderer");
    strcpy(desc.description, "");
    desc.type = PluginType::Renderer;
    desc.version = T_MAKE_VERSION(0, 9);
    return &desc;
}

static void* initStockRenderer(bx::AllocatorI* alloc, GetApiFunc getApi)
{
    g_core = (CoreApi_v0*)getApi(uint16_t(ApiId::Core), 0);
    if (!g_core)
        return nullptr;

    return &g_stockRenderer;
}

static void shutdownStockRenderer()
{
}

T_PLUGIN_EXPORT void* termiteGetPluginApi(uint16_t apiId, uint32_t version)
{
    static PluginApi_v0 v0;

    if (T_VERSION_MAJOR(version) == 0) {
        v0.init = initStockRenderer;
        v0.shutdown = shutdownStockRenderer;
        v0.getDesc = getStockRendererDesc;
        return &v0;
    } else {
        return nullptr;
    }
}
#endif
