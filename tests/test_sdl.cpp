#include <cstddef>
#define HAVE_STDINT_H
#include <SDL.h>
#undef main
#include <SDL_syswm.h>
#undef None

#include "termite/core.h"
#include "bxx/logger.h"
#include "bxx/path.h"

#include "termite/gfx_defines.h"
#include "termite/gfx_font.h"
#include "termite/gfx_vg.h"
#include "termite/datastore.h"
#include "termite/gfx_texture.h"
#include "termite/gfx_debug.h"
#include "termite/camera.h"

#include <conio.h>

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
static vgContext* g_vg = nullptr;
static dbgContext* g_debug = nullptr;
static dsResourceHandle g_logo = T_INVALID_HANDLE;
static Camera g_cam;

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
        coreSendInputChars(e.text.text);
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        int key = e.key.keysym.sym & ~SDLK_SCANCODE_MASK;
        g_input.keysDown[key] = e.type == SDL_KEYDOWN;
        g_input.keyShift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        g_input.keyCtrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        g_input.keyAlt = (SDL_GetModState() & KMOD_ALT) != 0;
        coreSendInputKeys(g_input.keysDown, g_input.keyShift, g_input.keyAlt, g_input.keyCtrl);
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

    coreSendInputMouse(mousePos, g_input.mouseButtons, g_input.mouseWheel);

    g_input.mouseButtons[0] = g_input.mouseButtons[1] = g_input.mouseButtons[2] = 0;
    g_input.mouseWheel = 0.0f;

    vgBegin(g_vg, WINDOW_WIDTH, WINDOW_HEIGHT);
/*
    vgTextf(g_vg, 10, 10, "Hello from %s", "sepehr");
    vgRotate(g_vg, bx::toRad(90.0f));
    vgTranslate(g_vg, 100.0f, 100.0f);
    vgRect(g_vg, rectf(0.0f, 0.0f, 100.0, 100.0f));
    vgTranslate(g_vg, 100.0f, 100.0f);
    vgImageRect(g_vg, rectf(0, 0, 100, 100), dsGetObjPtr<gfxTexture*>(nullptr, g_logo));
*/
    // Camera look/movement
    const float moveSpeed = 5.0f;
    const float lookSpeed = 3.0f;

    int numKeys;
    const uint8_t* keys = SDL_GetKeyboardState(&numKeys);
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
        camStrafe(&g_cam, -1.0f*moveSpeed*dt);
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
        camStrafe(&g_cam, moveSpeed*dt);
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
        camForward(&g_cam, moveSpeed*dt);
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
        camForward(&g_cam, -1.0f*moveSpeed*dt);
    SDL_GetRelativeMouseState(&mx, &my);
    if (mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        camPitchYaw(&g_cam, -float(my)*lookSpeed*dt, -float(mx)*lookSpeed*dt);
    }
    vgTextf(g_vg, 10.0f, 10.0f, "pitch=%.4f", g_cam.pitch);
    vgEnd(g_vg);

    dbgBegin(g_debug, WINDOW_WIDTH, WINDOW_HEIGHT, &g_cam, g_vg);
    dbgText(g_debug, vec3f(0, 1.0f, 5.0f), "testing");
    dbgSnapGridXZ(g_debug, 5.0f, 70.0f);
    dbgEnd(g_debug);
}

static termite::gfxPlatformData* getSDLWindowData(SDL_Window* window)
{
    static termite::gfxPlatformData platform;
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

    termite::coreConfig conf;
    bx::Path pluginPath(argv[0]);
    strcpy(conf.pluginPath, pluginPath.getDirectory().cstr());

    g_window = SDL_CreateWindow("stTestSDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                               WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!g_window) {
        BX_FATAL("SDL window creation failed");
        termite::coreShutdown();
        return -1;
    }

    conf.gfxWidth = WINDOW_WIDTH;
    conf.gfxHeight = WINDOW_HEIGHT;

    // Set Keymap
    conf.keymap[int(gfxGuiKeyMap::Tab)] = SDLK_TAB;
    conf.keymap[int(gfxGuiKeyMap::LeftArrow)] = SDL_SCANCODE_LEFT;
    conf.keymap[int(gfxGuiKeyMap::DownArrow)] = SDL_SCANCODE_DOWN;
    conf.keymap[int(gfxGuiKeyMap::UpArrow)] = SDL_SCANCODE_UP;
    conf.keymap[int(gfxGuiKeyMap::DownArrow)] = SDL_SCANCODE_DOWN;
    conf.keymap[int(gfxGuiKeyMap::PageUp)] = SDL_SCANCODE_PAGEUP;
    conf.keymap[int(gfxGuiKeyMap::PageDown)] = SDL_SCANCODE_PAGEDOWN;
    conf.keymap[int(gfxGuiKeyMap::Home)] = SDL_SCANCODE_HOME;
    conf.keymap[int(gfxGuiKeyMap::End)] = SDL_SCANCODE_END;
    conf.keymap[int(gfxGuiKeyMap::Delete)] = SDLK_DELETE;
    conf.keymap[int(gfxGuiKeyMap::Backspace)] = SDLK_BACKSPACE;
    conf.keymap[int(gfxGuiKeyMap::Enter)] = SDLK_RETURN;
    conf.keymap[int(gfxGuiKeyMap::Escape)] = SDLK_ESCAPE;
    conf.keymap[int(gfxGuiKeyMap::A)] = SDLK_a;
    conf.keymap[int(gfxGuiKeyMap::C)] = SDLK_c;
    conf.keymap[int(gfxGuiKeyMap::V)] = SDLK_v;
    conf.keymap[int(gfxGuiKeyMap::X)] = SDLK_x;
    conf.keymap[int(gfxGuiKeyMap::Y)] = SDLK_y;
    conf.keymap[int(gfxGuiKeyMap::Z)] = SDLK_z;

    if (termite::coreInit(conf, update, getSDLWindowData(g_window))) {
        BX_FATAL(termite::errGetString());
        BX_VERBOSE(termite::errGetCallstack());
        termite::coreShutdown();
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }

    g_vg = vgCreateContext(100);
    g_debug = dbgCreateContext(101);
    camInit(&g_cam);
    camLookAt(&g_cam, vec3f(0, 1.0f, -12.0f), vec3f(0, 0, 0));

    gfxTextureLoadParams texParams;
    g_logo = dsLoadResource(nullptr, "texture", "textures/logo.dds", &texParams);
        
    while (sdlPollEvents()) {
        termite::coreFrame();
    }

    dbgDestroyContext(g_debug);
    vgDestroyContext(g_vg);
    termite::coreShutdown();
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}

