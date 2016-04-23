#pragma once

#include "imgui/imgui.h"

namespace termite
{
    int imguiInit(uint16_t viewWidth, uint16_t viewHeight, gfxDriver* driver, const int* keymap = nullptr);
    void imguiShutdown();

    void imguiRender();
    void imguiNewFrame();
} // namespace st
