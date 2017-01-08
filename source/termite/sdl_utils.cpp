#include "pch.h"

#ifdef termite_SDL2
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>
#undef None

#include "bxx/array.h"

#include "sdl_utils.h"
#include "imgui.h"

using namespace termite;

struct ShortcutKey
{
    uint32_t key;
    ModifierKey::Bits modKeys;
    ShortcutKeyCallback callback;
    void* userData;
};

struct SdlState
{
    bx::AllocatorI* alloc;
    float mousePos[2];
    float mouseWheel;
    int mouseButtons[3];
    ModifierKey::Bits modKeys;
    bool keysDown[512];
    bx::Array<ShortcutKey> shortcutKeys;
    float accel[3];

    SdlState(bx::AllocatorI* _alloc) : alloc(_alloc)
    {
        mouseWheel = 0;
        mouseButtons[0] = mouseButtons[1] = mouseButtons[2] = false;
        modKeys = 0;
        memset(keysDown, 0x00, sizeof(keysDown));
        accel[0] = accel[1] = accel[2] = 0;
    }
};

static SdlState* g_sdl = nullptr;

#if BX_PLATFORM_ANDROID
#include <jni.h>
extern "C" JNIEXPORT void JNICALL Java_com_termite_utils_PlatformUtils_termiteSetAccelData(JNIEnv* env, jclass cls,
                                                                                           jfloat x, jfloat y, jfloat z)
{
    BX_UNUSED(cls);

    if (g_sdl) {
        g_sdl->accel[0] = x;
        g_sdl->accel[1] = y;
        g_sdl->accel[2] = z;
    }
}
#endif

result_t termite::initSdlUtils(bx::AllocatorI* alloc)
{
    if (g_sdl) {
        assert(0);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_sdl = BX_NEW(alloc, SdlState)(alloc);
    if (!g_sdl)
        return T_ERR_OUTOFMEM;
    if (!g_sdl->shortcutKeys.create(16, 32, alloc))
        return T_ERR_OUTOFMEM;

    return 0;
}

void termite::shutdownSdlUnits()
{
    if (!g_sdl)
        return;
    g_sdl->shortcutKeys.destroy();
    BX_DELETE(g_sdl->alloc, g_sdl);
    g_sdl = nullptr;
}

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

static void processShortcutKeys()
{
    assert(g_sdl);
    for (int i = 0, c = g_sdl->shortcutKeys.getCount(); i < c; i++) {
        const ShortcutKey& sk = g_sdl->shortcutKeys[i];
        if (g_sdl->keysDown[sk.key] && sk.modKeys == g_sdl->modKeys) {
            assert(sk.callback);
            sk.callback(sk.userData);
        }
    }
}

bool termite::sdlHandleEvent(SDL_Event* ev, bool wait)
{
    int r = !wait ? SDL_PollEvent(ev) : SDL_WaitEvent(ev);
    if (r) {
        switch (ev->type) {
            case SDL_MOUSEWHEEL:
            {
                if (ev->wheel.y > 0)
                    g_sdl->mouseWheel = 1.0f;
                else if (ev->wheel.y < 0)
                    g_sdl->mouseWheel = -1.0f;
                inputSendMouse(g_sdl->mousePos, g_sdl->mouseButtons, g_sdl->mouseWheel);
                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            {
                if (ev->button.button == SDL_BUTTON_LEFT) g_sdl->mouseButtons[0] = 1;
                if (ev->button.button == SDL_BUTTON_RIGHT) g_sdl->mouseButtons[1] = 1;
                if (ev->button.button == SDL_BUTTON_MIDDLE) g_sdl->mouseButtons[2] = 1;
                inputSendMouse(g_sdl->mousePos, g_sdl->mouseButtons, 0);
                break;
            }

            case SDL_MOUSEBUTTONUP:
            {
                if (ev->button.button == SDL_BUTTON_LEFT) g_sdl->mouseButtons[0] = 0;
                if (ev->button.button == SDL_BUTTON_RIGHT) g_sdl->mouseButtons[1] = 0;
                if (ev->button.button == SDL_BUTTON_MIDDLE) g_sdl->mouseButtons[2] = 0;
                inputSendMouse(g_sdl->mousePos, g_sdl->mouseButtons, 0);
                break;
            }

            case SDL_MOUSEMOTION:
            {
                g_sdl->mousePos[0] = float(ev->motion.x);
                g_sdl->mousePos[1] = float(ev->motion.y);
                inputSendMouse(g_sdl->mousePos, g_sdl->mouseButtons, 0);
                break;
            }

            case SDL_TEXTINPUT:
            inputSendChars(ev->text.text);
            break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                int key = ev->key.keysym.sym & ~SDLK_SCANCODE_MASK;
                g_sdl->keysDown[key] = (ev->type == SDL_KEYDOWN);
                g_sdl->modKeys = 0;
                bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
                bool alt = (SDL_GetModState() & KMOD_ALT) != 0;
                inputSendKeys(g_sdl->keysDown, shift, alt, ctrl);
                if (shift)  g_sdl->modKeys |= ModifierKey::Shift;
                if (ctrl)   g_sdl->modKeys |= ModifierKey::Ctrl;
                if (alt)    g_sdl->modKeys |= ModifierKey::Alt;

                if (ev->type == SDL_KEYDOWN)
                    processShortcutKeys();
                break;
            }
        }

        return true;
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

void termite::sdlGetAccelState(float* accel)
{
    accel[0] = g_sdl->accel[0];
    accel[1] = g_sdl->accel[1];
    accel[2] = g_sdl->accel[2];
}

void termite::sdlRegisterShortcutKey(SDL_Keycode vkey, ModifierKey::Bits modKeys, ShortcutKeyCallback callback, void* userData)
{
    assert(g_sdl);
    assert(callback);
    
    // Search previous shortcuts for a duplicate, if found just change the callback
    int index = g_sdl->shortcutKeys.find([vkey, modKeys](const ShortcutKey& sk) { 
        if (sk.key == vkey && sk.modKeys == modKeys)
            return true; 
        return false; 
    });

    if (index == -1) {
        ShortcutKey* sk = g_sdl->shortcutKeys.push();
        if (sk) {
            sk->key = vkey & ~SDLK_SCANCODE_MASK;
            sk->modKeys = modKeys;
            sk->callback = callback;
            sk->userData = userData;
        }
    } else {
        ShortcutKey* sk = g_sdl->shortcutKeys.itemPtr(index);
        sk->callback = callback;
        sk->userData = userData;
    }
}

#endif  // if termite_SDL2