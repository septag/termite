#include "pch.h"

#ifdef termite_SDL2
//#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>
#undef None

#include "bxx/array.h"

#include "sdl_utils.h"
#include "imgui.h"
#include "internal.h"

#if BX_PLATFORM_IOS
void* iosCreateNativeLayer(void* wnd);
#endif

namespace tee {
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
            bx::memSet(keysDown, 0x00, sizeof(keysDown));
            accel[0] = accel[1] = accel[2] = 0;
        }
    };

    static SdlState* gSDL = nullptr;

#if BX_PLATFORM_IOS
    static void* gLayerHandleIOS = nullptr;
#endif

#if BX_PLATFORM_ANDROID
#include <jni.h>
    extern "C" JNIEXPORT void JNICALL Java_com_termite_util_Platform_termiteSetAccelData(JNIEnv* env, jclass cls,
                                                                                         jfloat x, jfloat y, jfloat z)
    {
        BX_UNUSED(cls);

        if (gSDL) {
            gSDL->accel[0] = x;
            gSDL->accel[1] = y;
            gSDL->accel[2] = z;
        }
    }
#endif

    bool sdl::init(bx::AllocatorI* alloc)
    {
        if (gSDL) {
            BX_ASSERT(0);
            return false;
        }

        gSDL = BX_NEW(alloc, SdlState)(alloc);
        if (!gSDL)
            return false;
        if (!gSDL->shortcutKeys.create(16, 32, alloc))
            return false;
        return true;
    }

    void sdl::shutdown()
    {
        if (!gSDL)
            return;
        gSDL->shortcutKeys.destroy();
        BX_DELETE(gSDL->alloc, gSDL);
        gSDL = nullptr;
    }

    void sdl::getNativeWindowHandle(SDL_Window* window, void** pWndHandle, void** pDisplayHandle, void** pBackbuffer)
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
        wmi.info.cocoa
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
#elif BX_PLATFORM_IOS
        if (!gLayerHandleIOS)
            gLayerHandleIOS = iosCreateNativeLayer(wmi.info.uikit.window);
        *pWndHandle = gLayerHandleIOS;
#endif // BX_PLATFORM_
    }

    static void processShortcutKeys()
    {
        BX_ASSERT(gSDL);
        for (int i = 0, c = gSDL->shortcutKeys.getCount(); i < c; i++) {
            const ShortcutKey& sk = gSDL->shortcutKeys[i];
            if (gSDL->keysDown[sk.key] && sk.modKeys == gSDL->modKeys) {
                BX_ASSERT(sk.callback);
                sk.callback(sk.userData);
            }
        }
    }

    bool sdl::handleEvent(SDL_Event* ev, bool wait)
    {
        int r = !wait ? SDL_PollEvent(ev) : SDL_WaitEvent(ev);
        if (r) {
            switch (ev->type) {
            case SDL_MOUSEWHEEL:
            {
                if (ev->wheel.y > 0)
                    gSDL->mouseWheel = 1.0f;
                else if (ev->wheel.y < 0)
                    gSDL->mouseWheel = -1.0f;
                inputSendMouse(gSDL->mousePos, gSDL->mouseButtons, gSDL->mouseWheel);
                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            {
                if (ev->button.button == SDL_BUTTON_LEFT) gSDL->mouseButtons[0] = 1;
                if (ev->button.button == SDL_BUTTON_RIGHT) gSDL->mouseButtons[1] = 1;
                if (ev->button.button == SDL_BUTTON_MIDDLE) gSDL->mouseButtons[2] = 1;
                inputSendMouse(gSDL->mousePos, gSDL->mouseButtons, 0);
                break;
            }

            case SDL_MOUSEBUTTONUP:
            {
                if (ev->button.button == SDL_BUTTON_LEFT) gSDL->mouseButtons[0] = 0;
                if (ev->button.button == SDL_BUTTON_RIGHT) gSDL->mouseButtons[1] = 0;
                if (ev->button.button == SDL_BUTTON_MIDDLE) gSDL->mouseButtons[2] = 0;
                inputSendMouse(gSDL->mousePos, gSDL->mouseButtons, 0);
                break;
            }

            case SDL_MOUSEMOTION:
            {
                gSDL->mousePos[0] = float(ev->motion.x);
                gSDL->mousePos[1] = float(ev->motion.y);
                inputSendMouse(gSDL->mousePos, gSDL->mouseButtons, 0);
                break;
            }

            case SDL_TEXTINPUT:
                inputSendChars(ev->text.text);
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                int key = ev->key.keysym.sym & ~SDLK_SCANCODE_MASK;
                gSDL->keysDown[key] = (ev->type == SDL_KEYDOWN);
                gSDL->modKeys = 0;
                bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
                bool alt = (SDL_GetModState() & KMOD_ALT) != 0;
                inputSendKeys(gSDL->keysDown, shift, alt, ctrl);
                if (shift)  gSDL->modKeys |= ModifierKey::Shift;
                if (ctrl)   gSDL->modKeys |= ModifierKey::Ctrl;
                if (alt)    gSDL->modKeys |= ModifierKey::Alt;

                if (ev->type == SDL_KEYDOWN)
                    processShortcutKeys();
                break;
            }
            }

            return true;
        }

        return false;
    }

    void sdl::mapImGuiKeys(Config* conf)
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

    void sdl::getAccelState(float* accel)
    {
        accel[0] = gSDL->accel[0];
        accel[1] = gSDL->accel[1];
        accel[2] = gSDL->accel[2];
    }

    bool sdl::isKeyPressed(SDL_Keycode vkey)
    {
        return gSDL->keysDown[vkey & ~SDLK_SCANCODE_MASK];
    }

    void sdl::registerShortcutKey(SDL_Keycode vkey, ModifierKey::Bits modKeys, ShortcutKeyCallback callback, void* userData)
    {
        BX_ASSERT(gSDL);
        BX_ASSERT(callback);

        // Search previous shortcuts for a duplicate, if found just change the callback
        int index = gSDL->shortcutKeys.find([vkey, modKeys](const ShortcutKey& sk) {
            if (sk.key == vkey && sk.modKeys == modKeys)
                return true;
            return false;
        });

        if (index == -1) {
            ShortcutKey* sk = gSDL->shortcutKeys.push();
            if (sk) {
                sk->key = vkey & ~SDLK_SCANCODE_MASK;
                sk->modKeys = modKeys;
                sk->callback = callback;
                sk->userData = userData;
            }
        } else {
            ShortcutKey* sk = gSDL->shortcutKeys.itemPtr(index);
            sk->callback = callback;
            sk->userData = userData;
        }
    }

    SDL_Window* sdl::createWindow(const char* name, int x, int y, int width, int height, uint32_t* pSdlWindowFlags)
    {
        uint32_t windowFlags = SDL_WINDOW_SHOWN;

        // Mobile and desktop flags are different
        if (BX_ENABLED(BX_PLATFORM_IOS | BX_PLATFORM_ANDROID)) {
            windowFlags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN_DESKTOP;

            if (BX_ENABLED(BX_PLATFORM_IOS))
                windowFlags |= SDL_WINDOW_ALLOW_HIGHDPI;
            
            width = height = 0;
        } else {
            bool maximized = pSdlWindowFlags ? ((*pSdlWindowFlags & SDL_WINDOW_MAXIMIZED) ? true : false) : false;
            if (width == 0 || height == 0 || maximized)
                windowFlags |= SDL_WINDOW_MAXIMIZED;

            if (!pSdlWindowFlags || !((*pSdlWindowFlags) & SDL_WINDOW_FULLSCREEN_DESKTOP)) {
                windowFlags |= SDL_WINDOW_RESIZABLE;
            }

            if (pSdlWindowFlags) {
                windowFlags |= (*pSdlWindowFlags);
            }
        }

        SDL_Window* wnd = SDL_CreateWindow(name,
                                           x == 0 ? SDL_WINDOWPOS_UNDEFINED : x,
                                           y == 0 ? SDL_WINDOWPOS_UNDEFINED : y,
                                           width, height, windowFlags);

        if (pSdlWindowFlags)
            *pSdlWindowFlags = windowFlags & (~SDL_WINDOW_MAXIMIZED);
        return wnd;
    }
} // namespace tee
#endif  // if termite_SDL2
