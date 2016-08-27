#pragma once

#include "bx/bx.h"

#include "gfx_driver.h"
#include "resource_lib.h"
#include "vec_math.h"

#define T_SPRITE_MEMORY_TAG 0x24c7a709645feb5b 

namespace termite
{
    struct Camera2D;
    struct SpriteT {};
    typedef PhantomType<uint16_t, SpriteT, UINT16_MAX> SpriteHandle;

    struct SpriteAnimClip
    {
        char name[32];
        float fps;
        int startFrame;
        int endFrame;
    };

    struct SpriteSheet
    {
        ResourceHandle textureHandle;
        int numRegions;
        int numAnimClips;
        vec4_t* regions;
        SpriteAnimClip* clips;
    };

    typedef void(*SetSpriteStateCallback)(GfxDriverApi* driver);

    // SpriteCache is usually used with static sprites
    struct SpriteCache;

    result_t initSpriteSystem(GfxDriverApi* driver, bx::AllocatorI* alloc, uint16_t poolSize = 256);
    void shutdownSpriteSystem();

    //
    // localAnchor: Relative to sprite's center. MaxTopLeft(-0.5, 0.5), MaxBottomRight(0.5, -0.5)
    TERMITE_API SpriteHandle createSpriteFromTexture(ResourceHandle textureHandle, const vec2_t halfSize,
                                                     const vec2_t localAnchor = vec2f(0, 0),
                                                     const vec2_t topLeftCoords = vec2f(0, 0),
                                                     const vec2_t bottomRightCoords = vec2f(1.0f, 1.0f));
    TERMITE_API SpriteHandle createSpriteFromSpritesheet(ResourceHandle spritesheetHandle, const vec2_t halfSize,
                                                         const vec2_t localAnchor = vec2f(0, 0), int index = -1);
    TERMITE_API void destroySprite(SpriteHandle handle);


    TERMITE_API SpriteCache* createSpriteCache(int maxSprites);
    TERMITE_API void destroySpriteCache(SpriteCache* spriteCache);
    TERMITE_API void updateSpriteCache(const SpriteHandle* sprites, int numSprites, const vec2_t* posBuffer,
                                       const float* rotBuffer, const color_t* tintColors);

    TERMITE_API void drawSprites(uint8_t viewId, const SpriteHandle* sprites, int numSprites,
                                 const vec2_t* posBuffer, const float* rotBuffer, const color_t* tintColors,
                                 ProgramHandle prog = ProgramHandle(), SetSpriteStateCallback stateCallback = nullptr);
    TERMITE_API void drawSprites(uint8_t viewId, SpriteCache* spriteCache);

    // Registers "spritesheet" resource type and Loads SpriteSheet object
    void registerSpriteSheetToResourceLib(ResourceLib* resLib);

    struct LoadSpriteSheetParams
    {
        bool generateMips;
        uint8_t skipMips;
        TextureFlag flags;

        LoadSpriteSheetParams()
        {
            generateMips = false;
            skipMips = 0;
            flags = TextureFlag::U_Clamp | TextureFlag::V_Clamp |
                TextureFlag::MinPoint | TextureFlag::MagPoint;
        }
    };

} // namespace termite
