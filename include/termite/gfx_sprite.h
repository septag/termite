#pragma once

//
// Issues: 
//   1 - Rotatable sprite sheets are buggy, For now they can be used with no-animation sprites

#include "bx/bx.h"

#include "assetlib.h"
#include "math.h"

#include <functional>

namespace tee
{
    struct Sprite;
    struct SpriteCache;
    struct GfxDriver;

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

    struct SpriteFlag
    {
        enum Enum
        {
            None = 0,
            DestroyResource = 0x1
        };

        typedef uint8_t Bits;
    };

    struct SpriteFlip
    {
        enum Enum
        {
            FlipX = 0x4,
            FlipY = 0x8,
        };

        typedef uint8_t Bits;
    };

    struct SpriteRenderMode
    {
        enum Enum
        {
            Normal = 0,
            Wireframe
        };
    };

    // SpriteSheet resource API
    namespace gfx {
        TEE_API rect_t getSpriteSheetTextureFrame(AssetHandle spritesheet, int index);
        TEE_API rect_t getSpriteSheetTextureFrame(AssetHandle spritesheet, const char* name);
        TEE_API AssetHandle getSpriteSheetTexture(AssetHandle spritesheet);
        TEE_API vec2_t getSpriteSheetFrameSize(AssetHandle spritesheet, const char* name);
        TEE_API vec2_t getSpriteSheetFrameSize(AssetHandle spritesheet, int index);
    }

    // Sprite API
    namespace sprite {
        // Callback for setting custom states when drawing sprites
        typedef void(*StateCallback)(GfxDriver* driver, void* userData);
        // Callback for animation frames
        typedef void(*FrameCallback)(Sprite* sprite, int frameIdx, void* userData);

        TEE_API void setRenderMode(SpriteRenderMode::Enum mode);
        TEE_API SpriteRenderMode::Enum getRenderMode();

        TEE_API Sprite* create(bx::AllocatorI* alloc, const vec2_t halfSize);
        TEE_API void destroy(Sprite* sprite);

        // Pivot: Relative to sprite's center. MaxTopLeft(-0.5, 0.5), MaxBottomRight(0.5, -0.5)
        TEE_API void addFrame(Sprite* sprite,
                              AssetHandle texHandle,
                              SpriteFlag::Bits flags = 0,
                              const vec2_t pivot = vec2(0, 0),
                              const vec2_t topLeftCoords = vec2(0, 0),
                              const vec2_t bottomRightCoords = vec2(1.0f, 1.0f),
                              const char* frameTag = nullptr);
        TEE_API void addFrame(Sprite* sprite,
                               AssetHandle ssHandle,
                               const char* name,
                               SpriteFlag::Bits flags = 0,
                               const char* frameTag = nullptr);
        TEE_API void addAllFrames(Sprite* sprite, AssetHandle ssHandle, SpriteFlag::Bits flags);

        inline Sprite* createFromTexture(bx::AllocatorI* alloc,
                                               const vec2_t& halfSize,
                                               AssetHandle texHandle,
                                               SpriteFlag::Bits flags = 0,
                                               const vec2_t& pivot = vec2(0, 0),
                                               const vec2_t& topLeftCoords = vec2(0, 0),
                                               const vec2_t& bottomRightCoords = vec2(1.0f, 1.0f))
        {
            Sprite* s = create(alloc, halfSize);
            if (s)
                addFrame(s, texHandle, flags, pivot, topLeftCoords, bottomRightCoords);
            return s;
        }

        inline Sprite* createFromSpritesheet(bx::AllocatorI* alloc,
                                                   const vec2_t& halfSize,
                                                   AssetHandle ssHandle,
                                                   const char* name,
                                                   SpriteFlag::Bits flags = 0)
        {
            Sprite* s = create(alloc, halfSize);
            if (s)
                addFrame(s, ssHandle, name, flags);
            return s;
        }

        // Animate sprites in loop
        TEE_API void animate(Sprite** sprites, uint16_t numSprites, float dt);

        TEE_API void invertAnim(Sprite* sprite);
        TEE_API void setAnimSpeed(Sprite* sprite, float speed);
        TEE_API float getAnimSpeed(Sprite* sprite);
        TEE_API void pauseAnim(Sprite* sprite);
        TEE_API void resumeAnim(Sprite* sprite);
        TEE_API void stopAnim(Sprite* sprite);
        TEE_API void replayAnim(Sprite* sprite);

        // Set frame callbacks: Frame callbacks are called when sprite animation reaches them
        TEE_API void setFrameEvent(Sprite* sprite, const char* name, FrameCallback callback, void* userData);
        TEE_API void setFrameEvent(Sprite* sprite, int frameIdx, FrameCallback callback, void* userData);
        TEE_API void setEndEvent(Sprite* sprite, FrameCallback callback, void* userData);

        // Set/Get Sprite props
        TEE_API void setHalfSize(Sprite* sprite, const vec2_t& halfSize);
        TEE_API vec2_t getHalfSize(Sprite* sprite);
        TEE_API void setScale(Sprite* sprite, const vec2_t& scale);
        TEE_API vec2_t getScale(Sprite* sprite);
        TEE_API void goFrame(Sprite* sprite, int frameIdx);
        TEE_API void goFrame(Sprite* sprite, const char* name);
        TEE_API void goTag(Sprite* sprite, const char* frameTag);
        TEE_API int getFrame(Sprite* sprite);
        TEE_API int getFrameCount(Sprite* sprite);
        TEE_API void setFlip(Sprite* sprite, SpriteFlip::Bits flip);
        TEE_API SpriteFlip::Bits getFlip(Sprite* sprite);
        TEE_API void setPosOffset(Sprite* sprite, const vec2_t posOffset);
        TEE_API vec2_t getPosOffset(Sprite* sprite);
        TEE_API void setOrder(Sprite* sprite, uint8_t order); // higher orders gets to draw on top
        TEE_API int getOrder(Sprite* sprite);
        TEE_API void setPivot(Sprite* sprite, const vec2_t pivot);
        TEE_API void setTint(Sprite* sprite, ucolor_t color);
        TEE_API void setGlow(Sprite* sprite, float glow);
        TEE_API ucolor_t getTint(Sprite* sprite);
        TEE_API rect_t getDrawRect(Sprite* sprite);
        TEE_API void getRealRect(Sprite* sprite, vec2_t* pHalfSize, vec2_t* pCenter);
        TEE_API vec2_t getImageSize(Sprite* sprite);
        TEE_API rect_t getTexelCoords(Sprite* sprite);
        TEE_API ProgramHandle getAddProgram();
        TEE_API void setUserData(Sprite* sprite, void* userData);
        TEE_API void* getUserData(Sprite* sprite);

        // For manual rendering of sprite frames
        TEE_API void getDrawData(Sprite* sprite, int frameIdx, rect_t* drawRect, rect_t* textureRect,
                                 AssetHandle* textureHandle);

        TEE_API void convertCoordsPixelToLogical(vec2_t* ptsOut, const vec2_t* ptsIn, int numPts, Sprite* sprite);

        TEE_API void draw(uint8_t viewId, Sprite** sprites, uint16_t numSprites, const mat3_t* mats,
                          ProgramHandle progOverride = ProgramHandle(), sprite::StateCallback stateCallback = nullptr,
                          void* stateUserData = nullptr, const ucolor_t* colors = nullptr);

        // SpriteCache contains static sprite rendering data
        // Send the same drawing data same as drawSprites
        TEE_API SpriteCache* createCache(bx::AllocatorI* alloc, Sprite** sprites, uint16_t numSprites,
                                         const mat3_t* mats, const ucolor_t* colors = nullptr);
        TEE_API void drawCache(uint8_t viewId, SpriteCache* spriteCache,
                               ProgramHandle progOverride = ProgramHandle(),
                               sprite::StateCallback stateCallback = nullptr,
                               void* stateUserData = nullptr);
        TEE_API void destroyCache(SpriteCache* spriteCache);
        TEE_API rect_t getCacheBounds(SpriteCache* spriteCache);

        inline void animate(Sprite* sprite, float dt)
        {
            animate(&sprite, 1, dt);
        }

        inline void draw(uint8_t viewId, Sprite* sprite, const mat3_t& mat,
                         ProgramHandle progOverride = ProgramHandle(), sprite::StateCallback stateCallback = nullptr,
                         void* stateUserData = nullptr)
        {
            assert(sprite);
            draw(viewId, &sprite, 1, &mat, progOverride, stateCallback, stateUserData);
        }
    }
} // namespace tee
