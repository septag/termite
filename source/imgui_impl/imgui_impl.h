#pragma once

#include "imgui.h"

namespace termite
{
    int imguiInit(uint8_t viewId, uint16_t viewWidth, uint16_t viewHeight, gfxDriverI* driver, const int* keymap = nullptr);
    void imguiShutdown();

    void imguiRender();
    void imguiNewFrame();
} // namespace st
