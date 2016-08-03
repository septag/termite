#include <cstddef>
#ifdef _WIN32
#  define HAVE_STDINT_H 1
#endif
#include <SDL.h>
#undef main
#include <SDL_syswm.h>
#undef None

#include "bxx/logger.h"
#include "bxx/path.h"

#include "termite/core.h"
#include "termite/gfx_defines.h"

#define T_IMGUI_API
#include "termite/plugin_api.h"

#include <conio.h>

#include "nvg/nanovg.h"
#include "Box2D/Box2D.h"

using namespace termite;

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800

struct InputData
{
    int mouseButtons[3];
    float mouseWheel;
    bool keysDown[512];
    bool keyShift;
    bool keyCtrl;
    bool keyAlt;

    InputData()
    {
        mouseButtons[0] = mouseButtons[1] = mouseButtons[2] = 0;
        mouseWheel = 0.0f;
        keyShift = keyCtrl = keyAlt = false;
        memset(keysDown, 0x00, sizeof(keysDown));
    }
};

static SDL_Window* g_window = nullptr;
static InputData g_input;
static ImGuiApi_v0* g_gui = nullptr;
static NVGcontext* g_nvg = nullptr;
static b2World* g_world = nullptr;
//static b2Body* g_ball = nullptr;
static b2ParticleSystem* g_ps = nullptr;
static b2ParticleGroup* g_ball = nullptr;

static void* b2AllocCallback(int32 size, void* callbackData)
{
    bx::AllocatorI* alloc = getHeapAlloc();
    return BX_ALLOC(alloc, size);
}

static void b2FreeCallback(void* mem, void* callbackData)
{
    bx::AllocatorI* alloc = getHeapAlloc();
    BX_FREE(alloc, mem);
}

class PhysicsDraw : public b2Draw
{
private:
    NVGcontext* m_nvg;

public:
    PhysicsDraw(NVGcontext* nvg)
    {
        m_nvg = nvg;
    }

    void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override
    {
        NVGcontext* vg = m_nvg;
        nvgBeginPath(vg);

        for (int i = 0; i < vertexCount; i++) {
            int nextIdx = (i + 1) % vertexCount;

            nvgMoveTo(vg, vertices[i].x, vertices[i].y);
            nvgLineTo(vg, vertices[nextIdx].x, vertices[nextIdx].y);
        }

        nvgStrokeColor(vg, nvgRGBf(color.r, color.g, color.b));
        nvgStroke(vg);
    }

    /// Draw a solid closed polygon provided in CCW order.
    void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override
    {
        NVGcontext* vg = m_nvg;
        nvgBeginPath(vg);

        for (int i = 0; i < vertexCount; i++) {
            int nextIdx = (i + 1) % vertexCount;

            nvgMoveTo(vg, vertices[i].x, vertices[i].y);
            nvgLineTo(vg, vertices[nextIdx].x, vertices[nextIdx].y);
        }

        nvgFillColor(vg, nvgRGBf(color.r, color.g, color.b));
        nvgFill(vg);
    }

    /// Draw a circle.
    void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color) override
    {
        NVGcontext* vg = m_nvg;
        nvgBeginPath(vg);
        nvgCircle(vg, center.x, center.y, radius);
        nvgStrokeColor(vg, nvgRGBf(color.r, color.g, color.b));
        nvgStroke(vg);
    }

    /// Draw a solid circle.
    void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color) override
    {
        NVGcontext* vg = m_nvg;
        nvgBeginPath(vg);
        nvgCircle(vg, center.x, center.y, radius);
        nvgFillColor(vg, nvgRGBf(color.r, color.g, color.b));
        nvgFill(vg);
    }

    /// Draw a particle array
    void DrawParticles(const b2Vec2 *centers, float32 radius, const b2ParticleColor *colors, int32 count) override
    {
        NVGcontext* vg = m_nvg;
        nvgBeginPath(vg);
        nvgFillColor(vg, nvgRGB(255, 255, 255));
        for (int i = 0; i < count; i++)
            nvgCircle(vg, centers[i].x, centers[i].y, radius*0.1f);
        nvgFill(vg);
    }

    /// Draw a line segment.
    void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override
    {
        NVGcontext* vg = m_nvg;
        nvgBeginPath(vg);
        nvgMoveTo(vg, p1.x, p1.y);
        nvgLineTo(vg, p2.x, p2.y);
        nvgStrokeColor(vg, nvgRGBf(color.r, color.g, color.b));
        nvgStroke(vg);
    }

    /// Draw a transform. Choose your own length scale.
    /// @param xf a transform.
    void DrawTransform(const b2Transform& xf) override
    {
        NVGcontext* vg = m_nvg;
        nvgBeginPath(vg);

        b2Vec2 p = b2Mul(xf.q, xf.p + b2Vec2(1.0f, 0));
        
        nvgCircle(vg, xf.p.x, xf.p.y, 0.5f);
        nvgMoveTo(vg, xf.p.x, xf.p.y);
        nvgLineTo(vg, p.x, p.y);

        nvgStrokeColor(vg, nvgRGB(255, 0, 0));
        nvgStroke(vg);
    }
};
static PhysicsDraw* g_pdraw = nullptr;

static void setupWorld()
{
    b2Vec2 gravity = b2Vec2(0, -9.8f);

    b2SetAllocFreeCallbacks(b2AllocCallback, b2FreeCallback, nullptr);

    bx::AllocatorI* alloc = getHeapAlloc();
    g_world = BX_NEW(alloc, b2World)(gravity);

    // Ground Box
    b2BodyDef groundBodyDef;
    groundBodyDef.position.Set(0, -5.0f);
    b2Body* groundBody = g_world->CreateBody(&groundBodyDef);
    b2PolygonShape groundBox;
    b2FixtureDef groundFixture;
    groundFixture.shape = &groundBox;
    //groundFixture.friction = 0.3f;
    groundBox.SetAsBox(20.0f, 5.0f);
    groundBody->CreateFixture(&groundFixture);

    // Ball
    /*
    b2BodyDef ballDef;
    ballDef.type = b2_dynamicBody;
    ballDef.position.Set(0, 10.0f);
    b2Body* ballBody = g_world->CreateBody(&ballDef);
    b2CircleShape ballShape;
    ballShape.m_radius = 1.0f;
    b2FixtureDef ballFixture;
    ballFixture.density = 5.0f;
    //ballFixture.friction = 0.3f;
    ballFixture.restitution = 0.5f;
    ballFixture.shape = &ballShape;
    ballBody->CreateFixture(&ballFixture);

    g_ball = ballBody;
    */

    b2ParticleSystemDef psDef;
    psDef.radius = 1.0f;
    psDef.maxCount = 512;
    psDef.density = 1.0f;
    g_ps = g_world->CreateParticleSystem(&psDef);

    b2ParticleGroupDef pd;
    b2CircleShape shape;
    shape.m_radius = 5.0f;
    pd.shape = &shape;
    pd.flags = b2_elasticParticle;
    pd.strength = 0.3f;
    pd.position.Set(0, 10.0f);
    g_ball = g_ps->CreateParticleGroup(pd);
}

static void releaseWorld()
{
    if (g_world) {
        bx::AllocatorI* alloc = getHeapAlloc();
        BX_DELETE(alloc, g_world);
        g_world = nullptr;
    }
}

static bool sdlPollEvents()
{
    SDL_Event e;
    SDL_PollEvent(&e);

    switch (e.type) {
    case SDL_QUIT:
        return false;
    case SDL_MOUSEWHEEL:
    {
        if (e.wheel.y > 0)
            g_input.mouseWheel = 1.0f;
        else if (e.wheel.y < 0)
            g_input.mouseWheel = -1.0f;
        break;
    }

    case SDL_MOUSEBUTTONDOWN:
    {
        if (e.button.button == SDL_BUTTON_LEFT) g_input.mouseButtons[0] = 1;
        if (e.button.button == SDL_BUTTON_RIGHT) g_input.mouseButtons[1] = 1;
        if (e.button.button == SDL_BUTTON_MIDDLE) g_input.mouseButtons[2] = 1;
        break;
    }

    case SDL_TEXTINPUT:
        inputSendChars(e.text.text);
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        int key = e.key.keysym.sym & ~SDLK_SCANCODE_MASK;
        g_input.keysDown[key] = e.type == SDL_KEYDOWN;
        g_input.keyShift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        g_input.keyCtrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        g_input.keyAlt = (SDL_GetModState() & KMOD_ALT) != 0;
        inputSendKeys(g_input.keysDown, g_input.keyShift, g_input.keyAlt, g_input.keyCtrl);
        break;
    }

    default:
        break;
    }

    return true;
}

static void update(float dt)
{
    int mx, my;
    uint32_t mouseMask = SDL_GetMouseState(&mx, &my);

    float mousePos[2] = { 0.0f, 0.0f };
    if (SDL_GetWindowFlags(g_window) & SDL_WINDOW_MOUSE_FOCUS) {
        mousePos[0] = (float)mx;
        mousePos[1] = (float)my;
    } else {
        mousePos[0] = -1.0f;
        mousePos[1] = -1.0f;
    }

    g_input.mouseButtons[0] = (mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT)) ? 1 : 0;
    g_input.mouseButtons[1] = (mouseMask & SDL_BUTTON(SDL_BUTTON_RIGHT)) ? 1 : 0;
    g_input.mouseButtons[2] = (mouseMask & SDL_BUTTON(SDL_BUTTON_MIDDLE)) ? 1 : 0;

    inputSendMouse(mousePos, g_input.mouseButtons, g_input.mouseWheel);

    g_input.mouseButtons[0] = g_input.mouseButtons[1] = g_input.mouseButtons[2] = 0;
    g_input.mouseWheel = 0.0f;

    /*
    static bool opened = true;
    g_gui->begin("test", &opened, 0);
    g_gui->button("test", ImVec2(100, 0));
    static float color[3] = { 1.0f, 0, 0 };
    g_gui->colorEdit3("Color", color);
    //g_gui->colorButton(ImVec4(color[0], color[1], color[2], 1.0f), true, true);
    g_gui->end();

    nvgBeginFrame(g_nvg, WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f);
    nvgFillColor(g_nvg, nvgRGB(255, 255, 255));
    nvgRoundedRect(g_nvg, 10, 10, 100.0f, 200.0f, 10.0f);
    nvgFill(g_nvg);
    nvgEndFrame(g_nvg);
    */

    if (mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        g_ball->ApplyLinearImpulse(b2Vec2(0, 100.0f));

        /*
        b2ParticleDef pd;
        pd.flags = b2_elasticParticle;
        pd.lifetime = 5.0f;
        //pd.color.Set(getRandomIntUniform(0, 255), getRandomIntUniform(0, 255), getRandomIntUniform(0, 255), 255);
        b2Vec2 pos = g_ball->GetPosition();
        float xRight = pos.x < -1.0f ? 0 : 0.1f;
        float xLeft = pos.x > 1.0f ? 0 : -0.1f;
        pd.position = pos + b2Vec2(0, -1.0f);
        pd.velocity = b2Vec2(getRandomFloatUniform(xLeft, xRight), getRandomFloatUniform(0, 0.1f));
        g_ps->CreateParticle(pd);

        pd.velocity = b2Vec2(getRandomFloatUniform(xLeft, xRight), getRandomFloatUniform(0, 0.1f));
        g_ps->CreateParticle(pd);
        */
    }

    g_world->Step(1.0f/20.0f, 8, 3, 2);

    float aspectRatio = float(WINDOW_WIDTH) / float(WINDOW_HEIGHT);

    nvgBeginFrame(g_nvg, WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f);
    nvgTranslate(g_nvg, float(WINDOW_WIDTH)*0.5f, float(WINDOW_HEIGHT)*0.5f);
    nvgScale(g_nvg, 10.0f, -10.0f);

/*
    nvgFillColor(g_nvg, nvgRGB(255, 255, 255));
    nvgCircle(g_nvg, 0, 0, 1.0f);
    //nvgMoveTo(g_nvg, 0, -10.0f);
    //nvgLineTo(g_nvg, 0, 0);
    nvgMoveTo(g_nvg, 0, 0);
    nvgLineTo(g_nvg, 0, 10.0f);
    nvgRect(g_nvg, 10.0f, 10.0f, 30.0f, 10.0f);
    nvgFill(g_nvg);*/
    g_world->DrawDebugData();
    b2Vec2 pos = g_ball->GetCenter();
    nvgBeginPath(g_nvg);
    nvgStrokeColor(g_nvg, nvgRGB(0, 255, 0));
    nvgStrokeWidth(g_nvg, 0.1f);
    nvgCircle(g_nvg, pos.x, pos.y, 5.0f);
    nvgStroke(g_nvg);
    nvgEndFrame(g_nvg);
    
}

static termite::GfxPlatformData* getSDLWindowData(SDL_Window* window)
{
    static termite::GfxPlatformData platform;
    memset(&platform, 0x00, sizeof(platform));

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window, &wmi)) {
        BX_WARN("Could not fetch SDL window handle");
        return nullptr;
    }

#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
    platform.ndt = wmi.info.x11.display;
    platform.nwh = (void*)(uintptr_t)wmi.info.x11.window;
#elif BX_PLATFORM_OSX
    platform.ndt = NULL;
    platform.nwh = wmi.info.cocoa.window;
#elif BX_PLATFORM_WINDOWS
    platform.ndt = NULL;
    platform.nwh = wmi.info.win.window;
#endif // BX_PLATFORM_

    return &platform;
}

int main(int argc, char* argv[])
{
    bx::enableLogToFileHandle(stdout, stderr);

    if (SDL_Init(0) != 0) {
        BX_FATAL("SDL Init failed");
        return -1;
    }

    termite::Config conf;
    bx::Path pluginPath(argv[0]);
    strcpy(conf.pluginPath, pluginPath.getDirectory().cstr());

    g_window = SDL_CreateWindow("Termite: TestGui", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!g_window) {
        BX_FATAL("SDL window creation failed");
        termite::shutdown();
        return -1;
    }

    conf.gfxWidth = WINDOW_WIDTH;
    conf.gfxHeight = WINDOW_HEIGHT;
    conf.gfxDriverFlags = uint32_t(GfxResetFlag::VSync);

    // Set Keymap
    conf.keymap[int(GuiKeyMap::Tab)] = SDLK_TAB;
    conf.keymap[int(GuiKeyMap::LeftArrow)] = SDL_SCANCODE_LEFT;
    conf.keymap[int(GuiKeyMap::DownArrow)] = SDL_SCANCODE_DOWN;
    conf.keymap[int(GuiKeyMap::UpArrow)] = SDL_SCANCODE_UP;
    conf.keymap[int(GuiKeyMap::DownArrow)] = SDL_SCANCODE_DOWN;
    conf.keymap[int(GuiKeyMap::PageUp)] = SDL_SCANCODE_PAGEUP;
    conf.keymap[int(GuiKeyMap::PageDown)] = SDL_SCANCODE_PAGEDOWN;
    conf.keymap[int(GuiKeyMap::Home)] = SDL_SCANCODE_HOME;
    conf.keymap[int(GuiKeyMap::End)] = SDL_SCANCODE_END;
    conf.keymap[int(GuiKeyMap::Delete)] = SDLK_DELETE;
    conf.keymap[int(GuiKeyMap::Backspace)] = SDLK_BACKSPACE;
    conf.keymap[int(GuiKeyMap::Enter)] = SDLK_RETURN;
    conf.keymap[int(GuiKeyMap::Escape)] = SDLK_ESCAPE;
    conf.keymap[int(GuiKeyMap::A)] = SDLK_a;
    conf.keymap[int(GuiKeyMap::C)] = SDLK_c;
    conf.keymap[int(GuiKeyMap::V)] = SDLK_v;
    conf.keymap[int(GuiKeyMap::X)] = SDLK_x;
    conf.keymap[int(GuiKeyMap::Y)] = SDLK_y;
    conf.keymap[int(GuiKeyMap::Z)] = SDLK_z;

    if (termite::initialize(conf, update, getSDLWindowData(g_window))) {
        BX_FATAL(termite::getErrorString());
        BX_VERBOSE(termite::getErrorCallstack());
        termite::shutdown();
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }
    g_gui = (ImGuiApi_v0*)getEngineApi(uint16_t(ApiId::ImGui), 0);
    g_nvg = nvgCreate(1, 254, getGfxDriver(), (GfxApi_v0*)getEngineApi(uint16_t(ApiId::Gfx), 0), getHeapAlloc());
    g_pdraw = BX_NEW(getHeapAlloc(), PhysicsDraw)(g_nvg);

    setupWorld();
    g_world->SetDebugDraw(g_pdraw);
    g_pdraw->SetFlags(b2Draw::e_shapeBit | b2Draw::e_particleBit);

    while (sdlPollEvents()) {
        termite::doFrame();
    }

    BX_DELETE(getHeapAlloc(), g_pdraw);
    
    releaseWorld();
    nvgDelete(g_nvg);
    termite::shutdown();
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}

