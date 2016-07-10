#pragma once

#include "bx/bx.h"

namespace termite
{
    struct GfxDriverApi;
    struct GfxPlatformData;

    struct RendererApi
    {
		result_t(*init)(bx::AllocatorI* alloc, GfxDriverApi* driver);
		void(*shutdown)();
		void(*render)(const void* renderData);
    };
} // namespace termite

