#pragma once

//
// Issues: 
//   1 - Rotatable sprite sheets are buggy, For now they can be used with no-animation sprites

#include "bx/bx.h"

#include "gfx_driver.h"
#include "resource_lib.h"
#include "vec_math.h"

#include <functional>

namespace termite
{
    struct Sprite;
    struct SpriteCache;

    struct SpriteFlag
    {
        enum Enum
        {
            DestroyResource = 0x1,
            None = 0
        };

        typedef uint8_t Bits;
    };

    struct SpriteFlip
    {
        enum Enum
        {
            FlipX = 0x1,
            FlipY = 0x2,
        };

        typedef uint8_t Bits;
    };

    // Callback for setting custom states when drawing sprites
    typedef void(*SetSpriteStateCallback)(GfxDriverApi* driver, void* userData);
    // Callback for animation frames
    typedef void(*SpriteFrameCallback)(Sprite* sprite, int frameIdx, void* userData);

    // Allocator can be heap, it's used to created memory pools and initial buffers
    result_t initSpriteSystem(GfxDriverApi* driver, bx::AllocatorI* alloc);
    void shutdownSpriteSystem();

    TERMITE_API Sprite* createSprite(bx::AllocatorI* alloc, const vec2_t halfSize);
    TERMITE_API void destroySprite(Sprite* sprite);

    // Pivot: Relative to sprite's center. MaxTopLeft(-0.5, 0.5), MaxBottomRight(0.5, -0.5)
    TERMITE_API void addSpriteFrameTexture(Sprite* sprite,
                                           ResourceHandle texHandle,
                                           SpriteFlag::Bits flags = 0,
                                           const vec2_t pivot = vec2f(0, 0),
                                           const vec2_t topLeftCoords = vec2f(0, 0),
                                           const vec2_t bottomRightCoords = vec2f(1.0f, 1.0f),
                                           const char* frameTag = nullptr);
    TERMITE_API void addSpriteFrameSpritesheet(Sprite* sprite,
                                               ResourceHandle ssHandle, 
                                               const char* name,
                                               SpriteFlag::Bits flags = 0,
                                               const char* frameTag = nullptr);
    TERMITE_API void addSpriteFrameAll(Sprite* sprite, ResourceHandle ssHandle, SpriteFlag::Bits flags);

    inline Sprite* createSpriteFromTexture(bx::AllocatorI* alloc,
                                           const vec2_t& halfSize, 
                                           ResourceHandle texHandle, 
                                           SpriteFlag::Bits flags = 0,
                                           const vec2_t& pivot = vec2f(0, 0),
                                           const vec2_t& topLeftCoords = vec2f(0, 0),
                                           const vec2_t& bottomRightCoords = vec2f(1.0f, 1.0f))
    {
        Sprite* s = createSprite(alloc, halfSize);
        if (s)
            addSpriteFrameTexture(s, texHandle, flags, pivot, topLeftCoords, bottomRightCoords);
        return s;
    }

    inline Sprite* createSpriteFromSpritesheet(bx::AllocatorI* alloc,
                                               const vec2_t& halfSize, 
                                               ResourceHandle ssHandle,
                                               const char* name,
                                               SpriteFlag::Bits flags = 0)
    {
        Sprite* s = createSprite(alloc, halfSize);
        if (s)
            addSpriteFrameSpritesheet(s, ssHandle, name, flags);
        return s;
    }

    // Animate sprites in loop
    TERMITE_API void animateSprites(Sprite** sprites, uint16_t numSprites, float dt);
    inline void animateSprite(Sprite* sprite, float dt)
    {
        animateSprites(&sprite, 1, dt);
    }
    TERMITE_API void invertSpriteAnim(Sprite* sprite);
    TERMITE_API void setSpriteAnimSpeed(Sprite* sprite, float speed);
    TERMITE_API float getSpriteAnimSpeed(Sprite* sprite);
    TERMITE_API void pauseSpriteAnim(Sprite* sprite);
    TERMITE_API void resumeSpriteAnim(Sprite* sprite);
    TERMITE_API void stopSpriteAnim(Sprite* sprite);
    TERMITE_API void replaySpriteAnim(Sprite* sprite);
    
    // Set frame callbacks: Frame callbacks are called when sprite animation reaches them
    TERMITE_API void setSpriteFrameCallbackByTag(Sprite* sprite, const char* frameTag, SpriteFrameCallback callback,
                                                 void* userData);
    TERMITE_API void setSpriteFrameCallbackByName(Sprite* sprite, const char* name, SpriteFrameCallback callback,
                                                  void* userData);
    TERMITE_API void setSpriteFrameCallbackByIndex(Sprite* sprite, int frameIdx, SpriteFrameCallback callback,
                                                   void* userData);
    TERMITE_API void setSpriteFrameEndCallback(Sprite* sprite, SpriteFrameCallback callback, void* userData);

    // Set/Get Sprite props
    TERMITE_API void setSpriteHalfSize(Sprite* sprite, const vec2_t& halfSize);
    TERMITE_API void setSpriteSizeMultiplier(Sprite* sprite, const vec2_t& sizeMultiplier);
    TERMITE_API void gotoSpriteFrameIndex(Sprite* sprite, int frameIdx);
    TERMITE_API void gotoSpriteFrameName(Sprite* sprite, const char* name);
    TERMITE_API void gotoSpriteFrameTag(Sprite* sprite, const char* frameTag);
    TERMITE_API int getSpriteFrameIndex(Sprite* sprite);
    TERMITE_API int getSpriteFrameCount(Sprite* sprite);
    TERMITE_API void setSpriteFrameIndex(Sprite* sprite, int index);
    TERMITE_API void setSpriteFlip(Sprite* sprite, SpriteFlip::Bits flip);
    TERMITE_API SpriteFlip::Bits getSpriteFlip(Sprite* sprite);
    TERMITE_API void setSpritePosOffset(Sprite* sprite, const vec2_t posOffset);
    TERMITE_API vec2_t getSpritePosOffset(Sprite* sprite);
    TERMITE_API void setSpriteCurFrameTag(Sprite* sprite, const char* frameTag);
    TERMITE_API void setSpriteOrder(Sprite* sprite, uint8_t order); // higher orders gets to draw on top
    TERMITE_API int getSpriteOrder(Sprite* sprite);
    TERMITE_API void setSpritePivot(Sprite* sprite, const vec2_t pivot);
    TERMITE_API void setSpriteTintColor(Sprite* sprite, color_t color);
    TERMITE_API color_t getSpriteTintColor(Sprite* sprite);
    TERMITE_API rect_t getSpriteDrawRect(Sprite* sprite);
    TERMITE_API void getSpriteRealRect(Sprite* sprite, vec2_t* pHalfSize, vec2_t* pCenter);
    TERMITE_API vec2_t getSpriteImageSize(Sprite* sprite);
    TERMITE_API rect_t getSpriteTexelRect(Sprite* sprite);
    TERMITE_API termite::ProgramHandle getSpriteColorAddProgram();
    TERMITE_API void setSpriteUserData(Sprite* sprite, void* userData);
    TERMITE_API void* getSpriteUserData(Sprite* sprite);

    // For manual rendering of spritesheet frames
    TERMITE_API void getSpriteFrameDrawData(Sprite* sprite, int frameIdx, rect_t* drawRect, rect_t* textureRect, 
                                            ResourceHandle* textureHandle);

    TERMITE_API void convertSpritePhysicsVerts(vec2_t* ptsOut, const vec2_t* ptsIn, int numPts, Sprite* sprite);

    // Dynamically draw sprites
    TERMITE_API void drawSprites(uint8_t viewId, Sprite** sprites, uint16_t numSprites, const mtx3x3_t* mats,
                                 ProgramHandle progOverride = ProgramHandle(), SetSpriteStateCallback stateCallback = nullptr,
                                 void* stateUserData = nullptr);
    inline void drawSprite(uint8_t viewId, Sprite* sprite, const mtx3x3_t& mat,
                           ProgramHandle progOverride = ProgramHandle(), SetSpriteStateCallback stateCallback = nullptr,
                           void* stateUserData = nullptr)
    {
        assert(sprite);
        drawSprites(viewId, &sprite, 1, &mat, progOverride, stateCallback, stateUserData);
    }

    // SpriteCache contains static sprite rendering data
    // When filled 'fillSpriteCache', the rendering data is saved in GPU buffer for consistant use
    // Call 'updateSpriteCache' if current sprites inside the cache needs to be updated (like animation frames)
    TERMITE_API SpriteCache* createSpriteCache(uint16_t maxSprites);
    TERMITE_API void destroySpriteCache(SpriteCache* scache);
    TERMITE_API void fillSpriteCache(SpriteCache* scache, Sprite** sprites, uint16_t numSprites,
                                     const mtx3x3_t* mats,
                                     ProgramHandle progOverride = ProgramHandle(), SetSpriteStateCallback stateCallback = nullptr);
    TERMITE_API void updateSpriteCache(SpriteCache* scache);

    // Registers "spritesheet" resource type and Loads SpriteSheet object
    struct LoadSpriteSheetParams
    {
        TextureFlag::Bits flags;
        TextureFormat::Enum fmt;
        uint8_t skipMips;
        bool generateMips;
        uint8_t padding[2];

        LoadSpriteSheetParams()
        {
            flags = TextureFlag::U_Clamp | TextureFlag::V_Clamp;        // Spritesheets should be CLAMP as default
            fmt = TextureFormat::RGBA8;
            skipMips = 0;
            generateMips = false;
            padding[0] = padding[1] = 0;
        }
    };

    void registerSpriteSheetToResourceLib();
    TERMITE_API rect_t getSpriteSheetTextureFrame(ResourceHandle spritesheet, const char* name);
    TERMITE_API ResourceHandle getSpriteSheetTexture(ResourceHandle spritesheet);
} // namespace termite
