#pragma once

#include "bx/bx.h"

namespace termite
{
    struct GfxApi;
    struct GfxPlatformData;

    class BX_NO_VTABLE RendererI
    {
    public:
        virtual result_t init(bx::AllocatorI* alloc, GfxApi* driver) = 0;
        virtual void shutdown() = 0;
        virtual void render(const void* renderData) = 0;
    };
} // namespace termite

