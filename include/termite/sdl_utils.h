#pragma once

#include "bx/bx.h"

namespace tee
{
    struct Config;

    struct ModifierKey
    {
        enum Enum
        {
            Shift = 0x1,
            Ctrl = 0x2,
            Alt = 0x4
        };

        typedef uint8_t Bits;
    };

    typedef void (*ShortcutKeyCallback)(void* userData);

    namespace sdl {
        TEE_API void getNativeWindowHandle(SDL_Window* window, void** pWndHandle, void** pDisplayHandle = nullptr,
                                           void** pBackbuffer = nullptr);
        TEE_API bool handleEvent(SDL_Event* ev, bool wait = false);
        TEE_API void mapImGuiKeys(Config* conf);
        TEE_API void getAccelState(float* accel);
        TEE_API bool isKeyPressed(SDL_Keycode vkey);

        // Register shortcut keys, mostly for tools and editors
        // vkey: SDLK_xxx 
        TEE_API void registerShortcutKey(SDL_Keycode vkey, ModifierKey::Bits modKeys, ShortcutKeyCallback callback,
                                         void* userData = nullptr);

        /// If width=0 OR height=0, show window maximized
        TEE_API SDL_Window* createWindow(const char* name, int x, int y, int width = 0, int height = 0,
                                         uint32_t* pSdlWindowFlags = nullptr);
    } // namespace sdl
} // namespace tee
