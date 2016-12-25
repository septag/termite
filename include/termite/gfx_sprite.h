#pragma once

//
// Issues: 
//   1 - Rotatable sprite sheets are buggy, For now they can be used with no-animation sprites

#include "bx/bx.h"

#include "gfx_driver.h"
#include "resource_lib.h"
#include "vec_math.h"

namespace termite
{
    struct Sprite;
    struct SpriteCache;

    struct SpriteFlag
    {
        enum Enum
        {
            DestroyResource = 0x1,
            FlipX = 0x2,
            FlipY = 0x3
        };

        typedef uint8_t Bits;
    };

    // Callback for setting custom states when drawing sprites
    typedef void(*SetSpriteStateCallback)(GfxDriverApi* driver);
    // Callback for animation frames
    typedef void(*SpriteFrameCallback)(Sprite* sprite, void* userData);

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

    // Set frame callbacks: Frame callbacks are called when sprite animation reaches them
    TERMITE_API void setSpriteFrameCallbackByTag(Sprite* sprite, const char* frameTag, SpriteFrameCallback callback,
                                                 void* userData);
    TERMITE_API void setSpriteFrameCallbackByName(Sprite* sprite, const char* name, SpriteFrameCallback callback,
                                                  void* userData);
    TERMITE_API void setSpriteFrameCallbackByIndex(Sprite* sprite, int frameIdx, SpriteFrameCallback callback,
                                                   void* userData);

    // Goes from current sprite frame to the next specified tag/name or frameIdx
    TERMITE_API void gotoSpriteFrameIndex(Sprite* sprite, int frameIdx);
    TERMITE_API void gotoSpriteFrameName(Sprite* sprite, const char* name);
    TERMITE_API void gotoSpriteFrameTag(Sprite* sprite, const char* frameTag);
    TERMITE_API int getSpriteFrameIndex(Sprite* sprite);

    // Set/Get tint color for the sprite
    TERMITE_API void setSpriteTintColor(Sprite* sprite, color_t color);
    TERMITE_API color_t getSpriteTintColor(Sprite* sprite);

    // Dynamically draw sprites
    TERMITE_API void drawSprites(uint8_t viewId, Sprite** sprites, uint16_t numSprites, const mtx3x3_t* mats,
                                 ProgramHandle progOverride = ProgramHandle(), SetSpriteStateCallback stateCallback = nullptr);
    inline void drawSprite(uint8_t viewId, Sprite* sprite, const mtx3x3_t& mat,
                           ProgramHandle progOverride = ProgramHandle(), SetSpriteStateCallback stateCallback = nullptr)
    {
        assert(sprite);
        drawSprites(viewId, &sprite, 1, &mat, progOverride, stateCallback);
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
        bool generateMips;
        uint8_t skipMips;
        TextureFlag::Bits flags;

        LoadSpriteSheetParams()
        {
            generateMips = false;
            skipMips = 0;
            flags = TextureFlag::U_Clamp | TextureFlag::V_Clamp;
        }
    };

    void registerSpriteSheetToResourceLib();

    // Thin wrapper around SpriteHandle
    // Mainly used to get cleaner API for single sprites
    class CSprite
    {
    private:
        Sprite* m_sprite;

    public:
        inline CSprite() :
            m_sprite(nullptr)
        {
        }

        explicit inline CSprite(Sprite* _sprite) :
            m_sprite(_sprite)
        {
        }

        inline bool create(bx::AllocatorI* alloc, const vec2_t& halfSize)
        {
            m_sprite = createSprite(alloc, halfSize);
            return m_sprite != nullptr;
        }

        inline bool create(bx::AllocatorI* alloc,
                           const vec2_t& halfSize,
                           ResourceHandle texHandle,
                           SpriteFlag::Bits flags = 0,
                           const vec2_t pivot = vec2f(0, 0),
                           const vec2_t topLeftCoords = vec2f(0, 0),
                           const vec2_t bottomRightCoords = vec2f(1.0f, 1.0f))
        {
            m_sprite = createSpriteFromTexture(alloc, halfSize, texHandle, flags, pivot, topLeftCoords, bottomRightCoords);
            return m_sprite != nullptr;
        }

        inline bool create(bx::AllocatorI* alloc,
                           const vec2_t& halfSize,
                           ResourceHandle ssHandle,
                           const char* name,
                           SpriteFlag::Bits flags = 0)
        {
            m_sprite = createSpriteFromSpritesheet(alloc, halfSize, ssHandle, name, flags);
            return m_sprite != nullptr;
        }

        inline CSprite& addFrame(ResourceHandle texHandle, 
                                 SpriteFlag::Bits flags = 0,
                                 const vec2_t& pivot = vec2f(0, 0),
                                 const vec2_t& topLeftCoords = vec2f(0, 0),
                                 const vec2_t& bottomRightCoords = vec2f(1.0f, 1.0f),
                                 const char* frameTag = nullptr)
        {
            addSpriteFrameTexture(m_sprite, texHandle, flags, pivot, topLeftCoords, bottomRightCoords, frameTag);
            return *this;
        }

        inline CSprite& addFrame(ResourceHandle ssHandle, const char* name, SpriteFlag::Bits flags = 0, const char* frameTag = nullptr)
        {
            addSpriteFrameSpritesheet(m_sprite, ssHandle, name, flags, frameTag);
            return *this;
        }

        inline CSprite& addAllFrames(ResourceHandle ssHandle, SpriteFlag::Bits flags = 0)
        {
            addSpriteFrameAll(m_sprite, ssHandle, flags);
            return *this;
        }

        inline void animate(float dt)
        {
            animateSprite(m_sprite, dt);
        }

        inline void pause()
        {
            pauseSpriteAnim(m_sprite);
        }

        inline void resume()
        {
            resumeSpriteAnim(m_sprite);
        }

        inline void stop()
        {
            stopSpriteAnim(m_sprite);
        }

        inline void invertAnimation()
        {
            invertSpriteAnim(m_sprite);
        }

        inline void setSpeed(float speed)
        {
            setSpriteAnimSpeed(m_sprite, speed);
        }

        inline float getSpeed() const
        {
            return getSpriteAnimSpeed(m_sprite);
        }

        inline void destroy()
        {
            if (m_sprite != nullptr) {
                destroySprite(m_sprite);
                m_sprite = nullptr;
            }
        }

        inline void setFrameCallbackByTag(const char* frameTag, SpriteFrameCallback callback, void* userData)
        {
            setSpriteFrameCallbackByTag(m_sprite, frameTag, callback, userData);
        }

        inline void setFrameCallbackByName(const char* name, SpriteFrameCallback callback, void* userData)
        {
            setSpriteFrameCallbackByName(m_sprite, name, callback, userData);
        }

        inline void setFrameCallbackByIndex(int index, SpriteFrameCallback callback, void* userData)
        {
            setSpriteFrameCallbackByIndex(m_sprite, index, callback, userData);
        }

        inline void gotoFrameIndex(int frameIdx)
        {
            gotoSpriteFrameIndex(m_sprite, frameIdx);
        }

        inline void gotoFrameName(const char* name)
        {
            gotoSpriteFrameName(m_sprite, name);
        }

        inline void gotoFrameTag(const char* frameTag)
        {
            gotoSpriteFrameTag(m_sprite, frameTag);
        }

        inline int getFrameIndex() const
        {
            return getSpriteFrameIndex(m_sprite);
        }

        inline void setTintColor(color_t color)
        {
            setSpriteTintColor(m_sprite, color);
        }

        inline color_t getTintColor() const
        {
            return getSpriteTintColor(m_sprite);
        }

        inline void draw(uint8_t viewId, const mtx3x3_t& mat,
                         ProgramHandle progOverride = ProgramHandle(), SetSpriteStateCallback stateCallback = nullptr)
        {
            drawSprite(viewId, m_sprite, mat, progOverride, stateCallback);
        }

        inline bool isValid() const
        {
            return m_sprite != nullptr;
        }

        inline operator Sprite*()
        {
            return m_sprite;
        }

        inline operator const Sprite*() const
        {
            return m_sprite;
        }
    };

} // namespace termite
