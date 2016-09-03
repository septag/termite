#pragma once

#include "bx/bx.h"

namespace termite
{
    TERMITE_API void sdlGetNativeWindowHandle(SDL_Window* window, void** pWndHandle, void** pDisplayHandle = nullptr);
    TERMITE_API bool sdlHandleEvent(const SDL_Event& ev);
    TERMITE_API void sdlMapImGuiKeys(int keymap[19]);
} // namespace termite
