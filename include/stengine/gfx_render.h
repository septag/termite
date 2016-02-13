#pragma once

namespace st
{
    class gfxDriver;

    class BX_NO_VTABLE gfxRender
    {
    public:
        virtual int init(bx::AllocatorI* alloc, gfxDriver* driver, void* sdlWindow = nullptr) = 0;
        virtual void shutdown() = 0;
        virtual void render() = 0;
    };
} // namespace st

