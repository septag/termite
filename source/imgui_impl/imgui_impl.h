#pragma once

#include "imgui.h"
#include "ImGuizmo.h"

namespace termite
{
    int initImGui(uint8_t viewId, GfxDriverApi* driver, bx::AllocatorI* alloc, 
				  const int* keymap, const char* iniFilename = nullptr, void* nativeWindowHandle = nullptr);
    void shutdownImGui();
} // namespace termite
