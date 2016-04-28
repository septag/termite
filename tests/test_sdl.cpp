#include <SDL2/SDL.h>
#undef main
#include <SDL2/SDL_syswm.h>
#undef None

#include "termite/core.h"
#include "bxx/logger.h"
#include "bxx/path.h"

#include "termite/gfx_defines.h"

#include <conio.h>

SDL_Window* gWindow = nullptr;
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

static InputData gInput;

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800

using namespace termite;

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
            gInput.mouseWheel = 1.0f;
        else if (e.wheel.y < 0)
            gInput.mouseWheel = -1.0f;
        break;
    }

    case SDL_MOUSEBUTTONDOWN:
    {
        if (e.button.button == SDL_BUTTON_LEFT) gInput.mouseButtons[0] = 1;
        if (e.button.button == SDL_BUTTON_RIGHT) gInput.mouseButtons[1] = 1;
        if (e.button.button == SDL_BUTTON_MIDDLE) gInput.mouseButtons[2] = 1;
        break;
    }

    case SDL_TEXTINPUT:
        coreSendInputChars(e.text.text);
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        int key = e.key.keysym.sym & ~SDLK_SCANCODE_MASK;
        gInput.keysDown[key] = e.type == SDL_KEYDOWN;
        gInput.keyShift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        gInput.keyCtrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        gInput.keyAlt = (SDL_GetModState() & KMOD_ALT) != 0;
        coreSendInputKeys(gInput.keysDown, gInput.keyShift, gInput.keyAlt, gInput.keyCtrl);
        break;
    }

    default:
        break;
    }

    return true;
}

#include "bx/os.h"
#include "bx/thread.h"

static void update(float dt)
{
    int mx, my;
    uint32_t mouseMask = SDL_GetMouseState(&mx, &my);

    float mousePos[2] = { 0.0f, 0.0f };
    if (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_MOUSE_FOCUS) {
        mousePos[0] = (float)mx;
        mousePos[1] = (float)my;
    } else {
        mousePos[0] = -1.0f;
        mousePos[1] = -1.0f;
    }

    gInput.mouseButtons[0] = (mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT)) ? 1 : 0;
    gInput.mouseButtons[1] = (mouseMask & SDL_BUTTON(SDL_BUTTON_RIGHT)) ? 1 : 0;
    gInput.mouseButtons[2] = (mouseMask & SDL_BUTTON(SDL_BUTTON_MIDDLE)) ? 1 : 0;

    coreSendInputMouse(mousePos, gInput.mouseButtons, gInput.mouseWheel);

    gInput.mouseButtons[0] = gInput.mouseButtons[1] = gInput.mouseButtons[2] = 0;
    gInput.mouseWheel = 0.0f;
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

#	if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
    platform.ndt = wmi.info.x11.display;
    platform.nwh = (void*)(uintptr_t)wmi.info.x11.window;
#	elif BX_PLATFORM_OSX
    platform.ndt = NULL;
    platform.nwh = wmi.info.cocoa.window;
#	elif BX_PLATFORM_WINDOWS
    platform.ndt = NULL;
    platform.nwh = wmi.info.win.window;
#	endif // BX_PLATFORM_

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

    gWindow = SDL_CreateWindow("stTestSDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                               WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!gWindow) {
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

    if (termite::coreInit(conf, update, getSDLWindowData(gWindow))) {
        BX_FATAL(termite::errGetString());
        BX_VERBOSE(termite::errGetCallstack());
        termite::coreShutdown();
        SDL_DestroyWindow(gWindow);
        SDL_Quit();
        return -1;
    }

    while (sdlPollEvents()) {
        termite::coreFrame();
    }

    termite::coreShutdown();
    SDL_DestroyWindow(gWindow);
    SDL_Quit();

    return 0;
}

