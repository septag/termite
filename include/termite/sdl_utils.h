#pragma once

#include "bx/bx.h"

namespace termite
{
    struct Config;

    TERMITE_API void sdlGetNativeWindowHandle(SDL_Window* window, void** pWndHandle, void** pDisplayHandle = nullptr,
                                              void** pBackbuffer = nullptr);
    TERMITE_API bool sdlHandleEvent(const SDL_Event& ev);
    TERMITE_API void sdlMapImGuiKeys(Config* conf);
} // namespace termite
