#pragma once

#include "bx/bx.h"

#include "gfx_defines.h"
#include "datastore.h"

namespace termite
{
    class gfxDriverI;

    // Use this structure to load texture with params
    struct gfxTextureLoadParams
    {
        bool generateMips;
        uint8_t skipMips;
        gfxTextureFlag flags;

        gfxTextureLoadParams()
        {
            generateMips = false;
            skipMips = 0;
            flags = gfxTextureFlag::U_Clamp | gfxTextureFlag::V_Clamp | 
                    gfxTextureFlag::MinPoint | gfxTextureFlag::MagPoint;
        }
    };

    struct gfxTexture
    {
        gfxTextureHandle handle;
        gfxTextureInfo info;

        gfxTexture()
        {
            handle = T_INVALID_HANDLE;
            memset(&info, 0x00, sizeof(info));
        }
    };

    result_t textureInitLoader(gfxDriverI* driver, bx::AllocatorI* alloc, int texturePoolSize = 128);
    void textureRegisterToDatastore(dsDataStore* ds);

    void textureShutdownLoader();
    gfxTextureHandle textureGetWhite1x1();

} // namespace termite
