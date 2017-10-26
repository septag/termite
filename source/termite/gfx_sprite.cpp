#include "pch.h"
#include "gfx_sprite.h"
#include "gfx_texture.h"
#include "math_util.h"

#include "bx/readerwriter.h"
#include "bxx/path.h"
#include "bxx/array.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/logger.h"
#include "bxx/linear_allocator.h"

#include "rapidjson/error/en.h"
#include "rapidjson/document.h"
#include "bxx/rapidjson_allocator.h"

#include T_MAKE_SHADER_PATH(shaders_h, sprite.vso)
#include T_MAKE_SHADER_PATH(shaders_h, sprite.fso)
#include T_MAKE_SHADER_PATH(shaders_h, sprite_add.vso)
#include T_MAKE_SHADER_PATH(shaders_h, sprite_add.fso)

#include <algorithm>

using namespace rapidjson;
using namespace termite;

static const uint64_t kSpriteKeyOrderBits = 8;
static const uint64_t kSpriteKeyOrderMask = (1 << kSpriteKeyOrderBits) - 1;
static const uint64_t kSpriteKeyTextureBits = 16;
static const uint64_t kSpriteKeyTextureMask = (1 << kSpriteKeyTextureBits) - 1;
static const uint64_t kSpriteKeyIdBits = 32;
static const uint64_t kSpriteKeyIdMask = (1llu << kSpriteKeyIdBits) - 1;

static const uint8_t kSpriteKeyOrderShift = kSpriteKeyTextureBits + kSpriteKeyIdBits;
static const uint8_t kSpriteKeyTextureShift = kSpriteKeyIdBits;

#define MAKE_SPRITE_KEY(_Order, _Texture, _Id) \
    (uint64_t(_Order & kSpriteKeyOrderMask) << kSpriteKeyOrderShift) | (uint64_t(_Texture & kSpriteKeyTextureMask) << kSpriteKeyTextureShift) | uint64_t(_Id & kSpriteKeyIdMask)
#define SPRITE_KEY_GET_BATCH(_Key) uint32_t((_Key >> kSpriteKeyIdBits) & kSpriteKeyIdMask)

struct SpriteVertex
{
    vec2_t pos;
    vec3_t transform1;
    vec3_t transform2;
    vec2_t coords;
    uint32_t color;

    static void init()
    {
        vdeclBegin(&Decl);
        vdeclAdd(&Decl, VertexAttrib::Position, 2, VertexAttribType::Float);        // pos
        vdeclAdd(&Decl, VertexAttrib::TexCoord0, 3, VertexAttribType::Float);       // transform mat (part 1)
        vdeclAdd(&Decl, VertexAttrib::TexCoord1, 3, VertexAttribType::Float);       // transform mat (part 2)
        vdeclAdd(&Decl, VertexAttrib::TexCoord2, 2, VertexAttribType::Float);       // texture coords
        vdeclAdd(&Decl, VertexAttrib::Color0, 4, VertexAttribType::Uint8, true);    // color
        vdeclEnd(&Decl);
    }

    static VertexDecl Decl;
};
VertexDecl SpriteVertex::Decl;

class SpriteSheetLoader : public ResourceCallbacksI
{
public:
    bool loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
    void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
    void onReload(ResourceHandle handle, bx::AllocatorI* alloc) override;
};

struct SpriteMesh
{
    vec2_t* verts;
    vec2_t* uvs;
    uint16_t* tris;
    int numVerts;
    int numTris;

    SpriteMesh() :
        verts(nullptr),
        uvs(nullptr),
        tris(nullptr),
        numVerts(0),
        numTris(0)
    {
    }
};

struct SpriteDrawBatch
{
    ResourceHandle texHandle;
    uint16_t startIdx;
    uint16_t numIndices;
    uint32_t numVerts;
};

struct SortedSprite
{
    int index;
    Sprite* sprite;
    uint64_t key;
};

struct SpriteFrame
{
    ResourceHandle texHandle;   // Handle to spritesheet/texture resource
    ResourceHandle ssHandle;    // For spritesheets we have spritesheet handle

    size_t nameHash;
    size_t tagHash;
    int meshId;

    rect_t frame;       // top-left, right-bottom
    vec2_t pivot;       // In textures it should be provided, in spritesheets, this value comes from the file
    vec2_t sourceSize;  // Retreive from texture or spritesheet
    vec2_t posOffset;   // Offset to move the image in source bounds (=0 in texture sprites)
    vec2_t sizeOffset;  // Offset to resize the image in source bounds (=1 in texture sprites)
    float rotOffset;    // Rotation offset to fit the original (=0 in texture sprites)
    float pixelRatio;

    SpriteFrameCallback frameCallback;
    void* frameCallbackUserData;

    uint8_t flags;

    SpriteFrame() :
        nameHash(0),
        tagHash(0),
        meshId(-1),
        frameCallback(nullptr),
        frameCallbackUserData(nullptr)
    {
    }
};

namespace termite
{
    struct Sprite
    {
        typedef bx::List<Sprite*>::Node LNode;

        uint32_t id;
        bx::AllocatorI* alloc;
        vec2_t halfSize;
        vec2_t sizeMultiplier;
        vec2_t posOffset;
        bx::Array<SpriteFrame> frames;
        int curFrameIdx;

        float animTm;       // Animation timer
        bool playReverse;
        float playSpeed;
        float resumeSpeed;
        color_t tint;
        uint8_t order;

        SpriteFlag::Bits flip;
        LNode lnode;

        SpriteFrameCallback endCallback;
        void* endUserData;
        void* userData;
        bool triggerEndCallback;

        Sprite(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            curFrameIdx(0),
            animTm(0),
            playReverse(false),
            playSpeed(30.0f),
            resumeSpeed(30.0f),
            order(0),
            flip(SpriteFlag::None),
            lnode(this),
            endCallback(nullptr),
            endUserData(nullptr),
            triggerEndCallback(false)
        {
            tint = color1n(0xffffffff);
            posOffset = vec2f(0, 0);
            sizeMultiplier = vec2f(1.0f, 1.0f);
            userData = nullptr;
        }

        inline const SpriteFrame& getCurFrame() const
        {
            return frames[curFrameIdx];
        }
    };
}

struct SpriteSheetFrame
{
    size_t filenameHash;
    rect_t frame;
    vec2_t pivot;
    vec2_t sourceSize;
    vec2_t posOffset;
    vec2_t sizeOffset;
    float rotOffset;
    float pixelRatio;
};

struct SpriteSheet
{
    void* buff;
    SpriteSheetFrame* frames;
    SpriteMesh* meshes;         // If meshes is not nullptr, we have a mesh for each frame
    int numFrames;
    ResourceHandle texHandle;
    uint8_t padding[2];

    SpriteSheet() :
        buff(nullptr),
        numFrames(0),
        frames(nullptr),
        meshes(nullptr)
    {
    }
};

struct SpriteSystem
{
    GfxDriverApi* driver;
    bx::AllocatorI* alloc;
    ProgramHandle spriteProg;
    ProgramHandle spriteAddProg;
    UniformHandle u_texture;
    SpriteSheetLoader loader;
    SpriteSheet* failSheet;
    SpriteSheet* asyncSheet;
    bx::List<Sprite*> spriteList;       // keep a list of sprites for proper shutdown and sheet reloading
    SpriteRenderMode::Enum renderMode;

    SpriteSystem(bx::AllocatorI* _alloc) : 
        driver(nullptr),
        alloc(_alloc),
        failSheet(nullptr),
        asyncSheet(nullptr),
        renderMode(SpriteRenderMode::Normal)
    {
    }
};

static SpriteSystem* g_spriteSys = nullptr;

static const SpriteSheetFrame* findSpritesheetFrame(const SpriteSheet* sheet, size_t nameHash, int* index)
{
    for (int i = 0, c = sheet->numFrames; i < c; i++) {
        if (sheet->frames[i].filenameHash != nameHash)
            continue;
        *index = i;
        return &sheet->frames[i];
    }

    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SpriteSheetLoader
template <typename _T>
vec2_t* loadSpriteVerts(const _T& jnode, int* numVerts, bx::AllocatorI* alloc)
{
    int count = jnode.Size();
    vec2_t* verts = nullptr;
    vec2i_t ivert;
    if (count > 0) {
        verts = (vec2_t*)BX_ALLOC(alloc, sizeof(vec2_t)*count);
        for (int i = 0; i < count; i++) {
            getIntArray<_T>(jnode[i], ivert.n, 2);
            verts[i] = vec2f(float(ivert.x), float(ivert.y));
        }
    }
    *numVerts = count;
    return verts;
}

template <typename _T>
uint16_t* loadSpriteTris(const _T& jnode, int* numTries, bx::AllocatorI* alloc)
{
    int count = jnode.Size();
    uint16_t* tris = nullptr;
    if (count > 0) {
        tris = (uint16_t*)BX_ALLOC(alloc, sizeof(uint16_t)*count*3);
        for (int i = 0; i < count; i++) {
            getUInt16Array<_T>(jnode[i], tris + 3*i, 3);
        }
    }
    *numTries = count;
    return tris;
}

bool SpriteSheetLoader::loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, 
                                bx::AllocatorI* alloc)
{

    const LoadSpriteSheetParams* ssParams = (const LoadSpriteSheetParams*)params.userParams;

    bx::AllocatorI* tmpAlloc = getTempAlloc();
    char* jsonStr = (char*)BX_ALLOC(tmpAlloc, mem->size + 1);
    if (!jsonStr) {
        T_ERROR("Out of Memory");
        return false;
    }
    memcpy(jsonStr, mem->data, mem->size);
    jsonStr[mem->size] = 0;

    BxAllocatorNoFree jalloc(tmpAlloc);
    BxAllocator jpool(4096, &jalloc);
    BxDocument jdoc(&jpool, 1024, &jalloc);

    if (jdoc.ParseInsitu(jsonStr).HasParseError()) {
        T_ERROR("Parse Json Error: %s (Pos: %s)", GetParseError_En(jdoc.GetParseError()), jdoc.GetErrorOffset());                 
        return false;
    }
    BX_FREE(tmpAlloc, jsonStr);

    if (!jdoc.HasMember("frames") || !jdoc.HasMember("meta")) {
        T_ERROR("SpriteSheet Json is Invalid");
        return false;
    }
    const BxValue& jframes = jdoc["frames"];
    const BxValue& jmeta = jdoc["meta"];

    assert(jframes.IsArray());
    int numFrames = jframes.Size();
    if (numFrames == 0)
        return false;

    // evaluate total vertices, triangles and uvs
    int numTotalVerts = 0, numTotalTris = 0, numTotalUvs = 0;
    for (int i = 0; i < numFrames; i++) {
        const BxValue& jframe = jframes[i];
        if (jframe.HasMember("vertices")) {
            numTotalVerts += jframe["vertices"].Size();
            if (jframe.HasMember("verticesUV"))
                numTotalUvs += jframe["verticesUV"].Size();
            if (jframe.HasMember("triangles"))
                numTotalTris += jframe["triangles"].Size();
        } else {
            numTotalVerts += 4;
            numTotalUvs += 4;
            numTotalTris += 6;
        }        
    }

    // Create sprite sheet
    int totalAllocs = 3 + numFrames*3;
    size_t totalSz = sizeof(SpriteSheet) + numFrames*sizeof(SpriteSheetFrame) +
        numTotalVerts*sizeof(vec2_t) + numTotalTris*sizeof(uint16_t)*3 + numTotalUvs*sizeof(vec2_t) + 
        numFrames*sizeof(SpriteMesh) + bx::LinearAllocator::getExtraAllocSize(totalAllocs);
    uint8_t* buff = (uint8_t*)BX_ALLOC(alloc ? alloc : g_spriteSys->alloc, totalSz);
    if (!buff)
        return false;
    bx::LinearAllocator lalloc(buff, totalSz);
    SpriteSheet* ss = BX_NEW(&lalloc, SpriteSheet);
    ss->buff = buff;
    ss->frames = (SpriteSheetFrame*)BX_ALLOC(&lalloc, numFrames*sizeof(SpriteSheetFrame));
    ss->numFrames = numFrames;
    ss->meshes = (SpriteMesh*)BX_ALLOC(&lalloc, numFrames*sizeof(SpriteMesh));

    // image width/height
    const BxValue& jsize = jmeta["size"];
    float imgWidth = float(jsize["w"].GetInt());
    float imgHeight = float(jsize["h"].GetInt());

    // Make texture path and load it
    const char* imageFile = jmeta["image"].GetString();
    bx::Path texFilepath = bx::Path(params.uri).getDirectory();
    texFilepath.joinUnix(imageFile);

    LoadTextureParams texParams;
    texParams.flags = ssParams->flags;
    texParams.generateMips = ssParams->generateMips;
    texParams.skipMips = ssParams->skipMips;
    texParams.fmt = ssParams->fmt;
    ss->texHandle = loadResource("texture", texFilepath.cstr(), &texParams, params.flags, alloc ? alloc : nullptr);

    for (int i = 0; i < numFrames; i++) {
        SpriteSheetFrame& frame = ss->frames[i];
        const BxValue& jframe = jframes[i];
        const char* filename = jframe["filename"].GetString();
        frame.filenameHash = tinystl::hash_string(filename, strlen(filename));
        bool rotated = jframe["rotated"].GetBool();

        const BxValue& jframeFrame = jframe["frame"];
        float frameWidth = float(jframeFrame["w"].GetInt());
        float frameHeight = float(jframeFrame["h"].GetInt());
        if (rotated)
            std::swap<float>(frameWidth, frameHeight);

        frame.frame = rectwh(float(jframeFrame["x"].GetInt()) / imgWidth,
                              float(jframeFrame["y"].GetInt()) / imgHeight,
                              frameWidth / imgWidth,
                              frameHeight / imgHeight);

        const BxValue& jsourceSize = jframe["sourceSize"];
        frame.sourceSize = vec2f(float(jsourceSize["w"].GetInt()),
                                 float(jsourceSize["h"].GetInt()));

        // Normalize pos/size offsets (0~1)
        // Rotate offset can only be 90 degrees
        const BxValue& jssFrame = jframe["spriteSourceSize"];
        float srcx = float(jssFrame["x"].GetInt());
        float srcy = float(jssFrame["y"].GetInt());
        float srcw = float(jssFrame["w"].GetInt());
        float srch = float(jssFrame["h"].GetInt());

        frame.sizeOffset = vec2f(srcw / frame.sourceSize.x, srch / frame.sourceSize.y);

        if (!rotated) {
            frame.rotOffset = 0;
        } else {
            std::swap<float>(srcw, srch);
            frame.rotOffset = -90.0f;
        }
        frame.pixelRatio = frame.sourceSize.x / frame.sourceSize.y;

        const BxValue& jpivot = jframe["pivot"];
        const BxValue& jpivotX = jpivot["x"];
        const BxValue& jpivotY = jpivot["y"];
        float pivotx = jpivotX.IsFloat() ?  jpivotX.GetFloat() : float(jpivotX.GetInt());
        float pivoty = jpivotY.IsFloat() ?  jpivotY.GetFloat() : float(jpivotY.GetInt());
        frame.pivot = vec2f(pivotx - 0.5f, -pivoty + 0.5f);     // convert to our coordinates

        // PosOffset is the center of the sprite in -0.5~0.5 coords
        frame.posOffset = vec2f( (srcx + srcw*0.5f)/frame.sourceSize.x - 0.5f, 
                                -(srcy + srch*0.5f)/frame.sourceSize.y + 0.5f);

        // Mesh
        if (jframe.HasMember("vertices")) {
            SpriteMesh& mesh = ss->meshes[i];
            const BxValue& jverts = jframe["vertices"];
            mesh.verts = loadSpriteVerts(jverts, &mesh.numVerts, &lalloc);
            
            // Convert vertices to (-0.5-0.5) of the sprite frame
            for (int i = 0; i < mesh.numVerts; i++) {
                vec2_t pixelPos = mesh.verts[i];
                mesh.verts[i] = vec2f(pixelPos.x/frame.sourceSize.x - 0.5f, -pixelPos.y/frame.sourceSize.y + 0.5f);
            }

            if (jframe.HasMember("verticesUV")) {
                mesh.uvs = loadSpriteVerts(jframe["verticesUV"], &mesh.numVerts, &lalloc);
                // Convert UVs
                float invWidth = 1.0f/imgWidth;
                float invHeight = 1.0f/imgHeight;
                for (int j = 0; j < mesh.numVerts; j++) {
                    mesh.uvs[j].x *= invWidth;
                    mesh.uvs[j].y *= invHeight;
                }
            }

            if (jframe.HasMember("triangles"))
                mesh.tris = loadSpriteTris(jframe["triangles"], &mesh.numTris, &lalloc);
        } else {
            rect_t texcoords = frame.frame;
            vec2_t topLeft = vec2f(srcx/frame.sourceSize.x - 0.5f,
                                   -srcy/frame.sourceSize.y + 0.5f);
            vec2_t rightBottom = vec2f((srcx + srcw)/frame.sourceSize.x - 0.5f,
                                       -(srcy + srch)/frame.sourceSize.y + 0.5f);
            SpriteMesh& mesh = ss->meshes[i];
            mesh.numVerts = 4;
            mesh.verts = (vec2_t*)BX_ALLOC(&lalloc, sizeof(vec2_t)*mesh.numVerts);
            mesh.uvs = (vec2_t*)BX_ALLOC(&lalloc, sizeof(vec2_t)*mesh.numVerts);
            mesh.numTris = 2;
            mesh.tris = (uint16_t*)BX_ALLOC(&lalloc, sizeof(uint16_t)*mesh.numTris*3);

            mesh.verts[0] = topLeft;
            mesh.uvs[0] = texcoords.vmin;

            mesh.verts[1] = vec2f(rightBottom.x, topLeft.y);
            mesh.uvs[1] = vec2f(texcoords.xmax, texcoords.ymin);

            mesh.verts[2] = vec2f(topLeft.x, rightBottom.y);
            mesh.uvs[2] = vec2f(texcoords.xmin, texcoords.ymax);

            mesh.verts[3] = rightBottom;
            mesh.uvs[3] = texcoords.vmax;

            mesh.tris[0] = 0;       mesh.tris[1] = 1;       mesh.tris[2] = 2;
            mesh.tris[3] = 2;       mesh.tris[4] = 1;       mesh.tris[5] = 3;
        }
    }

    *obj = uintptr_t(ss);
    return true;
}

void SpriteSheetLoader::unloadObj(uintptr_t obj, bx::AllocatorI* alloc)
{
    assert(g_spriteSys);
    assert(obj);

    SpriteSheet* sheet = (SpriteSheet*)obj;
    if (sheet->texHandle.isValid()) {
        unloadResource(sheet->texHandle);
        sheet->texHandle.reset();
    }
    
    BX_FREE(alloc ? alloc : g_spriteSys->alloc, sheet->buff);
}

void SpriteSheetLoader::onReload(ResourceHandle handle, bx::AllocatorI* alloc)
{
    // Search in sprites, check if spritesheet is the same and reload the data
    Sprite::LNode* node = g_spriteSys->spriteList.getFirst();
    while (node) {
        Sprite* sprite = node->data;
        for (int i = 0, c = sprite->frames.getCount(); i < c; i++) {
            if (sprite->frames[i].ssHandle != handle)
                continue;

            // find the frame in spritesheet and update data
            SpriteFrame& frame = sprite->frames[i];
            SpriteSheet* sheet = getResourcePtr<SpriteSheet>(handle);
            int index;
            const SpriteSheetFrame* sheetFrame = findSpritesheetFrame(sheet, frame.nameHash, &index);
            if (sheetFrame) {
                frame.meshId = index;
                frame.texHandle = sheet->texHandle;
                frame.pivot = sheetFrame->pivot;
                frame.frame = sheetFrame->frame;
                frame.sourceSize = sheetFrame->sourceSize;
                frame.posOffset = sheetFrame->posOffset;
                frame.sizeOffset = sheetFrame->sizeOffset;
                frame.rotOffset = sheetFrame->rotOffset;
                frame.pixelRatio = sheetFrame->pixelRatio;
            } else {
                frame.meshId = -1;
                frame.texHandle = getResourceFailHandle("texture");
                Texture* tex = getResourcePtr<Texture>(frame.texHandle);
                frame.pivot = vec2f(0, 0);
                frame.frame = rectf(0, 0, 1.0f, 1.0f);
                frame.sourceSize = vec2f(float(tex->info.width), float(tex->info.height));
                frame.posOffset = vec2f(0, 0);
                frame.sizeOffset = vec2f(1.0f, 1.0f);
                frame.rotOffset = 0;
                frame.pixelRatio = 1.0f;
            }
        }
        node = node->next;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static SpriteSheet* createDummySpriteSheet(ResourceHandle texHandle, bx::AllocatorI* alloc)
{
    // Create sprite sheet
    size_t totalSz = sizeof(SpriteSheet) + sizeof(SpriteSheetFrame) + sizeof(SpriteMesh) + bx::LinearAllocator::getExtraAllocSize(3);
    uint8_t* buff = (uint8_t*)BX_ALLOC(alloc, totalSz);
    if (!buff) {
        T_ERROR("Out of Memory");
        return nullptr;
    }

    bx::LinearAllocator lalloc(buff, totalSz);
    SpriteSheet* ss = BX_NEW(&lalloc, SpriteSheet); 
    ss->buff = buff;
    ss->frames = (SpriteSheetFrame*)BX_ALLOC(&lalloc, sizeof(SpriteSheetFrame));
    ss->numFrames = 1;
    ss->meshes = (SpriteMesh*)BX_ALLOC(&lalloc, sizeof(SpriteMesh));

    ss->texHandle = getResourceFailHandle("texture");
    assert(ss->texHandle.isValid());

    Texture* tex = getResourcePtr<Texture>(ss->texHandle);
    float imgWidth = float(tex->info.width);
    float imgHeight = float(tex->info.height);

    ss->frames[0].filenameHash = 0;
    ss->frames[0].frame = rectf(0, 0, 1.0f, 1.0f);
    ss->frames[0].pivot = vec2f(0, 0);
    ss->frames[0].posOffset = vec2f(0, 0);
    ss->frames[0].sizeOffset = vec2f(1.0f, 1.0f);
    ss->frames[0].sourceSize = vec2f(imgWidth, imgHeight);
    ss->frames[0].rotOffset = 0;

    static uint16_t tris[] = {
        0, 1, 2,
        2, 1, 3
    };

    static vec2_t verts[] = {
        vec2f(-0.5f, 0.5f),
        vec2f(0.5f, 0.5f),
        vec2f(-0.5f, -0.5f),
        vec2f(0.5f, -0.5f)   
    };

    static vec2_t uvs[] = {
        vec2f(0, 0),
        vec2f(1.0f, 0),
        vec2f(0, 1.0f),
        vec2f(1.0f, 1.0f)
    };

    ss->meshes[0].numTris = 2;
    ss->meshes[0].numVerts = 4;
    ss->meshes[0].tris = tris;
    ss->meshes[0].verts = verts;
    ss->meshes[0].uvs = uvs;

    return ss;
}

result_t termite::initSpriteSystem(GfxDriverApi* driver, bx::AllocatorI* alloc)
{
    if (g_spriteSys) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_spriteSys = BX_NEW(alloc, SpriteSystem)(alloc);
    if (!g_spriteSys) {
        return T_ERR_OUTOFMEM;
    }

    g_spriteSys->driver = driver;

    SpriteVertex::init();

    g_spriteSys->spriteProg = 
        driver->createProgram(driver->createShader(driver->makeRef(sprite_vso, sizeof(sprite_vso), nullptr, nullptr)),
                              driver->createShader(driver->makeRef(sprite_fso, sizeof(sprite_fso), nullptr, nullptr)),
                              true);
    if (!g_spriteSys->spriteProg.isValid())
        return T_ERR_FAILED;

    g_spriteSys->spriteAddProg = 
        driver->createProgram(driver->createShader(driver->makeRef(sprite_add_vso, sizeof(sprite_add_vso), nullptr, nullptr)),
                              driver->createShader(driver->makeRef(sprite_add_fso, sizeof(sprite_add_fso), nullptr, nullptr)),
                              true);
    if (!g_spriteSys->spriteAddProg.isValid())
        return T_ERR_FAILED;

    g_spriteSys->u_texture = driver->createUniform("u_texture", UniformType::Int1, 1);

    // Create fail spritesheet
    g_spriteSys->failSheet = createDummySpriteSheet(getResourceFailHandle("texture"), g_spriteSys->alloc);    
    g_spriteSys->asyncSheet = createDummySpriteSheet(getResourceAsyncHandle("texture"), g_spriteSys->alloc);
    if (!g_spriteSys->failSheet || !g_spriteSys->asyncSheet) {
        T_ERROR("Creating async/fail spritesheets failed");
        return T_ERR_FAILED;
    }

    return 0;
}

Sprite* termite::createSprite(bx::AllocatorI* alloc, const vec2_t halfSize)
{
    static uint32_t id = 0;

    Sprite* sprite = BX_NEW(alloc, Sprite)(alloc);
    if (!sprite)
        return nullptr;
    sprite->id = ++id;
    if (!sprite->frames.create(4, 32, alloc))
        return nullptr;
    sprite->halfSize = halfSize;
    // Add to sprite list
    g_spriteSys->spriteList.addToEnd(&sprite->lnode);
    return sprite;
}

void termite::destroySprite(Sprite* sprite)
{
    // Destroy frames
    for (int i = 0, c = sprite->frames.getCount(); i < c; i++) {
        SpriteFrame& frame = sprite->frames[i];
        if (frame.flags & SpriteFlag::DestroyResource) {
            if (frame.ssHandle.isValid())
                unloadResource(frame.ssHandle);
            else
                unloadResource(frame.texHandle);
        }
    }
    sprite->frames.destroy();

    // Remove from list
    g_spriteSys->spriteList.remove(&sprite->lnode);

    BX_DELETE(sprite->alloc, sprite);
}

void termite::shutdownSpriteSystem()
{
    if (!g_spriteSys)
        return;

    bx::AllocatorI* alloc = g_spriteSys->alloc;
    GfxDriverApi* driver = g_spriteSys->driver;

    if (g_spriteSys->spriteProg.isValid())
        driver->destroyProgram(g_spriteSys->spriteProg);
    if (g_spriteSys->spriteAddProg.isValid())
        driver->destroyProgram(g_spriteSys->spriteAddProg);
    if (g_spriteSys->u_texture.isValid())
        driver->destroyUniform(g_spriteSys->u_texture);

    // Move through the remaining sprites and unload their resources
    Sprite::LNode* node = g_spriteSys->spriteList.getFirst();
    while (node) {
        Sprite::LNode* next = node->next;
        destroySprite(node->data);
        node = next;
    }

    if (g_spriteSys->failSheet)
        g_spriteSys->loader.unloadObj(uintptr_t(g_spriteSys->failSheet), g_spriteSys->alloc);
    if (g_spriteSys->asyncSheet)
        g_spriteSys->loader.unloadObj(uintptr_t(g_spriteSys->asyncSheet), g_spriteSys->alloc);

    BX_DELETE(alloc, g_spriteSys);
    g_spriteSys = nullptr;
}

bool termite::initSpriteSystemGraphics(GfxDriverApi* driver)
{
    g_spriteSys->driver = driver;

    g_spriteSys->spriteProg =
        driver->createProgram(driver->createShader(driver->makeRef(sprite_vso, sizeof(sprite_vso), nullptr, nullptr)),
                              driver->createShader(driver->makeRef(sprite_fso, sizeof(sprite_fso), nullptr, nullptr)),
                              true);
    if (!g_spriteSys->spriteProg.isValid())
        return false;

    g_spriteSys->spriteAddProg =
        driver->createProgram(driver->createShader(driver->makeRef(sprite_add_vso, sizeof(sprite_add_vso), nullptr, nullptr)),
                              driver->createShader(driver->makeRef(sprite_add_fso, sizeof(sprite_add_fso), nullptr, nullptr)),
                              true);
    if (!g_spriteSys->spriteAddProg.isValid())
        return false;

    g_spriteSys->u_texture = driver->createUniform("u_texture", UniformType::Int1, 1);

    return true;
}

void termite::shutdownSpriteSystemGraphics()
{
    GfxDriverApi* driver = g_spriteSys->driver;

    if (g_spriteSys->spriteProg.isValid())
        driver->destroyProgram(g_spriteSys->spriteProg);
    if (g_spriteSys->spriteAddProg.isValid())
        driver->destroyProgram(g_spriteSys->spriteAddProg);
    if (g_spriteSys->u_texture.isValid())
        driver->destroyUniform(g_spriteSys->u_texture);
}

void termite::setSpriteRenderMode(SpriteRenderMode::Enum mode)
{
    g_spriteSys->renderMode = mode;
}

SpriteRenderMode::Enum termite::getSpriteRenderMode()
{
    return g_spriteSys->renderMode;
}

void termite::addSpriteFrameTexture(Sprite* sprite, 
                                    ResourceHandle texHandle, SpriteFlag::Bits flags, const vec2_t pivot /*= vec2_t(0, 0)*/, 
                                    const vec2_t topLeftCoords /*= vec2_t(0, 0)*/, const vec2_t bottomRightCoords /*= vec2_t(1.0f, 1.0f)*/, 
                                    const char* frameTag /*= nullptr*/)
{
    if (texHandle.isValid()) {
        assert(getResourceLoadState(texHandle) != ResourceLoadState::LoadInProgress);
 
        SpriteFrame* frame = BX_PLACEMENT_NEW(sprite->frames.push(), SpriteFrame);
        if (frame) {
            frame->texHandle = texHandle;
            frame->flags = flags;
            frame->pivot = pivot;
            frame->frame = rectv(topLeftCoords, bottomRightCoords);
            frame->tagHash = !frameTag ? 0 : tinystl::hash_string(frameTag, strlen(frameTag)) ;

            Texture* tex = getResourcePtr<Texture>(texHandle);
            frame->sourceSize = vec2f(float(tex->info.width), float(tex->info.height));
            frame->posOffset = vec2f(0, 0);
            frame->sizeOffset = vec2f(1.0f, 1.0f);
            frame->rotOffset = 0;
            frame->pixelRatio = ((bottomRightCoords.x - topLeftCoords.x)*frame->sourceSize.x) /
                ((bottomRightCoords.y - topLeftCoords.y)*frame->sourceSize.y);
        }
    }
}

void termite::addSpriteFrameSpritesheet(Sprite* sprite,
                                        ResourceHandle ssHandle, const char* name, SpriteFlag::Bits flags, 
                                        const char* frameTag /*= nullptr*/)
{
    assert(name);
    if (ssHandle.isValid()) {
        assert(getResourceLoadState(ssHandle) != ResourceLoadState::LoadInProgress);
        
        SpriteFrame* frame = BX_PLACEMENT_NEW(sprite->frames.push(), SpriteFrame);
        if (frame) {
            frame->ssHandle = ssHandle;
            SpriteSheet* sheet = getResourcePtr<SpriteSheet>(ssHandle);

            // find frame name in spritesheet, if found fill the frame data
            size_t nameHash = tinystl::hash_string(name, strlen(name));
            int index;
            const SpriteSheetFrame* sheetFrame = findSpritesheetFrame(sheet, nameHash, &index);

            frame->nameHash = nameHash;
            frame->flags = flags;
            frame->tagHash = !frameTag ? 0 : tinystl::hash_string(frameTag, strlen(frameTag));

            // If not found fill dummy data
            if (sheetFrame) {
                frame->meshId = index;
                frame->texHandle = sheet->texHandle;
                frame->pivot = sheetFrame->pivot;
                frame->frame = sheetFrame->frame;
                frame->sourceSize = sheetFrame->sourceSize;
                frame->posOffset = sheetFrame->posOffset;
                frame->sizeOffset = sheetFrame->sizeOffset;
                frame->rotOffset = sheetFrame->rotOffset;
                frame->pixelRatio = sheetFrame->pixelRatio;
            } else {
                frame->texHandle = getResourceFailHandle("texture");
                Texture* tex = getResourcePtr<Texture>(frame->texHandle);
                frame->flags = flags;
                frame->pivot = vec2f(0, 0);
                frame->frame = rectf(0, 0, 1.0f, 1.0f);
                frame->sourceSize = vec2f(float(tex->info.width), float(tex->info.height));
                frame->posOffset = vec2f(0, 0);
                frame->sizeOffset = vec2f(1.0f, 1.0f);
                frame->rotOffset = 0;
                frame->pixelRatio = 1.0f;
            }
        }
    }
}

void termite::addSpriteFrameAll(Sprite* sprite, ResourceHandle ssHandle, SpriteFlag::Bits flags)
{
    if (ssHandle.isValid()) {
        assert(getResourceLoadState(ssHandle) != ResourceLoadState::LoadInProgress);

        SpriteSheet* sheet = getResourcePtr<SpriteSheet>(ssHandle);
        for (int i = 0, c = sheet->numFrames; i < c; i++) {
            const SpriteSheetFrame& sheetFrame = sheet->frames[i];
            SpriteFrame* frame = BX_PLACEMENT_NEW(sprite->frames.push(), SpriteFrame);

            frame->meshId = i;
            frame->texHandle = sheet->texHandle;
            frame->ssHandle = ssHandle;
            frame->nameHash = sheetFrame.filenameHash;
            frame->tagHash = 0;
            frame->flags = flags;
            frame->pivot = sheetFrame.pivot;
            frame->frame = sheetFrame.frame;
            frame->sourceSize = sheetFrame.sourceSize;
            frame->posOffset = sheetFrame.posOffset;
            frame->sizeOffset = sheetFrame.sizeOffset;
            frame->rotOffset = sheetFrame.rotOffset;
            frame->pixelRatio = sheetFrame.pixelRatio;
        }
    }
}

void termite::animateSprites(Sprite** sprites, uint16_t numSprites, float dt)
{
    for (int i = 0; i < numSprites; i++) {
        Sprite* sprite = sprites[i];
        bool playReverse = sprite->playReverse;

        if (!bx::fequal(sprite->playSpeed, 0, 0.00001f)) {
            float t = sprite->animTm;
            t += dt;
            float progress = t * sprite->playSpeed;
            float frames = bx::ffloor(progress);
            float reminder = frames > 0 ? bx::fmod(progress, frames) : progress;
            t = reminder / sprite->playSpeed;

            // Progress sprite frame
            int curFrameIdx = sprite->curFrameIdx;
            int frameIdx = curFrameIdx;
            int iframes = int(frames);
            if (sprite->endCallback == nullptr) {
                frameIdx = iwrap(!playReverse ? (frameIdx + iframes) : (frameIdx - iframes),
                                 0, sprite->frames.getCount() - 1);
            } else {
                if (sprite->triggerEndCallback && iframes > 0) {
                    sprite->triggerEndCallback = false;
                    sprite->endCallback(sprite, frameIdx, sprite->endUserData);
                }

                int nextFrame = !playReverse ? (frameIdx + iframes) : (frameIdx - iframes);
                frameIdx = iclamp(nextFrame, 0, sprite->frames.getCount() - 1);

                if (frameIdx != nextFrame && sprite->playSpeed != 0) {
                    sprite->triggerEndCallback = true;  // Tigger callback on the next update
                }
            }

            // Check if we hit any callbacks
            const SpriteFrame& frame = sprite->frames[frameIdx];

            if (frame.frameCallback)
                frame.frameCallback(sprite, frameIdx, frame.frameCallbackUserData);

            // Update the frame index only if it's not modified inside any callbacks
            if (curFrameIdx == sprite->curFrameIdx)
                sprite->curFrameIdx = frameIdx;
            sprite->animTm = t;
        }
    }
}

void termite::invertSpriteAnim(Sprite* sprite)
{
    sprite->playReverse = !sprite->playReverse;
}

void termite::setSpriteAnimSpeed(Sprite* sprite, float speed)
{
    sprite->playSpeed = sprite->resumeSpeed = speed;
}

float termite::getSpriteAnimSpeed(Sprite* sprite)
{
    return sprite->resumeSpeed;
}

void termite::pauseSpriteAnim(Sprite* sprite)
{
    sprite->playSpeed = 0;
}

void termite::resumeSpriteAnim(Sprite* sprite)
{
    sprite->playSpeed = sprite->resumeSpeed;
}

void termite::stopSpriteAnim(Sprite* sprite)
{
    sprite->triggerEndCallback = false;
    sprite->curFrameIdx = 0;
    sprite->playSpeed = 0;
}

void termite::replaySpriteAnim(Sprite* sprite)
{
    sprite->triggerEndCallback = false;
    sprite->curFrameIdx = 0;
    sprite->playSpeed = sprite->resumeSpeed;
}

void termite::setSpriteFrameCallbackByTag(Sprite* sprite, const char* frameTag, SpriteFrameCallback callback, void* userData)
{
    assert(frameTag);
    size_t frameTagHash = tinystl::hash_string(frameTag, strlen(frameTag));
    for (int i = 0, c = sprite->frames.getCount(); i < c; i++) {
        if (sprite->frames[i].tagHash != frameTagHash)
            continue;
        SpriteFrame* frame = sprite->frames.itemPtr(i);
        frame->frameCallback = callback;
        frame->frameCallbackUserData = userData;
    }
}

void termite::setSpriteFrameCallbackByName(Sprite* sprite, const char* name, SpriteFrameCallback callback, void* userData)
{
    size_t nameHash = tinystl::hash_string(name, strlen(name));
    for (int i = 0, c = sprite->frames.getCount(); i < c; i++) {
        if (sprite->frames[i].nameHash != nameHash)
            continue;
        SpriteFrame* frame = sprite->frames.itemPtr(i);
        frame->frameCallback = callback;
        frame->frameCallbackUserData = userData;
    }
}

void termite::setSpriteFrameCallbackByIndex(Sprite* sprite, int frameIdx, SpriteFrameCallback callback, void* userData)
{
    assert(frameIdx < sprite->frames.getCount());
    SpriteFrame* frame = sprite->frames.itemPtr(frameIdx);
    frame->frameCallback = callback;
    frame->frameCallbackUserData = userData;
}

void termite::setSpriteFrameEndCallback(Sprite* sprite, SpriteFrameCallback callback, void* userData)
{
    sprite->endCallback = callback;
    sprite->endUserData = userData;
    sprite->triggerEndCallback = false;
}

void termite::setSpriteHalfSize(Sprite* sprite, const vec2_t& halfSize)
{
    sprite->halfSize = halfSize;
}

vec2_t termite::getSpriteHalfSize(Sprite* sprite)
{
    return sprite->halfSize;
}

void termite::setSpriteSizeMultiplier(Sprite* sprite, const vec2_t& sizeMultiplier)
{
    sprite->sizeMultiplier = sizeMultiplier;
}

void termite::gotoSpriteFrameIndex(Sprite* sprite, int frameIdx)
{
    assert(frameIdx < sprite->frames.getCount());
    sprite->curFrameIdx = frameIdx;
}

void termite::gotoSpriteFrameName(Sprite* sprite, const char* name)
{
    size_t nameHash = tinystl::hash_string(name, strlen(name));
    int curIdx = sprite->curFrameIdx;
    for (int i = 0, c = sprite->frames.getCount(); i < c;  i++) {
        int idx = (i + curIdx) % c;
        if (sprite->frames[idx].nameHash == nameHash) {
            sprite->curFrameIdx = idx;
            return;
        }
    }
}

void termite::gotoSpriteFrameTag(Sprite* sprite, const char* frameTag)
{
    assert(frameTag);
    size_t tagHash = tinystl::hash_string(frameTag, strlen(frameTag));
    int curIdx = sprite->curFrameIdx;
    for (int i = 0, c = sprite->frames.getCount(); i < c; i++) {
        int idx = (i + curIdx) % c;
        if (sprite->frames[idx].tagHash == tagHash) {
            sprite->curFrameIdx = idx;
            return;
        }
    }
}

int termite::getSpriteFrameIndex(Sprite* sprite)
{
    return sprite->curFrameIdx;
}

int termite::getSpriteFrameCount(Sprite* sprite)
{
    return sprite->frames.getCount();
}

void termite::setSpriteFrameIndex(Sprite* sprite, int index)
{
    assert(index < sprite->frames.getCount());
    sprite->curFrameIdx = index;
}

void termite::setSpriteFlip(Sprite* sprite, SpriteFlag::Bits flip)
{
    sprite->flip = flip;
}

SpriteFlip::Bits termite::getSpriteFlip(Sprite* sprite)
{
    return sprite->flip;
}

void termite::setSpritePosOffset(Sprite* sprite, const vec2_t posOffset)
{
    sprite->posOffset = posOffset;
}

vec2_t termite::getSpritePosOffset(Sprite* sprite)
{
    return sprite->posOffset;
}

void termite::setSpriteCurFrameTag(Sprite* sprite, const char* frameTag)
{
    SpriteFrame& frame = sprite->frames[sprite->curFrameIdx];
    frame.tagHash = tinystl::hash_string(frameTag, strlen(frameTag));
}

void termite::setSpriteOrder(Sprite* sprite, uint8_t order)
{
    sprite->order = order;
}

int termite::getSpriteOrder(Sprite* sprite)
{
    return sprite->order;
}

void termite::setSpritePivot(Sprite* sprite, const vec2_t pivot)
{
    for (int i = 0, c = sprite->frames.getCount(); i < c; i++)
        sprite->frames[i].pivot = pivot;
}

void termite::setSpriteTintColor(Sprite* sprite, color_t color)
{
    sprite->tint = color;
}

color_t termite::getSpriteTintColor(Sprite* sprite)
{
    return sprite->tint;
}

static rect_t getSpriteDrawRectFrame(Sprite* sprite, int index)
{
    const SpriteFrame& frame = sprite->frames[index];
    vec2_t halfSize = sprite->halfSize;
    float pixelRatio = frame.pixelRatio;
    if (halfSize.y <= 0)
        halfSize.y = halfSize.x / pixelRatio;
    else if (halfSize.x <= 0)
        halfSize.x = halfSize.y * pixelRatio;
    halfSize = halfSize * sprite->sizeMultiplier;
    vec2_t fullSize = halfSize * 2.0f;

    // calculate final pivot offset to make geometry
    SpriteFlag::Bits flipX = sprite->flip | frame.flags;
    SpriteFlag::Bits flipY = sprite->flip | frame.flags;

    vec2_t offset = frame.posOffset + sprite->posOffset - frame.pivot;
    if (flipX & SpriteFlip::FlipX)
        offset.x = -offset.x;
    if (flipY & SpriteFlip::FlipY)
        offset.y = -offset.y;

    halfSize = halfSize * frame.sizeOffset;
    offset = offset * fullSize;

    return rectv(offset - halfSize, halfSize + offset);
}

rect_t termite::getSpriteDrawRect(Sprite* sprite)
{
    return getSpriteDrawRectFrame(sprite, sprite->curFrameIdx);
}

void termite::getSpriteRealRect(Sprite* sprite, vec2_t* pHalfSize, vec2_t* pCenter)
{
    const SpriteFrame& frame = sprite->getCurFrame();
    SpriteFlip::Bits flip = sprite->flip & frame.flags;
    vec2_t halfSize = sprite->halfSize;
    float pixelRatio = frame.pixelRatio;
    if (halfSize.y <= 0)
        halfSize.y = halfSize.x / pixelRatio;
    else if (halfSize.x <= 0)
        halfSize.x = halfSize.y * pixelRatio;
    vec2_t pivot = frame.pivot;
    if (flip & SpriteFlip::FlipX)
        pivot.x = -pivot.x;
    if (flip & SpriteFlip::FlipY)
        pivot.y = -pivot.y;

    *pHalfSize = halfSize;
    *pCenter = pivot * halfSize * 2.0f;
}

vec2_t termite::getSpriteImageSize(Sprite* sprite)
{
    return sprite->getCurFrame().sourceSize;
}

rect_t termite::getSpriteTexelRect(Sprite* sprite)
{
    return sprite->getCurFrame().frame;
}

ProgramHandle termite::getSpriteColorAddProgram()
{
    return g_spriteSys->spriteAddProg;
}

void termite::setSpriteUserData(Sprite* sprite, void* userData)
{
    sprite->userData = userData;
}

void* termite::getSpriteUserData(Sprite* sprite)
{
    return sprite->userData;
}

void termite::getSpriteFrameDrawData(Sprite* sprite, int frameIdx, rect_t* drawRect, rect_t* textureRect, 
                                     ResourceHandle* textureHandle)
{
    *drawRect = getSpriteDrawRectFrame(sprite, frameIdx);
    *textureRect = sprite->frames[frameIdx].frame;
    *textureHandle = sprite->frames[frameIdx].texHandle;
}

void termite::convertSpritePhysicsVerts(vec2_t* ptsOut, const vec2_t* ptsIn, int numPts, Sprite* sprite)
{
    vec2_t halfSize;
    vec2_t center;
    vec2_t imgSize = getSpriteImageSize(sprite);
    getSpriteRealRect(sprite, &halfSize, &center);
    SpriteFlip::Bits flip = sprite->flip;

    for (int i = 0; i < numPts; i++) {
        vec2_t pt = ptsIn[i];
        if (flip & SpriteFlip::FlipX)
            pt.x = -pt.x;
        if (flip & SpriteFlip::FlipY)
            pt.y = -pt.y;

        pt = vec2f(pt.x/imgSize.x, pt.y/imgSize.y);
        ptsOut[i] = pt * halfSize * 2.0f - center;
    }
}

static void drawSpritesWireframe(uint8_t viewId, Sprite** sprites, uint16_t numSprites, const mtx3x3_t* mats)
{
    if (numSprites <= 0)
        return;
    assert(sprites);

    GfxDriverApi* gDriver = g_spriteSys->driver;

    // Evaluate final vertices and indexes
    int numVerts = 0, numIndices = 0;
    for (int si = 0; si < numSprites; si++) {
        Sprite* sprite = sprites[si];
        const SpriteFrame& frame = sprite->frames[sprite->curFrameIdx];

        if (frame.ssHandle.isValid()) {
            assert(frame.meshId != -1);
            SpriteSheet* sheet = getResourcePtr<SpriteSheet>(frame.ssHandle);
            const SpriteMesh& mesh = sheet->meshes[frame.meshId];
            numVerts += mesh.numVerts;
            numIndices += mesh.numTris*6;
        } else {
            numVerts += 4;
            numIndices += 12;
        }
    }

    TransientVertexBuffer tvb;
    TransientIndexBuffer tib;

    if (!gDriver->allocTransientBuffers(&tvb, SpriteVertex::Decl, numVerts, &tib, numIndices))
        return;
    SpriteVertex* verts = (SpriteVertex*)tvb.data;
    uint16_t* indices = (uint16_t*)tib.data;
    bx::AllocatorI* tmpAlloc = getTempAlloc();

    // Sort sprites by order->texture->id and batch them
    SortedSprite* sortedSprites = (SortedSprite*)BX_ALLOC(tmpAlloc, sizeof(SortedSprite)*numSprites);

    for (int i = 0; i < numSprites; i++) {
        const SpriteFrame& frame = sprites[i]->getCurFrame();
        sortedSprites[i].index = i;
        sortedSprites[i].sprite = sprites[i];
        sortedSprites[i].key = MAKE_SPRITE_KEY(sprites[i]->order, frame.texHandle.value, sprites[i]->id);
    }

    if (numSprites > 1) {
        std::sort(sortedSprites, sortedSprites + numSprites, [](const SortedSprite& a, const SortedSprite&b)->bool {
            return a.key < b.key;
        });
    }

    // Fill draw data (geometry)
    mtx3x3_t preMat, finalMat;
    int indexIdx = 0;
    int vertexIdx = 0;

    // Batching
    bx::Array<SpriteDrawBatch> batches;
    batches.create(32, 64, tmpAlloc);
    uint32_t prevKey = UINT32_MAX;
    SpriteDrawBatch* curBatch = nullptr;

    for (int si = 0; si < numSprites; si++) {
        const SortedSprite ss = sortedSprites[si];
        Sprite* sprite = sprites[ss.index];
        const SpriteFrame& frame = sprite->frames[sprite->curFrameIdx];

        vec2_t halfSize = sprite->halfSize;
        float pixelRatio = frame.pixelRatio;
        SpriteFlag::Bits flipX = sprite->flip | frame.flags;
        SpriteFlag::Bits flipY = sprite->flip | frame.flags;
        float scalex, scaley;
        if (halfSize.y <= 0) {
            scalex = halfSize.x*2.0f;
            scaley = scalex / pixelRatio;
        } else if (halfSize.x <= 0) {
            scaley = halfSize.y*2.0f;
            scalex = scaley*pixelRatio;
        } else {
            scalex = halfSize.x*2.0f;
            scaley = halfSize.y*2.0f;
        }

        vec2_t pos = sprite->posOffset - frame.pivot;
        vec2_t scale = vec2f(scalex, scaley) * sprite->sizeMultiplier;

        if (flipX & SpriteFlip::FlipX)
            scale.x = -scale.x;
        if (flipY & SpriteFlip::FlipY)
            scale.y = -scale.y;

        // Transform (offset x sprite's own matrix)
        // PreMat = TranslateMat * ScaleMat
        preMat.m11 = scale.x;       preMat.m12 = 0;             preMat.m13 = 0;
        preMat.m21 = 0;             preMat.m22 = scale.y;       preMat.m23 = 0;
        preMat.m31 = scale.x*pos.x; preMat.m32 = scale.y*pos.y; preMat.m33 = 1.0f;
        bx::mtx3x3Mul(finalMat.f, preMat.f, mats[ss.index].f);

        vec3_t transform1 = vec3f(finalMat.m11, finalMat.m12, finalMat.m21);
        vec3_t transform2 = vec3f(finalMat.m22, finalMat.m31, finalMat.m32);
        uint32_t color = ss.sprite->tint.n;

        // Batch
        uint32_t batchKey = SPRITE_KEY_GET_BATCH(ss.key);
        if (batchKey != prevKey) {
            curBatch = batches.push();
            curBatch->texHandle = frame.texHandle;
            curBatch->numVerts = 0;
            curBatch->numIndices = 0;
            curBatch->startIdx = indexIdx;
            prevKey = batchKey;
        }

        // Fill geometry data
        if (frame.ssHandle.isValid()) {
            SpriteSheet* sheet = getResourcePtr<SpriteSheet>(frame.ssHandle);
            assert(frame.meshId != -1);
            const SpriteMesh& mesh = sheet->meshes[frame.meshId];

            for (int i = 0, c = mesh.numVerts; i < c; i++) {
                int k = i + vertexIdx;
                verts[k].pos = mesh.verts[i];
                verts[k].transform1 = transform1;
                verts[k].transform2 = transform2;
                verts[k].coords = mesh.uvs[i];
                verts[k].color = color;
            }

            for (int i = 0, c = mesh.numTris; i < c; i++) {
                int k = i*3;
                indices[k*2 + indexIdx] = mesh.tris[k] + vertexIdx;
                indices[k*2 + indexIdx + 1] = mesh.tris[k + 1] + vertexIdx;
                indices[k*2 + indexIdx + 2] = mesh.tris[k + 1] + vertexIdx;
                indices[k*2 + indexIdx + 3] = mesh.tris[k + 2] + vertexIdx;
                indices[k*2 + indexIdx + 4] = mesh.tris[k + 2] + vertexIdx;
                indices[k*2 + indexIdx + 5] = mesh.tris[k] + vertexIdx;
            }

            int numIndices = mesh.numTris*6;
            indexIdx += numIndices;
            vertexIdx += mesh.numVerts;
            curBatch->numVerts += mesh.numVerts;
            curBatch->numIndices += uint16_t(numIndices);
        } else {
            // Make a normalized quad
            SpriteVertex& v0 = verts[vertexIdx];
            SpriteVertex& v1 = verts[vertexIdx + 1];
            SpriteVertex& v2 = verts[vertexIdx + 2];
            SpriteVertex& v3 = verts[vertexIdx + 3];
            v0.pos = vec2f(-0.5f, 0.5f);
            v0.coords = vec2f(0, 0);

            v1.pos = vec2f(0.5f, 0.5f);
            v1.coords = vec2f(1.0f, 0);

            v2.pos = vec2f(-0.5f, -0.5f);
            v2.coords = vec2f(0, 1.0f);

            v3.pos = vec2f(0.5f, -0.5f);
            v3.coords = vec2f(1.0f, 1.0f);

            v0.transform1 = v1.transform1 = v2.transform1 = v3.transform1 = transform1;
            v0.transform2 = v1.transform2 = v2.transform2 = v3.transform2 = transform2;
            v0.color = v1.color = v2.color = v3.color = color;

            indices[indexIdx] = vertexIdx;
            indices[indexIdx + 1] = vertexIdx + 1;
            indices[indexIdx + 2] = vertexIdx + 1;
            indices[indexIdx + 3] = vertexIdx + 2;
            indices[indexIdx + 4] = vertexIdx + 2;
            indices[indexIdx + 5] = vertexIdx;
            indices[indexIdx + 6] = vertexIdx + 1;
            indices[indexIdx + 7] = vertexIdx + 3;
            indices[indexIdx + 8] = vertexIdx + 3;
            indices[indexIdx + 9] = vertexIdx + 2;
            indices[indexIdx + 10] = vertexIdx + 2;
            indices[indexIdx + 11] = vertexIdx + 1;

            vertexIdx += 4;
            indexIdx += 12;
            curBatch->numVerts += 4;
            curBatch->numIndices += 12;
        }
    }

    // Draw
    GfxState::Bits state = gfxStateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite | GfxState::PrimitiveLines;
    ProgramHandle prog = g_spriteSys->spriteProg;
    for (int i = 0, c = batches.getCount(); i < c; i++) {
        const SpriteDrawBatch batch = batches[i];
        gDriver->setState(state, 0);
        gDriver->setTransientVertexBufferI(0, &tvb, 0, batch.numVerts);
        gDriver->setTransientIndexBufferI(&tib, batch.startIdx, batch.numIndices);
        gDriver->setTexture(0, g_spriteSys->u_texture, getWhiteTexture1x1(), TextureFlag::FromTexture);
        gDriver->submit(viewId, prog, 0, false);
    }

    batches.destroy();
}


void termite::drawSprites(uint8_t viewId, Sprite** sprites, uint16_t numSprites, const mtx3x3_t* mats,
                           ProgramHandle progOverride /*= ProgramHandle()*/, SetSpriteStateCallback stateCallback /*= nullptr*/,
                           void* stateUserData, const color_t* colors)
{
#if termite_DEV
    if (g_spriteSys->renderMode == SpriteRenderMode::Wireframe)
        return drawSpritesWireframe(viewId, sprites, numSprites, mats);
#endif

    if (numSprites <= 0)
        return;
    assert(sprites);

    GfxDriverApi* gDriver = g_spriteSys->driver;

    // Evaluate final vertices and indexes
    int numVerts = 0, numIndices = 0;
    for (int si = 0; si < numSprites; si++) {
        Sprite* sprite = sprites[si];
        const SpriteFrame& frame = sprite->frames[sprite->curFrameIdx];

        if (frame.ssHandle.isValid()) {
            assert(frame.meshId != -1);
            SpriteSheet* sheet = getResourcePtr<SpriteSheet>(frame.ssHandle);
            const SpriteMesh& mesh = sheet->meshes[frame.meshId];
            numVerts += mesh.numVerts;
            numIndices += mesh.numTris*3;
        } else {
            numVerts += 4;
            numIndices += 6;
        }
    }

    TransientVertexBuffer tvb;
    TransientIndexBuffer tib;

    if (!gDriver->allocTransientBuffers(&tvb, SpriteVertex::Decl, numVerts, &tib, numIndices))
        return;
    SpriteVertex* verts = (SpriteVertex*)tvb.data;
    uint16_t* indices = (uint16_t*)tib.data;
    bx::AllocatorI* tmpAlloc = getTempAlloc();

    // Sort sprites by order->texture->id and batch them
    SortedSprite* sortedSprites = (SortedSprite*)BX_ALLOC(tmpAlloc, sizeof(SortedSprite)*numSprites);

    for (int i = 0; i < numSprites; i++) {
        const SpriteFrame& frame = sprites[i]->getCurFrame();
        sortedSprites[i].index = i;
        sortedSprites[i].sprite = sprites[i];
        sortedSprites[i].key = MAKE_SPRITE_KEY(sprites[i]->order, frame.texHandle.value, sprites[i]->id);
    }

    if (numSprites > 1) {
        std::sort(sortedSprites, sortedSprites + numSprites, [](const SortedSprite& a, const SortedSprite&b)->bool {
            return a.key < b.key;
        });
    }

    // Fill draw data (geometry)
    mtx3x3_t preMat, finalMat;
    int indexIdx = 0;
    int vertexIdx = 0;

    // Batching
    bx::Array<SpriteDrawBatch> batches;
    batches.create(32, 64, tmpAlloc);
    uint32_t prevKey = UINT32_MAX;
    SpriteDrawBatch* curBatch = nullptr;

    for (int si = 0; si < numSprites; si++) {
        const SortedSprite ss = sortedSprites[si];
        Sprite* sprite = sprites[ss.index];
        const SpriteFrame& frame = sprite->frames[sprite->curFrameIdx];

        vec2_t halfSize = sprite->halfSize;
        float pixelRatio = frame.pixelRatio;
        SpriteFlag::Bits flipX = sprite->flip | frame.flags;
        SpriteFlag::Bits flipY = sprite->flip | frame.flags;
        float scalex, scaley;
        if (halfSize.y <= 0) {
            scalex = halfSize.x*2.0f;
            scaley = scalex / pixelRatio;
        } else if (halfSize.x <= 0) {
            scaley = halfSize.y*2.0f;
            scalex = scaley*pixelRatio;
        } else {
            scalex = halfSize.x*2.0f;
            scaley = halfSize.y*2.0f;
        }

        vec2_t pos = sprite->posOffset - frame.pivot;
        vec2_t scale = vec2f(scalex, scaley) * sprite->sizeMultiplier;

        if (flipX & SpriteFlip::FlipX)
            scale.x = -scale.x;
        if (flipY & SpriteFlip::FlipY)
            scale.y = -scale.y;

        // Transform (offset x sprite's own matrix)
        // PreMat = TranslateMat * ScaleMat
        preMat.m11 = scale.x;       preMat.m12 = 0;             preMat.m13 = 0;
        preMat.m21 = 0;             preMat.m22 = scale.y;       preMat.m23 = 0;
        preMat.m31 = scale.x*pos.x; preMat.m32 = scale.y*pos.y; preMat.m33 = 1.0f;
        bx::mtx3x3Mul(finalMat.f, preMat.f, mats[ss.index].f);

        vec3_t transform1 = vec3f(finalMat.m11, finalMat.m12, finalMat.m21);
        vec3_t transform2 = vec3f(finalMat.m22, finalMat.m31, finalMat.m32);
        uint32_t color;
        if (!colors)    color = ss.sprite->tint.n;
        else            color = colors[ss.index].n;

        // Batch
        uint32_t batchKey = SPRITE_KEY_GET_BATCH(ss.key);
        if (batchKey != prevKey) {
            curBatch = batches.push();
            curBatch->texHandle = frame.texHandle;
            curBatch->numVerts = 0;
            curBatch->numIndices = 0;
            curBatch->startIdx = indexIdx;
            prevKey = batchKey;
        }
        
        // Fill geometry data
        if (frame.ssHandle.isValid()) {
            SpriteSheet* sheet = getResourcePtr<SpriteSheet>(frame.ssHandle);
            assert(frame.meshId != -1);
            const SpriteMesh& mesh = sheet->meshes[frame.meshId];

            for (int i = 0, c = mesh.numVerts; i < c; i++) {
                int k = i + vertexIdx;
                verts[k].pos = mesh.verts[i];
                verts[k].transform1 = transform1;
                verts[k].transform2 = transform2;
                verts[k].coords = mesh.uvs[i];
                verts[k].color = color;
            }

            for (int i = 0, c = mesh.numTris; i < c; i++) {
                int k = i*3;
                indices[k + indexIdx] = mesh.tris[k] + vertexIdx;
                indices[k + indexIdx + 1] = mesh.tris[k + 1] + vertexIdx;
                indices[k + indexIdx + 2] = mesh.tris[k + 2] + vertexIdx;
            }

            int numIndices = mesh.numTris*3;
            indexIdx += numIndices;
            vertexIdx += mesh.numVerts;
            curBatch->numVerts += mesh.numVerts;
            curBatch->numIndices += uint16_t(numIndices);
        } else {
            // Make a normalized quad
            SpriteVertex& v0 = verts[vertexIdx];
            SpriteVertex& v1 = verts[vertexIdx + 1];
            SpriteVertex& v2 = verts[vertexIdx + 2];
            SpriteVertex& v3 = verts[vertexIdx + 3];
            v0.pos = vec2f(-0.5f, 0.5f);
            v0.coords = vec2f(0, 0);

            v1.pos = vec2f(0.5f, 0.5f);
            v1.coords = vec2f(1.0f, 0);

            v2.pos = vec2f(-0.5f, -0.5f);
            v2.coords = vec2f(0, 1.0f);

            v3.pos = vec2f(0.5f, -0.5f);
            v3.coords = vec2f(1.0f, 1.0f);

            v0.transform1 = v1.transform1 = v2.transform1 = v3.transform1 = transform1;
            v0.transform2 = v1.transform2 = v2.transform2 = v3.transform2 = transform2;
            v0.color = v1.color = v2.color = v3.color = color;

            indices[indexIdx] = vertexIdx;
            indices[indexIdx + 1] = vertexIdx + 1;
            indices[indexIdx + 2] = vertexIdx + 2;
            indices[indexIdx + 3] = vertexIdx + 2;
            indices[indexIdx + 4] = vertexIdx + 1;
            indices[indexIdx + 5] = vertexIdx + 3;

            vertexIdx += 4;
            indexIdx += 6;
            curBatch->numVerts += 4;
            curBatch->numIndices += 6;
        }
    }

    // Draw
    GfxState::Bits state = gfxStateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite;
    ProgramHandle prog = !progOverride.isValid() ? g_spriteSys->spriteProg : progOverride;
    for (int i = 0, c = batches.getCount(); i < c; i++) {
        const SpriteDrawBatch batch = batches[i];
        gDriver->setState(state, 0);
        gDriver->setTransientVertexBufferI(0, &tvb, 0, batch.numVerts);
        gDriver->setTransientIndexBufferI(&tib, batch.startIdx, batch.numIndices);
        if (batch.texHandle.isValid())
            gDriver->setTexture(0, g_spriteSys->u_texture, getResourcePtr<Texture>(batch.texHandle)->handle, TextureFlag::FromTexture);

        if (stateCallback) {
            stateCallback(gDriver, stateUserData);
        }
        gDriver->submit(viewId, prog, 0, false);
    }

    batches.destroy();
}

void termite::registerSpriteSheetToResourceLib()
{
    ResourceTypeHandle handle;
    handle = registerResourceType("spritesheet", &g_spriteSys->loader, sizeof(LoadSpriteSheetParams), 
                                  uintptr_t(g_spriteSys->failSheet), uintptr_t(g_spriteSys->asyncSheet));
    assert(handle.isValid());
}

rect_t termite::getSpriteSheetTextureFrame(ResourceHandle spritesheet, int index)
{
    SpriteSheet* ss = getResourcePtr<SpriteSheet>(spritesheet);
    assert(index < ss->numFrames);
    return ss->frames[index].frame;
}

rect_t termite::getSpriteSheetTextureFrame(ResourceHandle spritesheet, const char* name)
{
    SpriteSheet* ss = getResourcePtr<SpriteSheet>(spritesheet);
    size_t filenameHash = tinystl::hash_string(name, strlen(name));
    for (int i = 0, c = ss->numFrames; i < c; i++) {
        if (ss->frames[i].filenameHash != filenameHash)
            continue;
        
        return ss->frames[i].frame;
    }
    return rectf(0, 0, 1.0f, 1.0f);
}

ResourceHandle termite::getSpriteSheetTexture(ResourceHandle spritesheet)
{
    SpriteSheet* ss = getResourcePtr<SpriteSheet>(spritesheet);
    return ss->texHandle;
}

vec2_t termite::getSpriteSheetFrameSize(ResourceHandle spritesheet, int index)
{
    SpriteSheet* ss = getResourcePtr<SpriteSheet>(spritesheet);
    assert(index < ss->numFrames);
    return ss->frames[index].sourceSize;
}

vec2_t termite::getSpriteSheetFrameSize(ResourceHandle spritesheet, const char* name)
{
    SpriteSheet* ss = getResourcePtr<SpriteSheet>(spritesheet);
    size_t filenameHash = tinystl::hash_string(name, strlen(name));
    for (int i = 0, c = ss->numFrames; i < c; i++) {
        if (ss->frames[i].filenameHash != filenameHash)
            continue;

        return ss->frames[i].sourceSize;
    }
    return vec2f(0, 0);

}
