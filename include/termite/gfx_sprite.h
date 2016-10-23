#pragma once

#include "bx/bx.h"

#include "gfx_driver.h"
#include "resource_lib.h"
#include "vec_math.h"

#define T_SPRITE_MEMORY_TAG 0x24c7a709645feb5b 

namespace termite
{
    struct Camera2D;
    struct SpriteT;
    struct SpriteAnimT;
    typedef PhantomType<uint16_t, SpriteT, UINT16_MAX> SpriteHandle;
    typedef PhantomType<uint16_t, SpriteAnimT, UINT16_MAX> SpriteAnimHandle;

    struct SpriteAnimClip
    {
        char name[32];
        size_t nameHash;
        int startFrame;
        int endFrame;
        float fps;
        float totalTime;
    };

    struct SpriteSheet
    {
        ResourceHandle textureHandle;
        int numFrames;
        int numAnimClips;
        vec4_t* frames; // count = numFrames
        size_t* tags; // count = numFrames (hashed value of tag name)
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
                                                     bool destroyTexture = false, 
                                                     const vec2_t localAnchor = vec2f(0, 0),
                                                     const vec2_t topLeftCoords = vec2f(0, 0),
                                                     const vec2_t bottomRightCoords = vec2f(1.0f, 1.0f));
    TERMITE_API SpriteHandle createSpriteFromSpriteSheet(ResourceHandle spritesheetHandle, const vec2_t halfSize,
                                                         bool destroySpriteSheet = false,
                                                         const vec2_t localAnchor = vec2f(0, 0), int index = -1);
    TERMITE_API void destroySprite(SpriteHandle handle);


    TERMITE_API SpriteCache* createSpriteCache(int maxSprites);
    TERMITE_API void destroySpriteCache(SpriteCache* spriteCache);
    TERMITE_API void updateSpriteCache(const SpriteHandle* sprites, int numSprites, const vec2_t* posBuffer,
                                       const float* rotBuffer, const color_t* tintColors);

    TERMITE_API void drawSprites(uint8_t viewId, const SpriteHandle* sprites, int numSprites,
                                 const vec2_t* posBuffer, const float* rotBuffer = nullptr, const float* sizeBuffer = nullptr,
                                 const color_t* tintColors = nullptr,
                                 ProgramHandle prog = ProgramHandle(), SetSpriteStateCallback stateFunc = nullptr);
    TERMITE_API void drawSprites(uint8_t viewId, SpriteCache* spriteCache);

    // Sprite Animation API
    typedef void(*SpriteAnimCallback)(SpriteAnimHandle handle, void* userData);

    TERMITE_API SpriteAnimHandle createSpriteAnim(SpriteHandle sprite);
    TERMITE_API void destroySpriteAnim(SpriteAnimHandle handle);
    TERMITE_API void stepSpriteAnim(SpriteAnimHandle handle, float dt);
    TERMITE_API void queueSpriteAnim(SpriteAnimHandle handle, const char* clipName, bool looped, 
                                     bool reverse = false, SpriteAnimCallback finishedCallback = nullptr, 
                                     void* userData = nullptr);
    TERMITE_API void addSpriteAnimEvent(SpriteAnimHandle handle, const char* tagName, SpriteAnimCallback tagInCallback,
                                        void* userData = nullptr, SpriteAnimCallback tagOutCallback = nullptr);
    TERMITE_API void resetSpriteAnim(SpriteAnimHandle handle);
    TERMITE_API void stopSpriteAnim(SpriteAnimHandle handle);
    TERMITE_API void setSpriteAnimPlaybackSpeed(SpriteAnimHandle handle, float speed);
    TERMITE_API float getSpriteAnimPlaybackSpeed(SpriteAnimHandle handle);
    TERMITE_API void playSpriteAnim(SpriteAnimHandle handle);

    TERMITE_API void setSpriteFrame(SpriteHandle handle, int frameIdx);
    TERMITE_API int getSpriteFrame(SpriteHandle handle);
    TERMITE_API void setSpriteAnchor(SpriteHandle handle, const vec2_t localAnchor);
    TERMITE_API vec2_t getSpriteAnchor(SpriteHandle handle);
    TERMITE_API void setSpriteSize(SpriteHandle handle, const vec2_t halfSize);
    TERMITE_API vec2_t getSpriteSize(SpriteHandle handle);

    // Registers "spritesheet" resource type and Loads SpriteSheet object
    void registerSpriteSheetToResourceLib();

    struct LoadSpriteSheetParams
    {
        bool generateMips;
        uint8_t skipMips;
        TextureFlag::Bits flags;

        LoadSpriteSheetParams()
        {
            generateMips = false;
            skipMips = 0;
            flags = TextureFlag::U_Clamp | TextureFlag::V_Clamp |
                    TextureFlag::MinPoint | TextureFlag::MagPoint;
        }
    };

    // Helper Wrappers
    class Sprite
    {
    public:
        SpriteHandle handle;

    public:
        inline Sprite()
        {
        }

        explicit inline Sprite(SpriteHandle _handle) : 
            handle(_handle)
        {
        }

        inline ~Sprite()
        {
            assert(!handle.isValid());
        }

        inline void setFrame(int index)
        {
            setSpriteFrame(this->handle, index);
        }

        inline int getFrame() const 
        {
            return getSpriteFrame(this->handle);
        }

        inline void setSize(const vec2_t& halfSize)
        {
            setSpriteSize(this->handle, halfSize);
        }

        inline vec2_t getSize() const
        {
            return getSpriteSize(this->handle);
        }

        inline void setLocalAnchor(const vec2_t& anchor)
        {
            setSpriteAnchor(this->handle, anchor);
        }

        inline vec2_t getLocalAnchor() const
        {
            return getSpriteAnchor(this->handle);
        }

        inline bool isValid() const
        {
            return this->handle.isValid();
        }

        inline bool createFromTexture(ResourceHandle textureHandle, 
                           const vec2_t halfSize,
                           bool destroyTexture,
                           const vec2_t localAnchor = vec2f(0, 0),
                           const vec2_t topLeftCoords = vec2f(0, 0),
                           const vec2_t bottomRightCoords = vec2f(1.0f, 1.0f))
        {
            this->handle =  createSpriteFromTexture(textureHandle, halfSize, destroyTexture, localAnchor, 
                                                    topLeftCoords, bottomRightCoords);
            return this->handle.isValid();
        }

        inline bool createFromSpriteSheet(ResourceHandle spritesheetHandle, 
                           const vec2_t halfSize,
                           bool destroySpriteSheet,
                           const vec2_t localAnchor = vec2f(0, 0), 
                           int frameIndex = -1)
        {
            this->handle = createSpriteFromSpriteSheet(spritesheetHandle, halfSize, destroySpriteSheet, 
                                                       localAnchor, frameIndex);
            return this->handle.isValid();
        }

        inline void destroy()
        {
            if (this->handle.isValid()) {
                destroySprite(this->handle);
                this->handle.reset();
            }
        }

        inline void draw(uint8_t viewId, const vec2_t& pos, float rot = 0, float size = 1.0f, color_t tintColor = 0xffffffff)
        {
            assert(this->handle.isValid());
            drawSprites(viewId, &this->handle, 1, &pos, &rot, &size, &tintColor);
        }

        inline SpriteHandle getHandle() const
        {
            return this->handle;
        }
    };

    class SpriteAnim
    {
    public:
        SpriteAnimHandle handle;

    public:
        inline SpriteAnim()
        {
        }

        explicit inline SpriteAnim(SpriteAnimHandle _handle) :
            handle(_handle)
        {
        }

        inline ~SpriteAnim()
        {
            assert(!handle.isValid());
        }

        inline bool create(const Sprite& sprite)
        {
            this->handle = createSpriteAnim(sprite.handle);
            return this->handle.isValid();
        }

        inline void destroy()
        {
            destroySpriteAnim(this->handle);
            this->handle.reset();
        }

        inline void step(float dt)
        {
            stepSpriteAnim(this->handle, dt);
        }

        inline SpriteAnim& stop()
        {
            stopSpriteAnim(this->handle);
            return *this;
        }

        inline SpriteAnim& queue(const char* clipName, bool looped, bool reverse = false,
                                 SpriteAnimCallback finishCallback = nullptr, void* userData = nullptr)
        {
            queueSpriteAnim(this->handle, clipName, looped, reverse, finishCallback, userData);
            return *this;
        }

        inline SpriteAnim& play()
        {
            playSpriteAnim(this->handle);
            return *this;
        }

        inline SpriteAnim& reset()
        {
            resetSpriteAnim(this->handle);
            return *this;
        }

        inline SpriteAnim& addEvent(const char* tagName, SpriteAnimCallback tagInCallback,
                                    void* userData = nullptr, SpriteAnimCallback tagOutCallback = nullptr)
        {
            addSpriteAnimEvent(this->handle, tagName, tagInCallback, userData, tagOutCallback);
            return *this;
        }

        inline void setPlaySpeed(float speedMult)
        {
            setSpriteAnimPlaybackSpeed(this->handle, speedMult);
        }

        inline float getPlaySpeed() const
        {
            return getSpriteAnimPlaybackSpeed(this->handle);
        }

    };

} // namespace termite
