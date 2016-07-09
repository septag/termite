#pragma once

#include "bx/bx.h"

#include "gfx_defines.h"
#include "resource_lib.h"

namespace termite
{
    struct GfxApi;

    // Use this structure to load texture with params
    struct LoadTextureParams
    {
        bool generateMips;
        uint8_t skipMips;
        TextureFlag flags;

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

        Texture()
        {
            memset(&info, 0x00, sizeof(info));
        }
    };

    result_t initTextureLoader(GfxApi* driver, bx::AllocatorI* alloc, int texturePoolSize = 128);
    void shutdownTextureLoader();
    void registerTextureToResourceLib(ResourceLib* resLib);
    TextureHandle getWhiteTexture1x1();

} // namespace termite
