#pragma once

#include "bx/bx.h"

#include "gfx_defines.h"
#include "resource_lib.h"

namespace termite
{
    struct GfxDriverApi;

    // Use this structure to load texture with params
    struct LoadTextureParams
    {
        TextureFlag::Bits flags;
        TextureFormat::Enum fmt;        // Requested texture format
        uint8_t skipMips;
        bool generateMips;
        uint8_t padding[2];

        LoadTextureParams()
        {
            flags = 0;
            fmt = TextureFormat::RGBA8;
            skipMips = 0;
            generateMips = false;
            padding[0] = padding[1] = 0;
        }
    };

    struct Texture
    {
        TextureHandle handle;
        TextureInfo info;
        float ratio;

        Texture()
        {
            memset(&info, 0x00, sizeof(info));
            ratio = 0;
        }
    };

    result_t initTextureLoader(GfxDriverApi* driver, bx::AllocatorI* alloc, int texturePoolSize = 64);
    void shutdownTextureLoader();

    void registerTextureToResourceLib();

    TERMITE_API TextureHandle getWhiteTexture1x1();
    TERMITE_API TextureHandle getBlackTexture1x1();

    TERMITE_API bool blitRawPixels(uint8_t* dest, int destX, int destY, int destWidth, int destHeight, 
                                   const uint8_t* src, int srcX, int srcY, int srcWidth, int srcHeight,
                                   int pixelSize);
} // namespace termite
