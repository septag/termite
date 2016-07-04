#pragma once

#include "imgui.h"

namespace termite
{
    int imguiInit(uint8_t viewId, uint16_t viewWidth, uint16_t viewHeight, GfxDriverI* driver, const int* keymap = nullptr);
    void imguiShutdown();
} // namespace termite
