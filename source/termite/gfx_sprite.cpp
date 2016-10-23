#include "pch.h"
#include "gfx_sprite.h"
#include "gfx_texture.h"
#include "memory_pool.h"
#include "camera.h"
#include "math_util.h"

#include "bx/readerwriter.h"
#include "bxx/path.h"
#include "bxx/indexed_pool.h"
#include "bxx/array.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/logger.h"

#include "../include_common/sprite_format.h"
#include "shaders_h/sprite.vso"
#include "shaders_h/sprite.fso"

#include <algorithm>

using namespace termite;

namespace termite
{
    struct SpriteCache
    {
        DynamicVertexBufferHandle vb;
        IndexBufferHandle ib;
        int numSprites;
    };

    struct SpriteBase
    {
        enum Enum
        {
            FromTexture,
            FromSpriteSheet
        };
    };

    struct SpriteImpl
    {
        ResourceHandle resHandle;   // depends on base type
        SpriteBase::Enum base;
        vec2_t halfSize;
        vec2_t localAnchor;
        vec2_t topLeft;
        vec2_t bottomRight;
        int frameIdx;               // initial Frame Index 
        bool destroyResource;
    };
}

struct SpriteVertex
{
    float x;
    float y;
    float r;
    float s;
    float tx;
    float ty;
    float offsetx;
    float offsety;
    float anchorx;
    float anchory;
    uint32_t color;

    static void init()
    {
        vdeclBegin(&Decl);
        vdeclAdd(&Decl, VertexAttrib::Position, 4, VertexAttribType::Float);
        vdeclAdd(&Decl, VertexAttrib::TexCoord0, 2, VertexAttribType::Float);
        vdeclAdd(&Decl, VertexAttrib::TexCoord1, 4, VertexAttribType::Float);
        vdeclAdd(&Decl, VertexAttrib::Color0, 4, VertexAttribType::Uint8, true);
        vdeclEnd(&Decl);
    }

    static VertexDecl Decl;
};
VertexDecl SpriteVertex::Decl;

class SpriteSheetLoader : public ResourceCallbacksI
{
    bool loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj) override;
    void unloadObj(uintptr_t obj) override;
    void onReload(ResourceHandle handle) override;
    uintptr_t getDefaultAsyncObj() override;
};

struct SpriteAnimEvent
{
    size_t tagNameHash;
    SpriteAnimCallback inCallback;
    SpriteAnimCallback outCallback;
    void* userData;
    SpriteAnimEvent* next;

    SpriteAnimEvent()
    {
        tagNameHash = 0;
        inCallback = nullptr;
        userData = nullptr;
        next = nullptr;
        inCallback = nullptr;
        outCallback = nullptr;
    }
};

struct SpriteAnimAction
{
    size_t clipNameHash;
    bool reverse;
    bool looped;
    SpriteAnimCallback finishCallback;
    void* userData;
    SpriteAnimAction* next;

    SpriteAnimAction()
    {
        clipNameHash = 0;
        reverse = false;
        looped = false;
        finishCallback = nullptr;
        userData = nullptr;
        next = nullptr;
    }
};

struct SpriteAnimImpl
{
    SpriteHandle sprite;
    float t;
    float speed;
    float resumeSpeed;
    size_t clipNameHash;
    SpriteAnimAction* action;
    SpriteAnimEvent* events;
    SpriteAnimEvent* lastCalledEvent;
};

struct SpriteSystem
{
    GfxDriverApi* driver;
    bx::AllocatorI* alloc;
    PageAllocator allocStub;
    bx::IndexedPool spritePool;
    bx::IndexedPool animPool;
    ProgramHandle spriteProg;
    UniformHandle u_texture;
    SpriteSheetLoader loader;
    bx::Pool<SpriteAnimAction> actionPool;
    bx::Pool<SpriteAnimEvent> eventPool;

    SpriteSystem(bx::AllocatorI* _alloc) : 
        allocStub(T_SPRITE_MEMORY_TAG)
    {
        alloc = _alloc;
        driver = nullptr;
    }
};

static SpriteSystem* g_sprite = nullptr;

result_t termite::initSpriteSystem(GfxDriverApi* driver, bx::AllocatorI* alloc, uint16_t poolSize)
{
    if (g_sprite) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_sprite = BX_NEW(alloc, SpriteSystem)(alloc);
    if (!g_sprite) {
        return T_ERR_OUTOFMEM;
    }

    g_sprite->driver = driver;

    const uint32_t spriteSizes[] = {
        sizeof(SpriteImpl)
    };
    if (!g_sprite->spritePool.create(spriteSizes, 1, poolSize, poolSize, &g_sprite->allocStub)) {
        return T_ERR_OUTOFMEM;
    }

    const uint32_t animSizes[] = {
        sizeof(SpriteAnimImpl)
    };
    if (!g_sprite->animPool.create(animSizes, 1, poolSize, poolSize, &g_sprite->allocStub)) {
        return T_ERR_OUTOFMEM;
    }

    if (!g_sprite->actionPool.create(128, &g_sprite->allocStub) ||
        !g_sprite->eventPool.create(64, &g_sprite->allocStub))
    {
        return T_ERR_OUTOFMEM;
    }

    SpriteVertex::init();

    g_sprite->spriteProg = driver->createProgram(driver->createShader(driver->makeRef(sprite_vso, sizeof(sprite_vso), nullptr, nullptr)),
                                                 driver->createShader(driver->makeRef(sprite_fso, sizeof(sprite_fso), nullptr, nullptr)),
                                                 true);
    if (!g_sprite->spriteProg.isValid())
        return T_ERR_FAILED;
    g_sprite->u_texture = driver->createUniform("u_texture", UniformType::Int1, 1);
    
    return 0;
}

void termite::shutdownSpriteSystem()
{
    if (!g_sprite)
        return;

    bx::AllocatorI* alloc = g_sprite->alloc;
    GfxDriverApi* driver = g_sprite->driver;

    if (g_sprite->spriteProg.isValid())
        driver->destroyProgram(g_sprite->spriteProg);
    if (g_sprite->u_texture.isValid())
        driver->destroyUniform(g_sprite->u_texture);

    g_sprite->eventPool.destroy();
    g_sprite->actionPool.destroy();
    g_sprite->animPool.destroy();
    g_sprite->spritePool.destroy();
    BX_DELETE(alloc, g_sprite);
    g_sprite = nullptr;
}

SpriteHandle termite::createSpriteFromTexture(ResourceHandle textureHandle, const vec2_t halfSize, 
                                              bool destroyTexture,
                                              const vec2_t localAnchor /*= vec2f(0, 0)*/, 
                                              const vec2_t topLeftCoords /*= vec2f(0, 0)*/, 
                                              const vec2_t bottomRightCoords /*= vec2f(1.0f, 1.0f)*/)
{
    if (!textureHandle.isValid())
        return SpriteHandle();

    SpriteHandle handle(g_sprite->spritePool.newHandle());
    
    SpriteImpl* sprite = g_sprite->spritePool.getHandleData<SpriteImpl>(0, handle.value);
    sprite->base = SpriteBase::FromTexture;
    sprite->resHandle = textureHandle;
    sprite->halfSize = halfSize;
    sprite->localAnchor = localAnchor;
    sprite->topLeft = topLeftCoords;
    sprite->bottomRight = bottomRightCoords;
    sprite->frameIdx = 0;
    sprite->destroyResource = destroyTexture;
    
    return handle;
}

SpriteHandle termite::createSpriteFromSpriteSheet(ResourceHandle spritesheetHandle, const vec2_t halfSize, 
                                                  bool destroySpriteSheet,
                                                  const vec2_t localAnchor /*= vec2f(0, 0)*/, int index /*= -1*/)
{
    if (!spritesheetHandle.isValid())
        return SpriteHandle();
    SpriteHandle handle(g_sprite->spritePool.newHandle());
    SpriteImpl* sprite = g_sprite->spritePool.getHandleData<SpriteImpl>(0, handle.value);
    sprite->base = SpriteBase::FromSpriteSheet;
    sprite->resHandle = spritesheetHandle;
    sprite->halfSize = halfSize;
    sprite->localAnchor = localAnchor;
    sprite->destroyResource = destroySpriteSheet;

    if (index == -1)
        index = 0;
    sprite->frameIdx = index;

    return handle;
}

void termite::destroySprite(SpriteHandle handle)
{
    assert(handle.isValid());

    SpriteImpl* sprite = g_sprite->spritePool.getHandleData<SpriteImpl>(0, handle.value);
    if (sprite->resHandle.isValid())
        unloadResource(sprite->resHandle);

    g_sprite->spritePool.freeHandle(handle.value);
}

SpriteCache* termite::createSpriteCache(int maxSprites)
{
    return nullptr;
}

void termite::destroySpriteCache(SpriteCache* spriteCache)
{

}

void termite::updateSpriteCache(const SpriteHandle* sprites, int numSprites, const vec2_t* posBuffer, 
                                const float* rotBuffer, const color_t* tintColors)
{

}

void termite::drawSprites(uint8_t viewId, const SpriteHandle* sprites, int numSprites, 
                          const vec2_t* posBuffer, const float* rotBuffer, const float* sizeBuffer,
                          const color_t* tintColors,
                          ProgramHandle prog, SetSpriteStateCallback stateFunc)
{
    assert(sprites);
    assert(posBuffer);
    
    if (numSprites <= 0)
        return;

    GfxDriverApi* driver = g_sprite->driver;

    TransientVertexBuffer tvb;
    TransientIndexBuffer tib;
    
    const int numVerts = numSprites * 4;
    const int numIndices = numSprites * 6;
    GfxState::Bits baseState = gfxStateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite | GfxState::CullCCW;

    if (!driver->checkAvailTransientVertexBuffer(numVerts, SpriteVertex::Decl))
        return;
    driver->allocTransientVertexBuffer(&tvb, numVerts, SpriteVertex::Decl);

    if (!driver->checkAvailTransientIndexBuffer(numIndices))
        return;
    driver->allocTransientIndexBuffer(&tib, numIndices);

    // Sort sprites by texture and batch them
    struct SortedSprite
    {
        int index;
        SpriteHandle handle;
    };

    bx::AllocatorI* tmpAlloc = getTempAlloc();
    SortedSprite* sortedSprites = (SortedSprite*)BX_ALLOC(tmpAlloc, sizeof(SortedSprite)*numSprites);
    for (int i = 0; i < numSprites; i++) {
        sortedSprites[i].index = i;
        sortedSprites[i].handle = sprites[i];
    }
    std::sort(sortedSprites, sortedSprites + numSprites, [](const SortedSprite& a, const SortedSprite&b)->bool {
        SpriteImpl* sa = g_sprite->spritePool.getHandleData<SpriteImpl>(0, a.handle.value);
        SpriteImpl* sb = g_sprite->spritePool.getHandleData<SpriteImpl>(0, b.handle.value);
        return sa->resHandle.value < sb->resHandle.value;
    });

    // Fill Default buffers
    if (rotBuffer == nullptr) {
        float* rots = (float*)BX_ALLOC(tmpAlloc, sizeof(float)*numSprites);
        memset(rots, 0x00, sizeof(float)*numSprites);
        rotBuffer = rots;
    }

    if (sizeBuffer == nullptr) {
        float* sizes = (float*)BX_ALLOC(tmpAlloc, sizeof(float)*numSprites);
        for (int i = 0; i < numSprites; i++)
            sizes[i] = 1.0f;
        sizeBuffer = sizes;
    }

    if (tintColors == nullptr) {
        color_t* colors = (color_t*)BX_ALLOC(tmpAlloc, sizeof(color_t)*numSprites);
        for (int i = 0; i < numSprites; i++)
            colors[i] = 0xffffffff;
        tintColors = colors;
    }

    // Fill sprites
    SpriteVertex* verts = (SpriteVertex*)tvb.data;
    uint16_t* indices = (uint16_t*)tib.data;
    int indexIdx = 0;
    int vertexIdx = 0;
    for (int i = 0; i < numSprites; i++) {
        const SortedSprite& ss = sortedSprites[i];
        SpriteImpl* sprite = g_sprite->spritePool.getHandleData<SpriteImpl>(0, ss.handle.value);
        vec2_t halfSize = sprite->halfSize;

        // Resolve sprite bounds
        if (sprite->base == SpriteBase::FromSpriteSheet) {
            SpriteSheet* sheet = getResourcePtr<SpriteSheet>(sprite->resHandle);
            if (sheet) {
                Texture* texture = getResourcePtr<Texture>(sheet->textureHandle);
                vec4_t frame = sheet->frames[sprite->frameIdx];
                sprite->topLeft = vec2f(frame.x, frame.y);
                sprite->bottomRight = vec2f(frame.z, frame.w);
                float w = (frame.z - frame.x)*float(texture->info.width);
                float h = (frame.w - frame.y)*float(texture->info.height);
                float ratio = w / h;
                if (halfSize.x == 0)
                    halfSize.x = ratio*halfSize.y;
                if (halfSize.y == 0)
                    halfSize.y = halfSize.x / ratio;
            }
        } else if (sprite->base == SpriteBase::FromTexture) {
            Texture* texture = getResourcePtr<Texture>(sprite->resHandle);
            if (halfSize.x == 0)
                halfSize.x = texture->ratio*halfSize.y;
            if (halfSize.y == 0)
                halfSize.y = halfSize.x / texture->ratio;
        }

        SpriteVertex& v0 = verts[vertexIdx];
        SpriteVertex& v1 = verts[vertexIdx + 1];
        SpriteVertex& v2 = verts[vertexIdx + 2];
        SpriteVertex& v3 = verts[vertexIdx + 3];

        vec2_t topleft = sprite->topLeft;
        vec2_t bottomright = sprite->bottomRight;
        vec2_t localAnchor = sprite->localAnchor;
        vec2_t pos = posBuffer[ss.index];
        vec2_t anchor = vec2f(2.0f*localAnchor.x*halfSize.x, 2.0f*localAnchor.y*halfSize.y);

        // Top-Left
        v0.x = -halfSize.x;       v0.y = halfSize.y;         
        v0.tx = topleft.x;               v0.ty = topleft.y;

        // Top-Right
        v1.x = halfSize.x;      v1.y = halfSize.y;          
        v1.tx = bottomright.x;          v1.ty = topleft.y;

        // Bottom-Left
        v2.x = -halfSize.x;      v2.y = -halfSize.y;          
        v2.tx = topleft.x;              v2.ty = bottomright.y;

        // Bottom-Right
        v3.x = halfSize.x;      v3.y = -halfSize.y;          
        v3.tx = bottomright.x;          v3.ty = bottomright.y;

        v0.r = v1.r = v2.r = v3.r = rotBuffer[ss.index];
        v0.s = v1.s = v2.s = v3.s = sizeBuffer[ss.index];

        v0.offsetx = v1.offsetx = v2.offsetx = v3.offsetx = pos.x;
        v0.offsety = v1.offsety = v2.offsety = v3.offsety = pos.y;
        v0.anchorx = v1.anchorx = v2.anchorx = v3.anchorx = anchor.x;
        v0.anchory = v1.anchory = v2.anchory = v3.anchory = anchor.y;
        v0.color = v1.color = v2.color = v3.color = tintColors[ss.index];

        // Make a quad from 4 verts
        indices[indexIdx] = vertexIdx;
        indices[indexIdx + 1] = vertexIdx + 1;
        indices[indexIdx + 2] = vertexIdx + 2;
        indices[indexIdx + 3] = vertexIdx + 2 ;
        indices[indexIdx + 4] = vertexIdx + 1;
        indices[indexIdx + 5] = vertexIdx + 3;

        indexIdx += 6;
        vertexIdx += 4;
    }

    struct Batch
    {
        int index;
        int count;
        //int dummy; // TODO: WTF is this? when the size of Batch is 8 the whole system crumbles
    };
    int maxBatches = 32;
    int numBatches = 0;
    Batch* batches = (Batch*)BX_ALLOC(tmpAlloc, sizeof(Batch)*maxBatches);

    ResourceHandle prevHandle;
    Batch* curBatch = nullptr;
    for (int i = 0; i < numSprites; i++) {
        SpriteImpl* sprite = g_sprite->spritePool.getHandleData<SpriteImpl>(0, sortedSprites[i].handle.value);
        ResourceHandle curHandle = g_sprite->spritePool.getHandleData<SpriteImpl>(0, sortedSprites[i].handle.value)->resHandle;
        if (curHandle.value != prevHandle.value) {
            if (numBatches == maxBatches) {
                maxBatches += 32;
                batches = (Batch*)BX_REALLOC(tmpAlloc, batches, sizeof(Batch)*maxBatches);
            }
            curBatch = &batches[numBatches++];
            curBatch->index = i;
            curBatch->count = 0;
            prevHandle = curHandle;
        }
        curBatch->count++;
    }

    // Draw batches
    if (!prog.isValid())
        prog = g_sprite->spriteProg;

    for (int i = 0; i < numBatches; i++) {
        const Batch& batch = batches[i];
        driver->setState(baseState, 0);
        driver->setTransientVertexBufferI(&tvb, 0, batch.count*4);
        driver->setTransientIndexBufferI(&tib, batch.index*6, batch.count*6);   
        SpriteImpl* sprite = g_sprite->spritePool.getHandleData<SpriteImpl>(0, sortedSprites[batch.index].handle.value);
        if (sprite->resHandle.isValid()) {
            TextureHandle texHandle;
            if (sprite->base == SpriteBase::FromTexture) {
                texHandle = getResourcePtr<Texture>(sprite->resHandle)->handle;
            } else if (sprite->base == SpriteBase::FromSpriteSheet) {
                SpriteSheet* sheet = getResourcePtr<SpriteSheet>(sprite->resHandle);
                if (sheet)
                    texHandle = getResourcePtr<Texture>(sheet->textureHandle)->handle;
                else
                    texHandle = getWhiteTexture1x1();
            }
            driver->setTexture(0, g_sprite->u_texture, texHandle, TextureFlag::FromTexture);
        }

        if (stateFunc)
            stateFunc(driver);
        driver->submit(viewId, g_sprite->spriteProg, 0, false);
    }
}

SpriteAnimHandle termite::createSpriteAnim(SpriteHandle sprite)
{
    assert(sprite.isValid());

    uint16_t animIdx = g_sprite->animPool.newHandle();
    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, animIdx);
    memset(anim, 0x00, sizeof(SpriteAnimImpl));

    anim->sprite = sprite;
    anim->speed = anim->resumeSpeed = 1.0f;
    
    return SpriteAnimHandle(animIdx);
}

void termite::destroySpriteAnim(SpriteAnimHandle handle)
{
    assert(handle.isValid());

    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, handle.value);

    // Free all actions
    while (anim->action) {
        SpriteAnimAction* action = anim->action;
        anim->action = anim->action->next;
        g_sprite->actionPool.deleteInstance(action);
    }
    g_sprite->animPool.freeHandle(handle.value);
}

inline SpriteAnimClip* searchClips(SpriteSheet* ss, size_t nameHash)
{
    for (int i = 0, c = ss->numAnimClips; i < c; i++) {
        if (ss->clips[i].nameHash = nameHash)
            return &ss->clips[i];
    }
    return nullptr;
}

inline void removeAnimAction(SpriteAnimImpl* anim)
{
    SpriteAnimAction* action = anim->action;
    if (action) {
        g_sprite->actionPool.deleteInstance(action);
        anim->action = action->next;
    } 
}

static void processSpriteAnimEvents(SpriteAnimHandle handle, SpriteSheet* sheet, int frameIdx)
{
    assert(frameIdx < sheet->numFrames);

    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, handle.value);
    size_t tag = sheet->tags[frameIdx];

    SpriteAnimEvent* ev = anim->events;
    while (ev) {
        if (ev->tagNameHash == tag) {
            ev->inCallback(handle, ev->userData);
            anim->lastCalledEvent = ev;
        } else if (anim->lastCalledEvent == ev) {
            if (anim->lastCalledEvent->outCallback)
                anim->lastCalledEvent->outCallback(handle, anim->lastCalledEvent->userData);
            anim->lastCalledEvent = nullptr;
        }
        ev = ev->next;
    }
    
}

void termite::stepSpriteAnim(SpriteAnimHandle handle, float dt)
{
    assert(handle.isValid());
    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, handle.value);
    SpriteImpl* sprite = g_sprite->spritePool.getHandleData<SpriteImpl>(0, anim->sprite.value);
    if (sprite->base != SpriteBase::FromSpriteSheet)
        return;

    SpriteSheet* ss = getResourcePtr<SpriteSheet>(sprite->resHandle);
    if (!ss)
        return;

    // Check For play action
    SpriteAnimAction* action = anim->action;
    if (action) {
        SpriteAnimClip* clip = searchClips(ss, action->clipNameHash);
        if (clip) {
            float t = anim->t;

            t += dt * anim->speed;
            float progress = t * clip->fps;
            float frames = bx::ffloor(progress);
            float reminder = frames != 0 ? bx::fmod(progress, frames) : progress;
            anim->t = reminder / clip->fps;
            
            int frameIdx = std::max<int>(sprite->frameIdx, clip->startFrame);
            int iframes = int(frames);
            if (!action->reverse) {
                if (action->looped) {
                    frameIdx = iwrap(frameIdx + iframes, clip->startFrame, clip->endFrame - 1);
                } else {
                    frameIdx = std::min<int>(frameIdx + iframes, clip->endFrame - 1);
                    if (frameIdx == (clip->endFrame - 1)) {
                        removeAnimAction(anim);
                        if (action->finishCallback)
                            action->finishCallback(handle, action->userData);
                    }
                }
            } else {
                if (action->looped) {
                    frameIdx = iwrap(frameIdx - iframes, clip->startFrame, clip->endFrame - 1);
                } else {
                    frameIdx = std::max<int>(frameIdx - iframes, clip->startFrame);
                    if (frameIdx == clip->startFrame) {
                        removeAnimAction(anim);
                        if (action->finishCallback)
                            action->finishCallback(handle, action->userData);
                    }
                }
            }            

            sprite->frameIdx = frameIdx;

            processSpriteAnimEvents(handle, ss, frameIdx);
        }  // if clip  
    }
}

void termite::queueSpriteAnim(SpriteAnimHandle handle, const char* clipName, bool looped, bool reverse /*= false*/,
                                  SpriteAnimCallback finishedCallback /*= nullptr*/, void* userData /*= nullptr*/)
{
    assert(handle.isValid());

    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, handle.value);
    SpriteAnimAction* action = g_sprite->actionPool.newInstance();
    if (action) {
        action->clipNameHash = tinystl::hash_string(clipName, strlen(clipName));
        action->looped = looped;
        action->reverse = reverse;
        action->finishCallback = finishedCallback;
        action->userData = userData;

        // Add to action list
        if (!anim->action) {
            anim->action = action;
        } else {
            SpriteAnimAction* lastAction = anim->action;
            while (lastAction->next != nullptr) {
                lastAction = lastAction->next;
            }
            lastAction->next = action;
        }
    }
}

void termite::addSpriteAnimEvent(SpriteAnimHandle handle, const char* tagName, SpriteAnimCallback tagInCallback,
                                 void* userData /*= nullptr*/, SpriteAnimCallback tagOutCallback /*= nullptr*/)
{
    assert(handle.isValid());

    size_t tagNameHash = tinystl::hash_string(tagName, strlen(tagName));
    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, handle.value);

    // Search in previous events, must not be duplicate
    SpriteAnimEvent* ev = anim->events;
    while (ev) {
        if (ev->tagNameHash == tagNameHash)
            return;
        ev = ev->next;
    }

    ev = g_sprite->eventPool.newInstance();
    if (ev) {
        ev->tagNameHash = tagNameHash;
        ev->inCallback = tagInCallback;
        ev->outCallback = tagOutCallback;
        ev->userData = userData;

        // Add to action list
        if (!anim->events) {
            anim->events = ev;
        } else {
            SpriteAnimEvent* lastEvent = anim->events;
            while (lastEvent->next != nullptr) {
                lastEvent = lastEvent->next;
            }
            lastEvent->next = ev;
        }
    }
}

void termite::resetSpriteAnim(SpriteAnimHandle handle)
{
    stopSpriteAnim(handle);
    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, handle.value);
    SpriteImpl* sprite = g_sprite->spritePool.getHandleData<SpriteImpl>(0, anim->sprite.value);

    anim->t = 0;
    sprite->frameIdx = 0;
}

void termite::stopSpriteAnim(SpriteAnimHandle handle)
{
    assert(handle.isValid());
    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, handle.value);
    // Delete all actions and stop
    while (anim->action) {
        SpriteAnimAction* action = anim->action;
        anim->action = anim->action->next;
        g_sprite->actionPool.deleteInstance(action);
    }
    anim->speed = 0;
}

void termite::setSpriteAnimPlaybackSpeed(SpriteAnimHandle handle, float speed)
{
    assert(handle.isValid());
    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, handle.value);
    anim->resumeSpeed = speed;
    anim->speed = speed;
}

float termite::getSpriteAnimPlaybackSpeed(SpriteAnimHandle handle)
{
    assert(handle.isValid());
    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, handle.value);
    return anim->resumeSpeed;
}

void termite::playSpriteAnim(SpriteAnimHandle handle)
{
    assert(handle.isValid());
    SpriteAnimImpl* anim = g_sprite->animPool.getHandleData<SpriteAnimImpl>(0, handle.value);
    anim->speed = anim->resumeSpeed;
}

void termite::setSpriteFrame(SpriteHandle handle, int frameIdx)
{
    SpriteImpl* sprite = (SpriteImpl*)g_sprite->spritePool.getHandleData<SpriteImpl>(0, handle.value);
    sprite->frameIdx = frameIdx;
}

int termite::getSpriteFrame(SpriteHandle handle)
{
    SpriteImpl* sprite = (SpriteImpl*)g_sprite->spritePool.getHandleData<SpriteImpl>(0, handle.value);
    return sprite->frameIdx;
}

void termite::setSpriteAnchor(SpriteHandle handle, const vec2_t localAnchor)
{
    SpriteImpl* sprite = (SpriteImpl*)g_sprite->spritePool.getHandleData<SpriteImpl>(0, handle.value);
    sprite->localAnchor = localAnchor;
}

vec2_t termite::getSpriteAnchor(SpriteHandle handle)
{
    SpriteImpl* sprite = (SpriteImpl*)g_sprite->spritePool.getHandleData<SpriteImpl>(0, handle.value);
    return sprite->localAnchor;
}

void termite::setSpriteSize(SpriteHandle handle, const vec2_t halfSize)
{
    SpriteImpl* sprite = (SpriteImpl*)g_sprite->spritePool.getHandleData<SpriteImpl>(0, handle.value);
    sprite->halfSize = halfSize;
}

vec2_t termite::getSpriteSize(SpriteHandle handle)
{
    SpriteImpl* sprite = (SpriteImpl*)g_sprite->spritePool.getHandleData<SpriteImpl>(0, handle.value);
    return sprite->halfSize;
}

void termite::registerSpriteSheetToResourceLib()
{
    ResourceTypeHandle handle;
    handle = registerResourceType("spritesheet", &g_sprite->loader, sizeof(LoadSpriteSheetParams));
    assert(handle.isValid());
}

void termite::drawSprites(uint8_t viewId, SpriteCache* spriteCache)
{

}

bool SpriteSheetLoader::loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj)
{
    const LoadSpriteSheetParams* ssParams = (const LoadSpriteSheetParams*)params.userParams;
    
    bx::MemoryReader reader(mem->data, mem->size);
    tsHeader header;
    bx::Error err;
    reader.read(&header, sizeof(header), &err);
    if (header.sign != TSPRITE_SIGN)
        return false;

    if (header.version != TSPRITE_VERSION)
        return false;

    // Load texture
    bx::Path texFilepath(header.textureFilepath);
    bx::Path texExt = texFilepath.getFileExt();
    LoadTextureParams texParams;
    texParams.flags = ssParams->flags;
    texParams.generateMips = ssParams->generateMips;
    texParams.skipMips = ssParams->skipMips;
    ResourceHandle textureHandle;
    if (texExt.isEqualNoCase("dds") || texExt.isEqualNoCase("ktx")) {
        textureHandle = loadResource("texture", texFilepath.cstr(), &texParams, params.flags);
    } else {
        textureHandle = loadResource("image", texFilepath.cstr(), &texParams, params.flags);
    }

    if (!textureHandle.isValid())
        return false;
    
    SpriteSheet* ss = (SpriteSheet*)BX_ALLOC(&g_sprite->allocStub, sizeof(SpriteSheet));
    memset(ss, 0x00, sizeof(SpriteSheet));
    ss->textureHandle = textureHandle;

    // Load Descriptor
    if (header.numSprites) {
        ss->frames = (vec4_t*)BX_ALLOC(&g_sprite->allocStub, sizeof(vec4_t)*header.numSprites);
        ss->tags = (size_t*)BX_ALLOC(&g_sprite->allocStub, sizeof(size_t)*header.numSprites);
        if (!ss->frames || !ss->tags)
            return false;
        ss->numFrames = header.numSprites;
        for (int i = 0; i < header.numSprites; i++) {
            tsSprite sprite;
            reader.read(&sprite, sizeof(sprite), &err);
            ss->frames[i] = vec4f(sprite.tx0, sprite.ty0, sprite.tx1, sprite.ty1);
            ss->tags[i] = (size_t)sprite.tag;
        }
    }

    if (header.numAnims) {
        ss->clips = (SpriteAnimClip*)BX_ALLOC(&g_sprite->allocStub, sizeof(SpriteAnimClip)*header.numAnims);
        if (!ss->clips)
            return false;
        ss->numAnimClips = header.numAnims;
        for (int i = 0; i < header.numAnims; i++) {
            tsAnimation anim;
            reader.read(&anim, sizeof(anim), &err);
            bx::strlcpy(ss->clips[i].name, anim.name, sizeof(ss->clips[i].name));
            ss->clips[i].nameHash = tinystl::hash_string(anim.name, strlen(anim.name));
            ss->clips[i].startFrame = anim.startFrame;
            ss->clips[i].endFrame = anim.endFrame;
            ss->clips[i].totalTime = float(anim.endFrame - anim.startFrame) / float(anim.fps);
            ss->clips[i].fps = float(anim.fps);
        }
    }

    *obj = (uintptr_t)ss;
    return true;
}

void SpriteSheetLoader::unloadObj(uintptr_t obj)
{
    assert(g_sprite);
    assert(obj);

    SpriteSheet* sheet = (SpriteSheet*)obj;
    if (sheet->textureHandle.isValid()) {
        unloadResource(sheet->textureHandle);
    }
    memset(sheet, 0x00, sizeof(SpriteSheet));
    sheet->textureHandle.reset();
}

void SpriteSheetLoader::onReload(ResourceHandle handle)
{
}

uintptr_t SpriteSheetLoader::getDefaultAsyncObj()
{
    return 0;
}