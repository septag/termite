#pragma once

#include "bx/bx.h"

namespace tee
{
    struct GfxPlatformData;
    struct GfxDriver;

    struct RendererApi
    {
		bool (*init)(bx::AllocatorI* alloc, GfxDriver* driver);
		void(*shutdown)();
		void(*render)(const void* renderData);
    };
} // namespace tee

