#pragma once

namespace termite
{
    class gfxDriver;
    struct gfxPlatformData;

    class BX_NO_VTABLE gfxRender
    {
    public:
        virtual int init(bx::AllocatorI* alloc, gfxDriver* driver, const gfxPlatformData* platformData = nullptr,
                         const int* uiKeymap = nullptr) = 0;
        virtual void shutdown() = 0;
        virtual void render() = 0;
        virtual void frame() = 0;

        virtual void sendImInputMouse(float mousePos[2], int mouseButtons[3], float mouseWheel) = 0;
        virtual void sendImInputKeys(const bool keysDown[512], bool shift, bool alt, bool ctrl);
        virtual void sendImInputChars(const char* chars) = 0;
    };
} // namespace st

