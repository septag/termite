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
        bool generateMips;
        uint8_t skipMips;
        TextureFlag::Bits flags;

        LoadTextureParams()
        {
            generateMips = false;
            skipMips = 0;
            flags = TextureFlag::U_Clamp | TextureFlag::V_Clamp | 
                    TextureFlag::MinPoint | TextureFlag::MagPoint;
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

    result_t initTextureLoader(GfxDriverApi* driver, bx::AllocatorI* alloc, int texturePoolSize = 128);
    void shutdownTextureLoader();
    void registerTextureToResourceLib();
    TextureHandle getWhiteTexture1x1();

    TERMITE_API bool blitRawPixels(uint8_t* dest, int destX, int destY, int destWidth, int destHeight, 
                                   const uint8_t* src, int srcX, int srcY, int srcWidth, int srcHeight,
                                   int pixelSize);

} // namespace termite
