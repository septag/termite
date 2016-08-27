#pragma once

#include "imgui.h"

namespace termite
{
    int initImGui(uint8_t viewId, uint16_t viewWidth, uint16_t viewHeight, GfxDriverApi* driver, bx::AllocatorI* alloc, 
				  const int* keymap, const char* iniFilename = nullptr, void* nativeWindowHandle = nullptr);
    void shutdownImGui();
} // namespace termite
