#pragma once

#include "bx/bx.h"

#include "gfx_defines.h"
#include "assetlib.h"

namespace tee
{
    struct GfxDriver;

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
            bx::memSet(&info, 0x00, sizeof(info));
            ratio = 0;
        }
    };

    namespace gfx {
        TEE_API TextureHandle getWhiteTexture1x1();
        TEE_API TextureHandle getBlackTexture1x1();

        TEE_API bool blitRawPixels(uint8_t* dest, int destX, int destY, int destWidth, int destHeight,
                                   const uint8_t* src, int srcX, int srcY, int srcWidth, int srcHeight,
                                   int pixelSize);
        TEE_API void saveTextureCache();
        TEE_API bool saveTexturePng(char const *filename, int w, int h, int comp, const void  *data, int stride_in_bytes);
        TEE_API bool resizeTexture(const uint8_t* input_pixels, int input_w, int input_h, int input_stride_in_bytes,
                                   uint8_t* output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                   int num_channels);
    }
} // namespace tee
