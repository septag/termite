#include "pch.h"

#ifdef termite_SDL2
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>
#undef None

#include "sdl_utils.h"
#include "imgui.h"

using namespace termite;

struct InputState
{
    float mousePos[2];
    float mouseWheel;
    int mouseButtons[3];
    bool keyShift;
    bool keyAlt;
    bool keyCtrl;
    bool keysDown[512];

    InputState()
    {
        mouseWheel = 0;
        mouseButtons[0] = mouseButtons[1] = mouseButtons[2] = false;
        keyShift = keyAlt = keyCtrl = false;
        memset(keysDown, 0x00, sizeof(keysDown));
    }
};

static InputState g_input;

void termite::sdlGetNativeWindowHandle(SDL_Window* window, void** pWndHandle, void** pDisplayHandle, void** pBackbuffer)
{
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window, &wmi)) {
        BX_WARN("Could not fetch SDL window handle");
        return;
    }

    if (pBackbuffer)
        *pBackbuffer = NULL;
    if (pDisplayHandle)
        *pDisplayHandle = NULL;

#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
    if (pDisplayHandle)
        *pDisplayHandle = wmi.info.x11.display;
    *pWndHandle = (void*)(uintptr_t)wmi.info.x11.window;
#elif BX_PLATFORM_OSX
    *pWndHandle = wmi.info.cocoa.window;
#elif BX_PLATFORM_STEAMLINK
    if (pDisplayHandle)
        *pDisplayHandle = wmi.info.vivante.display;
    *pWndHandle = wmi.info.vivante.window;
#elif BX_PLATFORM_WINDOWS
    *pWndHandle = wmi.info.win.window;
#elif BX_PLATFORM_ANDROID
    *pWndHandle = wmi.info.android.window;
    if (pBackbuffer)
        *pBackbuffer = wmi.info.android.surface;
#endif // BX_PLATFORM_
}

bool termite::sdlHandleEvent(const SDL_Event& ev)
{
    switch (ev.type) {
    case SDL_MOUSEWHEEL:
    {
        if (ev.wheel.y > 0)
            g_input.mouseWheel = 1.0f;
        else if (ev.wheel.y < 0)
            g_input.mouseWheel = -1.0f;
        inputSendMouse(g_input.mousePos, g_input.mouseButtons, g_input.mouseWheel);
        return true;
    }

    case SDL_MOUSEBUTTONDOWN:
    {
        if (ev.button.button == SDL_BUTTON_LEFT) g_input.mouseButtons[0] = 1;
        if (ev.button.button == SDL_BUTTON_RIGHT) g_input.mouseButtons[1] = 1;
        if (ev.button.button == SDL_BUTTON_MIDDLE) g_input.mouseButtons[2] = 1;
        inputSendMouse(g_input.mousePos, g_input.mouseButtons, 0);
        return true;
    }

    case SDL_MOUSEBUTTONUP:
    {
        if (ev.button.button == SDL_BUTTON_LEFT) g_input.mouseButtons[0] = 0;
        if (ev.button.button == SDL_BUTTON_RIGHT) g_input.mouseButtons[1] = 0;
        if (ev.button.button == SDL_BUTTON_MIDDLE) g_input.mouseButtons[2] = 0;
        inputSendMouse(g_input.mousePos, g_input.mouseButtons, 0);
        return true;
    }

    case SDL_MOUSEMOTION:
    {
        g_input.mousePos[0] = float(ev.motion.x);
        g_input.mousePos[1] = float(ev.motion.y);
        inputSendMouse(g_input.mousePos, g_input.mouseButtons, 0);
        return true;
    }

    case SDL_TEXTINPUT:
        inputSendChars(ev.text.text);
        return true;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        int key = ev.key.keysym.sym & ~SDLK_SCANCODE_MASK;
        g_input.keysDown[key] = (ev.type == SDL_KEYDOWN);
        g_input.keyShift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        g_input.keyCtrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        g_input.keyAlt = (SDL_GetModState() & KMOD_ALT) != 0;
        inputSendKeys(g_input.keysDown, g_input.keyShift, g_input.keyAlt, g_input.keyCtrl);
        return true;
    }
    }

    return false;
}

void termite::sdlMapImGuiKeys(Config* conf)
{
    conf->keymap[ImGuiKey_Tab] = SDLK_TAB;
    conf->keymap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
    conf->keymap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
    conf->keymap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
    conf->keymap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
    conf->keymap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
    conf->keymap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
    conf->keymap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
    conf->keymap[ImGuiKey_End] = SDL_SCANCODE_END;
    conf->keymap[ImGuiKey_Delete] = SDLK_DELETE;
    conf->keymap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
    conf->keymap[ImGuiKey_Enter] = SDLK_RETURN;
    conf->keymap[ImGuiKey_Escape] = SDLK_ESCAPE;
    conf->keymap[ImGuiKey_A] = SDLK_a;
    conf->keymap[ImGuiKey_C] = SDLK_c;
    conf->keymap[ImGuiKey_V] = SDLK_v;
    conf->keymap[ImGuiKey_X] = SDLK_x;
    conf->keymap[ImGuiKey_Y] = SDLK_y;
    conf->keymap[ImGuiKey_Z] = SDLK_z;
}
#endif  // if termite_SDL2