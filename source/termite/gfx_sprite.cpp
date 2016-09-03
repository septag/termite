#include "pch.h"
#include "gfx_sprite.h"
#include "gfx_texture.h"
#include "memory_pool.h"
#include "camera.h"

#include "bx/readerwriter.h"
#include "bxx/path.h"
#include "bxx/indexed_pool.h"
#include "bxx/array.h"

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

    enum class SpriteBase
    {
        FromTexture,
        FromSpriteSheet
    };

    struct Sprite
    {
        ResourceHandle resHandle;   // depends on base type
        SpriteBase base;
        vec2_t halfSize;
        vec2_t localAnchor;
        vec2_t topLeft;
        vec2_t bottomRight;
    };
}

struct SpriteVertex
{
    float x;
    float y;
    float z;
    float r;
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
    bool loadObj(ResourceLib* resLib, const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj) override;
    void unloadObj(ResourceLib* resLib, uintptr_t obj) override;
    void onReload(ResourceLib* resLib, ResourceHandle handle) override;
    uintptr_t getDefaultAsyncObj(ResourceLib* resLib) override;
};

struct SpriteSystem
{
    GfxDriverApi* driver;
    bx::AllocatorI* alloc;
    PageAllocator allocStub;
    bx::IndexedPool spritePool;
    ProgramHandle spriteProg;
    UniformHandle u_texture;
    SpriteSheetLoader loader;

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

    const uint32_t itemSizes[] = {
        sizeof(Sprite)
    };
    if (!g_sprite->spritePool.create(itemSizes, 1, poolSize, poolSize, &g_sprite->allocStub)) {
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

    g_sprite->spritePool.destroy();
    BX_DELETE(alloc, g_sprite);
    g_sprite = nullptr;
}

SpriteHandle termite::createSpriteFromTexture(ResourceHandle textureHandle, const vec2_t halfSize, 
                                              const vec2_t localAnchor /*= vec2f(0, 0)*/, 
                                              const vec2_t topLeftCoords /*= vec2f(0, 0)*/, 
                                              const vec2_t bottomRightCoords /*= vec2f(1.0f, 1.0f)*/)
{
    if (!textureHandle.isValid())
        return SpriteHandle();

    SpriteHandle handle(g_sprite->spritePool.newHandle());
    
    Sprite* sprite = g_sprite->spritePool.getHandleData<Sprite>(0, handle.value);
    sprite->base = SpriteBase::FromTexture;
    sprite->resHandle = textureHandle;
    sprite->halfSize = halfSize;
    sprite->localAnchor = localAnchor;
    sprite->topLeft = topLeftCoords;
    sprite->bottomRight = bottomRightCoords;
    
    return handle;
}

SpriteHandle termite::createSpriteFromSpritesheet(ResourceHandle spritesheetHandle, const vec2_t halfSize, 
                                                  const vec2_t localAnchor /*= vec2f(0, 0)*/, int index /*= -1*/)
{
    return SpriteHandle();
}

void termite::destroySprite(SpriteHandle handle)
{
    assert(handle.isValid());

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
                          const vec2_t* posBuffer, const float* rotBuffer, const color_t* tintColors,
                          ProgramHandle prog, SetSpriteStateCallback stateCallback)
{
    if (numSprites <= 0)
        return;

    GfxDriverApi* driver = g_sprite->driver;

    TransientVertexBuffer tvb;
    TransientIndexBuffer tib;
    
    const int numVerts = numSprites * 4;
    const int numIndices = numSprites * 6;
    GfxState baseState = gfxStateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite | GfxState::CullCCW;

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
        Sprite* sa = g_sprite->spritePool.getHandleData<Sprite>(0, a.handle.value);
        Sprite* sb = g_sprite->spritePool.getHandleData<Sprite>(0, b.handle.value);
        return sa->resHandle.value < sb->resHandle.value;
    });

    // Fill sprites
    SpriteVertex* verts = (SpriteVertex*)tvb.data;
    uint16_t* indices = (uint16_t*)tib.data;
    int indexIdx = 0;
    int vertexIdx = 0;
    for (int i = 0; i < numSprites; i++) {
        const SortedSprite& ss = sortedSprites[i];
        Sprite* sprite = g_sprite->spritePool.getHandleData<Sprite>(0, ss.handle.value);
        SpriteVertex& v0 = verts[vertexIdx];
        SpriteVertex& v1 = verts[vertexIdx + 1];
        SpriteVertex& v2 = verts[vertexIdx + 2];
        SpriteVertex& v3 = verts[vertexIdx + 3];

        vec2_t topleft = sprite->topLeft;
        vec2_t bottomright = sprite->bottomRight;
        vec2_t halfSize = sprite->halfSize;
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

        v0.z = v1.z = v2.z = v3.z = 0;
        v0.r = v1.r = v2.r = v3.r = rotBuffer[ss.index];

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
    };
    bx::Array<Batch> batches;
    batches.create(32, 64, tmpAlloc);

    ResourceHandle prevHandle;
    Batch* curBatch = nullptr;
    for (int i = 0; i < numSprites; i++) {
        Sprite* sprite = g_sprite->spritePool.getHandleData<Sprite>(0, sortedSprites[i].handle.value);
        ResourceHandle curHandle = g_sprite->spritePool.getHandleData<Sprite>(0, sortedSprites[i].handle.value)->resHandle;
        if (curHandle.value != prevHandle.value) {
            curBatch = batches.push();
            curBatch->index = i;
            curBatch->count = 0;
            prevHandle = curHandle;
        }
        curBatch->count++;
    }

    // Draw batches
    if (!prog.isValid())
        prog = g_sprite->spriteProg;

    for (int i = 0, c = batches.getCount(); i < c; i++) {
        const Batch& batch = batches[i];
        driver->setState(baseState, 0);
        driver->setTransientVertexBufferI(&tvb, 0, batch.count*4);
        driver->setTransientIndexBufferI(&tib, batch.index*6, batch.count*6);   
        Sprite* sprite = g_sprite->spritePool.getHandleData<Sprite>(0, sortedSprites[batch.index].handle.value);
        if (sprite->resHandle.isValid()) {
            TextureHandle texHandle;
            if (sprite->base == SpriteBase::FromTexture) {
                texHandle = ((Texture*)getResourceObj(nullptr, sprite->resHandle))->handle;
            } else if (sprite->base == SpriteBase::FromSpriteSheet) {
                SpriteSheet* sheet = (SpriteSheet*)getResourceObj(nullptr, sprite->resHandle);
                texHandle = ((Texture*)getResourceObj(nullptr, sheet->textureHandle))->handle;
            }
            driver->setTexture(0, g_sprite->u_texture, texHandle, TextureFlag::FromTexture);
        }

        if (stateCallback)
            stateCallback(driver);
        driver->submit(viewId, g_sprite->spriteProg, 0, false);
    }

    batches.destroy();
}

void termite::registerSpriteSheetToResourceLib(ResourceLib* resLib)
{
    ResourceTypeHandle handle;
    handle = registerResourceType(resLib, "spritesheet", &g_sprite->loader, sizeof(LoadSpriteSheetParams));
    assert(handle.isValid());
}

void termite::drawSprites(uint8_t viewId, SpriteCache* spriteCache)
{

}

bool SpriteSheetLoader::loadObj(ResourceLib* resLib, const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj)
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
        textureHandle = loadResource(resLib, "texture", texFilepath.cstr(), &texParams, params.flags);
    } else {
        textureHandle = loadResource(resLib, "image", texFilepath.cstr(), &texParams, params.flags);
    }

    if (!textureHandle.isValid())
        return false;
    
    SpriteSheet* ss = (SpriteSheet*)BX_ALLOC(&g_sprite->allocStub, sizeof(SpriteSheet));
    memset(ss, 0x00, sizeof(SpriteSheet));
    ss->textureHandle = textureHandle;

    // Load Descriptor
    if (header.numSprites) {
        ss->regions = (vec4_t*)BX_ALLOC(&g_sprite->allocStub, sizeof(vec4_t)*header.numSprites);
        if (!ss->regions)
            return false;
        ss->numRegions = header.numSprites;
        for (int i = 0; i < header.numSprites; i++) {
            tsSprite sprite;
            reader.read(&sprite, sizeof(sprite), &err);
            ss->regions[i] = vec4f(sprite.tx0, sprite.ty0, sprite.tx1, sprite.ty1);
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
            ss->clips[i].fps = float(anim.fps);
            ss->clips[i].startFrame = anim.startFrame;
            ss->clips[i].endFrame = anim.endFrame;
        }
    }

    *obj = (uintptr_t)ss;
    return true;
}

void SpriteSheetLoader::unloadObj(ResourceLib* resLib, uintptr_t obj)
{
    assert(g_sprite);
    assert(obj);

    SpriteSheet* sheet = (SpriteSheet*)obj;
    if (sheet->textureHandle.isValid()) {
        unloadResource(resLib, sheet->textureHandle);
    }
    memset(sheet, 0x00, sizeof(SpriteSheet));
    sheet->textureHandle.reset();
}

void SpriteSheetLoader::onReload(ResourceLib* resLib, ResourceHandle handle)
{
}

uintptr_t SpriteSheetLoader::getDefaultAsyncObj(ResourceLib* resLib)
{
    return 0;
}