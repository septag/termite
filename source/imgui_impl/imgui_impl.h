#pragma once

#include "imgui.h"
#include "ImGuizmo.h"

namespace tee
{
    int initImGui(uint8_t viewId, GfxDriver* driver, bx::AllocatorI* alloc, 
				  const int* keymap, const char* iniFilename = nullptr, void* nativeWindowHandle = nullptr);
    void shutdownImGui();
} // namespace tee
