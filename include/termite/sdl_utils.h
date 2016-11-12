#pragma once

#include "bx/bx.h"

namespace termite
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

    result_t initSdlUtils(bx::AllocatorI* alloc);
    void shutdownSdlUnits();

    TERMITE_API void sdlGetNativeWindowHandle(SDL_Window* window, void** pWndHandle, void** pDisplayHandle = nullptr,
                                              void** pBackbuffer = nullptr);
    TERMITE_API bool sdlHandleEvent(SDL_Event* ev, bool wait = false);
    TERMITE_API void sdlMapImGuiKeys(Config* conf);

    // Register shortcut keys, mostly for tools and editors
    // vkey: SDLK_xxx 
    TERMITE_API void sdlRegisterShortcutKey(SDL_Keycode vkey, ModifierKey::Bits modKeys, ShortcutKeyCallback callback,
                                            void* userData = nullptr);
} // namespace termite
