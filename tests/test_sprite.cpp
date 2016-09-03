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
#include "bxx/array.h"

#include "termite/core.h"
#include "termite/gfx_defines.h"
#include "termite/gfx_driver.h"
#include "termite/vec_math.h"
#include "termite/gfx_sprite.h"
#include "termite/camera.h"
#include "termite/gfx_texture.h"

#define T_IMGUI_API
#include "termite/plugin_api.h"

#include <conio.h>

#include "nvg/nanovg.h"

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

static bx::Array<SpriteHandle> g_sprites;
static bx::Array<vec2_t> g_poss;
static bx::Array<float> g_rots;
static bx::Array<color_t> g_colors;
static Camera2D g_cam;

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

static bool showAddSprite(const char* name, const char* caption, char* filepath, size_t filepathSize, vec2_t* ppos,
                          vec2_t* psize, float* prot)
{
    static char text[256];
    static vec2_t pos = vec2f(0, 0);
    static vec2_t size = vec2f(0.5f, 0.5f);
    static float rot = 0;

    bool r = false;
    if (g_gui->beginPopupModal(name, nullptr, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_NoResize)) {
        g_gui->inputText(caption, text, sizeof(text), 0, nullptr, nullptr);
        g_gui->inputFloat2("Pos", pos.f, -1, 0);
        g_gui->inputFloat2("Size", size.f, -1, 0);
        g_gui->sliderFloat("Rotation", &rot, 0, 360.0f, "%.0f", 1.0f);

        if (g_gui->button("Ok", ImVec2(100.0f, 0))) {
            bx::strlcpy(filepath, text, filepathSize);
            *ppos = pos;
            *psize = size;
            *prot = rot;
            g_gui->closeCurrentPopup();
            r = true;
        }
        g_gui->sameLine(0, -1.0f);
        if (g_gui->button("Cancel", ImVec2(100.0f, 0))) {
            g_gui->closeCurrentPopup();
        }
        g_gui->endPopup();
    }
    return r;
}

static void adjustNvgToCamera(NVGcontext* nvg, const vec2_t camPos, float camZoom, DisplayPolicy policy, float* strokeScale = nullptr)
{
    nvgTranslate(nvg, float(WINDOW_WIDTH)*0.5f, float(WINDOW_HEIGHT)*0.5f);
    float scale = 1.0f;
    if (policy == DisplayPolicy::FitToHeight) {
        scale = 0.5f*float(WINDOW_WIDTH) * camZoom;
    } else if (policy == DisplayPolicy::FitToWidth) {
        scale = 0.5f*float(WINDOW_HEIGHT) * camZoom;
    }
    nvgScale(nvg, scale, -scale);
    nvgTranslate(nvg, -g_cam.pos.x, -g_cam.pos.y);

    if (strokeScale)
        *strokeScale = 2.0f / scale;

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

    static bool opened = true;
    static vec2_t camPos = vec2f(0, 0);

    g_gui->begin("SpriteSystem", &opened, 0);
    g_gui->inputFloat2("Camera Pos", camPos.f, -1, 0);
    if (g_gui->button("Add", ImVec2(150.0f, 0))) {
        g_gui->openPopup("Add Sprite");
    }

    char spriteImage[64];
    vec2_t spritePos;
    vec2_t spriteSize;
    float rot;
    ResourceLib* resLib = getDefaultResourceLib();
    if (showAddSprite("Add Sprite", "Image", spriteImage, sizeof(spriteImage), &spritePos, &spriteSize, &rot)) {
        LoadTextureParams texParams;
        SpriteHandle handle = createSpriteFromTexture(loadResource(resLib, "image", spriteImage, &texParams), 
                                                      spriteSize);
        if (handle.isValid()) {
            *g_sprites.push() = handle;
            *g_poss.push() = spritePos;
            *g_colors.push() = 0xffffffff;
            *g_rots.push() = rot;
        }
    }
    g_gui->sliderFloat("Zoom", &g_cam.zoom, 0.01f, 1.0f, "%.2f", 1.0f);
    if (g_gui->button("Generate Random", ImVec2(150.0f, 0))) {
        LoadTextureParams texParams;
        const char* files[] = {
            "sprites/test2.jpg",
            "sprites/test.png"
        };
        const int batchCount = 10;
        for (int i = 0; i < batchCount; i++) {
            float halfSize = getRandomFloatUniform(0.5f, 1.0f);
            //float halfSize = 0.5f;
            SpriteHandle handle = createSpriteFromTexture(loadResource(resLib, "image", files[getRandomIntUniform(0, 1)], &texParams),
                                                          vec2f(halfSize, halfSize));
            if (handle.isValid()) {
                *g_sprites.push() = handle;
                *g_poss.push() = vec2f(getRandomFloatUniform(-5.0f, 5.0f), getRandomFloatUniform(-5.0f, 5.0f));
                *g_colors.push() = rgba(getRandomIntUniform(0, 255),
                                        getRandomIntUniform(0, 255),
                                        getRandomIntUniform(0, 255),
                                        255);
                *g_rots.push() = getRandomFloatUniform(0, 180.0f);
            }
        }
    }
    g_gui->end();

    g_gui->begin("Stats", &opened, 0);
    g_gui->labelText("Fps", "%.3f", getFps());
    g_gui->labelText("FrameTime", "%.3f", getFrameTime()*1000.0);
    g_gui->labelText("NumSprites", "%d", g_sprites.getCount());
    g_gui->end();

    GfxDriverApi* driver = getGfxDriver();
    rect_t vp = rectf(0, 0, float(WINDOW_WIDTH), float(WINDOW_HEIGHT));
    driver->touch(0);
    driver->setViewRect(0, uint16_t(vp.xmin), uint16_t(vp.ymin),
                        uint16_t(vp.xmax - vp.xmin), uint16_t(vp.ymax - vp.ymin));

    g_cam.pos = camPos;
    mtx4x4_t viewMtx = cam2dViewMtx(g_cam);
    mtx4x4_t projMtx = cam2dProjMtx(g_cam, g_cam.refWidth, g_cam.refHeight);
    driver->setViewTransform(0, viewMtx.f, projMtx.f, GfxViewFlag::Stereo, nullptr);
    driver->setViewSeq(0, true);

    drawSprites(0, g_sprites.getBuffer(), g_sprites.getCount(),
                g_poss.getBuffer(), g_rots.getBuffer(), g_colors.getBuffer());

    driver->setViewSeq(0, false);

    nvgBeginFrame(g_nvg, WINDOW_WIDTH, WINDOW_HEIGHT, 1.0f);

    float strokeScale;
    adjustNvgToCamera(g_nvg, g_cam.pos, g_cam.zoom, g_cam.policy, &strokeScale);

    nvgBeginPath(g_nvg);
    nvgStrokeWidth(g_nvg, strokeScale);
    nvgStrokeColor(g_nvg, nvgRGB(255, 255, 255));
    nvgRect(g_nvg, -0.5f, -0.5f, 1.0f, 1.0f);
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

    // Set Keymap
    conf.keymap[ImGuiKey_Tab] = SDLK_TAB;
    conf.keymap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
    conf.keymap[ImGuiKey_RightArrow] = SDL_SCANCODE_DOWN;
    conf.keymap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
    conf.keymap[ImGuiKey_RightArrow] = SDL_SCANCODE_DOWN;
    conf.keymap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
    conf.keymap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
    conf.keymap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
    conf.keymap[ImGuiKey_End] = SDL_SCANCODE_END;
    conf.keymap[ImGuiKey_Delete] = SDLK_DELETE;
    conf.keymap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
    conf.keymap[ImGuiKey_Enter] = SDLK_RETURN;
    conf.keymap[ImGuiKey_Escape] = SDLK_ESCAPE;
    conf.keymap[ImGuiKey_A] = SDLK_a;
    conf.keymap[ImGuiKey_C] = SDLK_c;
    conf.keymap[ImGuiKey_V] = SDLK_v;
    conf.keymap[ImGuiKey_X] = SDLK_x;
    conf.keymap[ImGuiKey_Y] = SDLK_y;
    conf.keymap[ImGuiKey_Z] = SDLK_z;

    if (termite::initialize(conf, update, getSDLWindowData(g_window))) {
        BX_FATAL(termite::getErrorString());
        BX_VERBOSE(termite::getErrorCallstack());
        termite::shutdown();
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }
    bx::AllocatorI* alloc = getHeapAlloc();
    g_gui = (ImGuiApi_v0*)getEngineApi(uint16_t(ApiId::ImGui), 0);
    g_nvg = nvgCreate(1, 254, getGfxDriver(), (GfxApi_v0*)getEngineApi(uint16_t(ApiId::Gfx), 0), getHeapAlloc());
    g_sprites.create(32, 32, alloc);
    g_poss.create(32, 32, alloc);
    g_rots.create(32, 32, alloc);
    g_colors.create(32, 32, alloc);
    cam2dInit(&g_cam, float(WINDOW_WIDTH), float(WINDOW_HEIGHT), DisplayPolicy::FitToHeight);

    while (sdlPollEvents()) {
        termite::doFrame();
    }

    g_sprites.destroy();
    g_poss.destroy();
    g_rots.destroy();
    g_colors.destroy();

    nvgDelete(g_nvg);
    termite::shutdown();
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}

