#include "pch.h"

#ifdef _WIN32
#  define HAVE_STDINT_H 1
#endif
#include <SDL.h>
#undef main
#include <SDL_syswm.h>
#undef None

#include "sdl_utils.h"
#include "imgui.h"

using namespace termite;

struct InputState
{
    float mouseWheel;
    bool mouseButtons[3];
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

void termite::sdlGetNativeWindowHandle(SDL_Window* window, void** pWndHandle, void** pDisplayHandle)
{
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window, &wmi)) {
        BX_WARN("Could not fetch SDL window handle");
        return;
    }

#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
    if (pDisplayHandle)
        *pDisplayHandle = wmi.info.x11.display;
    *pWndHandle = (void*)(uintptr_t)wmi.info.x11.window;
#elif BX_PLATFORM_OSX
    if (pDisplayHandle)
        *pDisplayHandle = NULL;
    *pWndHandle = wmi.info.cocoa.window;
#elif BX_PLATFORM_WINDOWS
    if (pDisplayHandle)
        *pDisplayHandle = NULL;
    *pWndHandle = wmi.info.win.window;
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
        return true;
    }

    case SDL_MOUSEBUTTONDOWN:
    {
        if (ev.button.button == SDL_BUTTON_LEFT) g_input.mouseButtons[0] = 1;
        if (ev.button.button == SDL_BUTTON_RIGHT) g_input.mouseButtons[1] = 1;
        if (ev.button.button == SDL_BUTTON_MIDDLE) g_input.mouseButtons[2] = 1;
        return true;
    }

    case SDL_TEXTINPUT:
        inputSendChars(ev.text.text);
        return true;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        int key = ev.key.keysym.sym & ~SDLK_SCANCODE_MASK;
        g_input.keysDown[key] = ev.type == SDL_KEYDOWN;
        g_input.keyShift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        g_input.keyCtrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        g_input.keyAlt = (SDL_GetModState() & KMOD_ALT) != 0;
        inputSendKeys(g_input.keysDown, g_input.keyShift, g_input.keyAlt, g_input.keyCtrl);
        return true;
    }
    }

    return false;
}

void termite::sdlMapImGuiKeys(int keymap[19])
{
    keymap[ImGuiKey_Tab] = SDLK_TAB;
    keymap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
    keymap[ImGuiKey_RightArrow] = SDL_SCANCODE_DOWN;
    keymap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
    keymap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
    keymap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
    keymap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
    keymap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
    keymap[ImGuiKey_End] = SDL_SCANCODE_END;
    keymap[ImGuiKey_Delete] = SDLK_DELETE;
    keymap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
    keymap[ImGuiKey_Enter] = SDLK_RETURN;
    keymap[ImGuiKey_Escape] = SDLK_ESCAPE;
    keymap[ImGuiKey_A] = SDLK_a;
    keymap[ImGuiKey_C] = SDLK_c;
    keymap[ImGuiKey_V] = SDLK_v;
    keymap[ImGuiKey_X] = SDLK_x;
    keymap[ImGuiKey_Y] = SDLK_y;
    keymap[ImGuiKey_Z] = SDLK_z;
}
