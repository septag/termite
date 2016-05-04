#include "termite/core.h"
#include "termite/plugins.h"
#include "termite/gfx_render.h"
#include "termite/gfx_driver.h"

#include "bxx/logger.h"

#include "../imgui_impl/imgui_impl.h"

using namespace termite;

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

    result_t init(bx::AllocatorI* alloc, gfxDriver* driver, const gfxPlatformData* platformData, const int* uiKeymap) override
    {
        m_alloc = alloc;
        m_driver = driver;

        const coreConfig& conf = coreGetConfig();

        if (platformData) {
            driver->setPlatformData(*platformData);
        } else {
            BX_WARN("Renderer initilization is skipped");
            return T_ERR_NOT_INITIALIZED;
        }            

        BX_BEGINP("Initializing Graphics Driver");
        if (driver->init(conf.gfxDeviceId, nullptr, alloc)) {
            BX_END_FATAL();
            BX_FATAL("Init Graphics Driver failed");
            return T_ERR_FAILED;
        }

        driver->reset(conf.gfxWidth, conf.gfxHeight, gfxResetFlag(conf.gfxDriverFlags));
        driver->setViewClear(0, gfxClearFlag::Color | gfxClearFlag::Depth, 0x303030ff, 1.0f, 0);
        driver->setDebug(gfxDebugFlag::Text);
        BX_END_OK();

        BX_VERBOSE("Graphics Driver: %s", rendererTypeToStr(driver->getRendererType()));

        if (imguiInit(conf.gfxWidth, conf.gfxHeight, driver, uiKeymap)) {
            BX_FATAL("Init ImGui Integration failed");
            return T_ERR_FAILED;
        }

        return T_OK;
    }

    void shutdown() override
    {
        imguiShutdown();

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
        imguiRender();

        m_driver->dbgTextClear(0, true);
        m_driver->dbgTextPrintf(1, 1, 0x03, "Fps: %.2f", coreGetFps());
        m_driver->dbgTextPrintf(1, 2, 0x03, "FrameTime: %.4f", coreGetFrameTime());
        m_driver->dbgTextPrintf(1, 3, 0x03, "ElapsedTime: %.2f", coreGetElapsedTime());
        m_driver->frame();
    }

    void frame()
    {
        imguiNewFrame();
    }

    void sendImInputMouse(float mousePos[2], int mouseButtons[3], float mouseWheel)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MousePos = ImVec2(mousePos[0], mousePos[1]);
        io.MouseDown[0] = mouseButtons[0] ? true : false;
        io.MouseDown[1] = mouseButtons[1] ? true : false;
        io.MouseDown[2] = mouseButtons[2] ? true : false;
        io.MouseWheel = mouseWheel;
    }

    void sendImInputChars(const char* chars) override
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddInputCharactersUTF8(chars);
    }

    void sendImInputKeys(const bool keysDown[512], bool shift, bool alt, bool ctrl)
    {
        ImGuiIO& io = ImGui::GetIO();
        memcpy(io.KeysDown, keysDown, sizeof(io.KeysDown));
        io.KeyShift = shift;
        io.KeyAlt = alt;
        io.KeyCtrl = ctrl;
    }
};

#ifdef termite_SHARED_LIB
#define MY_NAME "RenderDefault"
#define MY_VERSION T_MAKE_VERSION(1, 0)
static termite::drvHandle g_driver = nullptr;
struct bx::AllocatorI* g_alloc = nullptr;

TERMITE_PLUGIN_API pluginDesc* stPluginGetDesc()
{
    static pluginDesc desc;
    desc.name = MY_NAME;
    desc.description = "Default Simple Forward Renderer";
    desc.engineVersion = T_MAKE_VERSION(0, 1);
    desc.type = drvType::Renderer;
    desc.version = T_MAKE_VERSION(1, 0);
    return &desc;
}

TERMITE_PLUGIN_API result_t stPluginInit(bx::AllocatorI* alloc)
{
    assert(alloc);

    g_alloc = alloc;
    RenderDefault* renderer = BX_NEW(alloc, RenderDefault);
    if (renderer) {
        g_driver = drvRegister(drvType::Renderer, MY_NAME, MY_VERSION, renderer);
        if (g_driver == nullptr) {
            BX_DELETE(alloc, renderer);
            g_alloc = nullptr;
            return T_ERR_FAILED;
        }
    }

    return T_OK;
}

TERMITE_PLUGIN_API void stPluginShutdown()
{
    assert(g_alloc);

    if (g_driver != nullptr) {
        BX_DELETE(g_alloc, drvGetRenderer(g_driver));
        drvUnregister(g_driver);
    }
    g_alloc = nullptr;
    g_driver = nullptr;
}
#endif
