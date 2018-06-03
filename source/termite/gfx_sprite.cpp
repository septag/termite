#include "pch.h"
#include "gfx_sprite.h"
#include "gfx_texture.h"
#include "tmath.h"
#include "internal.h"
#include "gfx_driver.h"
#include "rapidjson.h"

#include "bx/readerwriter.h"
#include "bxx/path.h"
#include "bxx/array.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/linear_allocator.h"

#include TEE_MAKE_SHADER_PATH(shaders_h, sprite.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, sprite.fso)

#define MAKE_SPRITE_KEY(_Order, _Texture, _Id) \
    (uint64_t(_Order & kSpriteKeyOrderMask) << kSpriteKeyOrderShift) | (uint64_t(_Texture & kSpriteKeyTextureMask) << kSpriteKeyTextureShift) | uint64_t(_Id & kSpriteKeyIdMask)
#define SPRITE_KEY_GET_BATCH(_Key) uint32_t((_Key >> kSpriteKeyIdBits) & kSpriteKeyIdMask) & kSpriteKeyTextureMask

namespace tee {
    static const uint64_t kSpriteKeyOrderBits = 8;
    static const uint64_t kSpriteKeyOrderMask = (1 << kSpriteKeyOrderBits) - 1;
    static const uint64_t kSpriteKeyTextureBits = 16;
    static const uint64_t kSpriteKeyTextureMask = (1 << kSpriteKeyTextureBits) - 1;
    static const uint64_t kSpriteKeyIdBits = 32;
    static const uint64_t kSpriteKeyIdMask = (1llu << kSpriteKeyIdBits) - 1;

    static const uint8_t kSpriteKeyOrderShift = kSpriteKeyTextureBits + kSpriteKeyIdBits;
    static const uint8_t kSpriteKeyTextureShift = kSpriteKeyIdBits;

    struct SpriteVertex
    {
        vec2_t pos;
        vec3_t transform1;
        vec3_t transform2;
        vec2_t coords;
        uint32_t color;
        uint32_t colorAdd;

        static void init()
        {
            gfx::beginDecl(&Decl);
            gfx::addAttrib(&Decl, VertexAttrib::Position, 2, VertexAttribType::Float);        // pos
            gfx::addAttrib(&Decl, VertexAttrib::TexCoord0, 3, VertexAttribType::Float);       // transform mat (part 1)
            gfx::addAttrib(&Decl, VertexAttrib::TexCoord1, 3, VertexAttribType::Float);       // transform mat (part 2)
            gfx::addAttrib(&Decl, VertexAttrib::TexCoord2, 2, VertexAttribType::Float);       // texture coords + glow
            gfx::addAttrib(&Decl, VertexAttrib::Color0, 4, VertexAttribType::Uint8, true);    // color
            gfx::addAttrib(&Decl, VertexAttrib::Color1, 4, VertexAttribType::Uint8, true);    // colorAdd
            gfx::endDecl(&Decl);
        }

        static VertexDecl Decl;
    };
    VertexDecl SpriteVertex::Decl;

    class SpriteSheetLoader : public AssetLibCallbacksI
    {
    public:
        bool loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
        void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
        void onReload(AssetHandle handle, bx::AllocatorI* alloc) override;
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
        AssetHandle texHandle;
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
#ifdef _DEBUG
        bx::String32 name;
#endif
        AssetHandle texHandle;   // Handle to spritesheet/texture resource
        AssetHandle ssHandle;    // For spritesheets we have spritesheet handle

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

        sprite::FrameCallback frameCallback;
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

    struct SpriteCache
    {
        typedef bx::List<SpriteCache*>::Node LNode;

        bx::AllocatorI* alloc;
        VertexBufferHandle vb;
        IndexBufferHandle ib;

        // backup buffers to restore the gpu buffers
        void* verts;
        int vbSize;     // bytes
        void* indices;
        int ibSize;     // bytes

        SpriteDrawBatch* batches;       // Draw batches 
        int numBatches;

        rect_t bounds;

        LNode lnode;

        SpriteCache(bx::AllocatorI* _alloc) :
            alloc(_alloc),
            verts(nullptr),
            vbSize(0),
            indices(nullptr),
            ibSize(0),
            batches(nullptr),
            numBatches(0),
            lnode(this)
        {
        }
    };

    struct Sprite
    {
        typedef bx::List<Sprite*>::Node LNode;

        uint32_t id;
        bx::AllocatorI* alloc;
        vec2_t halfSize;
        vec2_t scale;
        vec2_t posOffset;
        bx::Array<SpriteFrame> frames;
        int curFrameIdx;

        float animTm;       // Animation timer
        bool playReverse;
        float playSpeed;
        float resumeSpeed;
        ucolor_t color;
        ucolor_t colorAdd;
        uint8_t order;

        SpriteFlag::Bits flip;
        LNode lnode;

        sprite::FrameCallback endCallback;
        void* endUserData;
        void* userData;
        bool triggerEndCallback;

        const char* dbgFile;

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
            color = ucolor(0xffffffff);
            colorAdd = ucolor(0);
            posOffset = vec2(0, 0);
            scale = vec2(1.0f, 1.0f);
            userData = nullptr;
        }

        inline const SpriteFrame& getCurFrame() const
        {
            return frames[curFrameIdx];
        }
    };

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
        float scale;
        AssetHandle texHandle;
        uint8_t padding[2];

        SpriteSheet() :
            buff(nullptr),
            frames(nullptr),
            meshes(nullptr),
            numFrames(0),
            scale(1.0f)
        {
        }
    };

    struct SpriteMgr
    {
        GfxDriver* driver;
        bx::AllocatorI* alloc;
        ProgramHandle spriteProg;
        UniformHandle u_texture;
        SpriteSheetLoader loader;
        SpriteSheet* failSheet;
        SpriteSheet* asyncSheet;
        bx::List<Sprite*> spriteList;       // keep a list of sprites for proper shutdown and sheet reloading
        bx::List<SpriteCache*> spriteCacheList;
        SpriteRenderMode::Enum renderMode;

        SpriteMgr(bx::AllocatorI* _alloc) : 
            driver(nullptr),
            alloc(_alloc),
            failSheet(nullptr),
            asyncSheet(nullptr),
            renderMode(SpriteRenderMode::Normal)
        {
        }
    };

    static SpriteMgr* gSpriteMgr = nullptr;

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
        ivec2_t ivert;
        if (count > 0) {
            verts = (vec2_t*)BX_ALLOC(alloc, sizeof(vec2_t)*count);
            for (int i = 0; i < count; i++) {
                json::getIntArray<_T>(jnode[i], ivert.n, 2);
                verts[i] = vec2(float(ivert.x), float(ivert.y));
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
                json::getUInt16Array<_T>(jnode[i], tris + 3*i, 3);
            }
        }
        *numTries = count;
        return tris;
    }

    // https://en.wikipedia.org/wiki/Shoelace_formula
    static void reorderSpriteMeshTriangles(SpriteMesh* mesh)
    {
        for (int i = 0, c = mesh->numTris; i < c; i++) {
            int tidx = i*3;

            const vec2_t& p1 = mesh->verts[mesh->tris[tidx]];
            const vec2_t& p2 = mesh->verts[mesh->tris[tidx + 1]];
            const vec2_t& p3 = mesh->verts[mesh->tris[tidx + 2]];

            float area = 0.5f*(p1.x*p2.y + p2.x*p3.y + p3.x*p1.y - p2.x*p1.y - p3.x*p2.y - p1.x*p3.y);
            //float s = (p2.x - p1.x)*(p2.y + p1.y) + (p3.x - p2.x)*(p3.y + p2.y) + (p1.x - p3.x)*(p1.y + p3.y);
            // it's CCW, turn it to CW
            if (area > 0) {
                bx::xchg(mesh->tris[tidx], mesh->tris[tidx + 2]);
            }
        }
    }

    bool SpriteSheetLoader::loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, 
                                    bx::AllocatorI* alloc)
    {

        const LoadSpriteSheetParams* ssParams = (const LoadSpriteSheetParams*)params.userParams;

        bx::AllocatorI* tmpAlloc = getTempAlloc();
        char* jsonStr = (char*)BX_ALLOC(tmpAlloc, mem->size + 1);
        if (!jsonStr) {
            TEE_ERROR("Out of Memory");
            return false;
        }
        memcpy(jsonStr, mem->data, mem->size);
        jsonStr[mem->size] = 0;

        json::StackAllocator jalloc(tmpAlloc);
        json::StackPoolAllocator jpool(4096, &jalloc);
        json::StackDocument jdoc(&jpool, 1024, &jalloc);

        if (jdoc.ParseInsitu(jsonStr).HasParseError()) {
            TEE_ERROR("Parse Json Error: %s (Pos: %d)", GetParseError_En(jdoc.GetParseError()), jdoc.GetErrorOffset());                 
            return false;
        }
        BX_FREE(tmpAlloc, jsonStr);

        if (!jdoc.HasMember("frames") || !jdoc.HasMember("meta")) {
            TEE_ERROR("SpriteSheet Json is Invalid");
            return false;
        }
        const json::svalue_t& jframes = jdoc["frames"];
        const json::svalue_t& jmeta = jdoc["meta"];

        BX_ASSERT(jframes.IsArray());
        int numFrames = jframes.Size();
        if (numFrames == 0)
            return false;

        // evaluate total vertices, triangles and uvs
        int numTotalVerts = 0, numTotalTris = 0, numTotalUvs = 0;
        for (int i = 0; i < numFrames; i++) {
            const json::svalue_t& jframe = jframes[i];
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
        uint8_t* buff = (uint8_t*)BX_ALLOC(alloc ? alloc : gSpriteMgr->alloc, totalSz);
        if (!buff)
            return false;
        bx::LinearAllocator lalloc(buff, totalSz);
        SpriteSheet* ss = BX_NEW(&lalloc, SpriteSheet);
        ss->buff = buff;
        ss->frames = (SpriteSheetFrame*)BX_ALLOC(&lalloc, numFrames*sizeof(SpriteSheetFrame));
        ss->numFrames = numFrames;
        ss->meshes = (SpriteMesh*)BX_ALLOC(&lalloc, numFrames*sizeof(SpriteMesh));

        if (jmeta.HasMember("scale"))
            ss->scale = bx::toFloat(jmeta["scale"].GetString());

        // image width/height
        const json::svalue_t& jsize = jmeta["size"];
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
        ss->texHandle = asset::load("texture", texFilepath.cstr(), &texParams, params.flags, alloc ? alloc : nullptr);

        for (int i = 0; i < numFrames; i++) {
            SpriteSheetFrame& frame = ss->frames[i];
            const json::svalue_t& jframe = jframes[i];
            const char* filename = jframe["filename"].GetString();
            frame.filenameHash = tinystl::hash_string(filename, strlen(filename));
            bool rotated = jframe["rotated"].GetBool();

            const json::svalue_t& jframeFrame = jframe["frame"];
            float frameWidth = float(jframeFrame["w"].GetInt());
            float frameHeight = float(jframeFrame["h"].GetInt());
            if (rotated)
                std::swap<float>(frameWidth, frameHeight);

            frame.frame = rectwh(float(jframeFrame["x"].GetInt()) / imgWidth,
                                  float(jframeFrame["y"].GetInt()) / imgHeight,
                                  frameWidth / imgWidth,
                                  frameHeight / imgHeight);

            const json::svalue_t& jsourceSize = jframe["sourceSize"];
            frame.sourceSize = vec2(float(jsourceSize["w"].GetInt()),
                                     float(jsourceSize["h"].GetInt()));

            // Normalize pos/size offsets (0~1)
            // Rotate offset can only be 90 degrees
            const json::svalue_t& jssFrame = jframe["spriteSourceSize"];
            float srcx = float(jssFrame["x"].GetInt());
            float srcy = float(jssFrame["y"].GetInt());
            float srcw = float(jssFrame["w"].GetInt());
            float srch = float(jssFrame["h"].GetInt());

            frame.sizeOffset = vec2(srcw / frame.sourceSize.x, srch / frame.sourceSize.y);

            if (!rotated) {
                frame.rotOffset = 0;
            } else {
                std::swap<float>(srcw, srch);
                frame.rotOffset = -90.0f;
            }
            frame.pixelRatio = frame.sourceSize.x / frame.sourceSize.y;

            const json::svalue_t& jpivot = jframe["pivot"];
            const json::svalue_t& jpivotX = jpivot["x"];
            const json::svalue_t& jpivotY = jpivot["y"];
            float pivotx = jpivotX.IsFloat() ?  jpivotX.GetFloat() : float(jpivotX.GetInt());
            float pivoty = jpivotY.IsFloat() ?  jpivotY.GetFloat() : float(jpivotY.GetInt());
            frame.pivot = vec2(pivotx - 0.5f, -pivoty + 0.5f);     // convert to our coordinates

            // PosOffset is the center of the sprite in -0.5~0.5 coords
            frame.posOffset = vec2( (srcx + srcw*0.5f)/frame.sourceSize.x - 0.5f, 
                                    -(srcy + srch*0.5f)/frame.sourceSize.y + 0.5f);

            // Mesh
            if (jframe.HasMember("vertices")) {
                SpriteMesh& mesh = ss->meshes[i];
                const json::svalue_t& jverts = jframe["vertices"];
                mesh.verts = loadSpriteVerts(jverts, &mesh.numVerts, &lalloc);
            
                // Convert vertices to normalized (-0.5~0.5) of the sprite frame
                for (int i = 0; i < mesh.numVerts; i++) {
                    vec2_t pixelPos = mesh.verts[i];
                    mesh.verts[i] = vec2(pixelPos.x/frame.sourceSize.x - 0.5f, -pixelPos.y/frame.sourceSize.y + 0.5f);
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

                reorderSpriteMeshTriangles(&mesh);
            } else {
                rect_t texcoords = frame.frame;
                vec2_t topLeft = vec2(srcx/frame.sourceSize.x - 0.5f,
                                       -srcy/frame.sourceSize.y + 0.5f);
                vec2_t rightBottom = vec2((srcx + srcw)/frame.sourceSize.x - 0.5f,
                                           -(srcy + srch)/frame.sourceSize.y + 0.5f);
                SpriteMesh& mesh = ss->meshes[i];
                mesh.numVerts = 4;
                mesh.verts = (vec2_t*)BX_ALLOC(&lalloc, sizeof(vec2_t)*mesh.numVerts);
                mesh.uvs = (vec2_t*)BX_ALLOC(&lalloc, sizeof(vec2_t)*mesh.numVerts);
                mesh.numTris = 2;
                mesh.tris = (uint16_t*)BX_ALLOC(&lalloc, sizeof(uint16_t)*mesh.numTris*3);

                mesh.verts[0] = topLeft;
                mesh.uvs[0] = texcoords.vmin;

                mesh.verts[1] = vec2(rightBottom.x, topLeft.y);
                mesh.uvs[1] = vec2(texcoords.xmax, texcoords.ymin);

                mesh.verts[2] = vec2(topLeft.x, rightBottom.y);
                mesh.uvs[2] = vec2(texcoords.xmin, texcoords.ymax);

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
        BX_ASSERT(gSpriteMgr);
        BX_ASSERT(obj);

        SpriteSheet* sheet = (SpriteSheet*)obj;
        if (sheet->texHandle.isValid()) {
            asset::unload(sheet->texHandle);
            sheet->texHandle.reset();
        }
    
        BX_FREE(alloc ? alloc : gSpriteMgr->alloc, sheet->buff);
    }

    void SpriteSheetLoader::onReload(AssetHandle handle, bx::AllocatorI* alloc)
    {
        // Search in sprites, check if spritesheet is the same and reload the data
        Sprite::LNode* node = gSpriteMgr->spriteList.getFirst();
        while (node) {
            Sprite* sprite = node->data;
            for (int i = 0, c = sprite->frames.getCount(); i < c; i++) {
                if (sprite->frames[i].ssHandle != handle)
                    continue;

                // find the frame in spritesheet and update data
                SpriteFrame& frame = sprite->frames[i];
                SpriteSheet* sheet = asset::getObjPtr<SpriteSheet>(handle);
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
                    frame.texHandle = asset::getFailHandle("texture");
                    Texture* tex = asset::getObjPtr<Texture>(frame.texHandle);
                    frame.pivot = vec2(0, 0);
                    frame.frame = rect(0, 0, 1.0f, 1.0f);
                    frame.sourceSize = vec2(float(tex->info.width), float(tex->info.height));
                    frame.posOffset = vec2(0, 0);
                    frame.sizeOffset = vec2(1.0f, 1.0f);
                    frame.rotOffset = 0;
                    frame.pixelRatio = 1.0f;
                }
            }
            node = node->next;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static SpriteSheet* createDummySpriteSheet(AssetHandle texHandle, bx::AllocatorI* alloc)
    {
        // Create sprite sheet
        size_t totalSz = sizeof(SpriteSheet) + sizeof(SpriteSheetFrame) + sizeof(SpriteMesh) + bx::LinearAllocator::getExtraAllocSize(3);
        uint8_t* buff = (uint8_t*)BX_ALLOC(alloc, totalSz);
        if (!buff) {
            TEE_ERROR("Out of Memory");
            return nullptr;
        }

        bx::LinearAllocator lalloc(buff, totalSz);
        SpriteSheet* ss = BX_NEW(&lalloc, SpriteSheet); 
        ss->buff = buff;
        ss->frames = (SpriteSheetFrame*)BX_ALLOC(&lalloc, sizeof(SpriteSheetFrame));
        ss->numFrames = 1;
        ss->meshes = (SpriteMesh*)BX_ALLOC(&lalloc, sizeof(SpriteMesh));

        ss->texHandle = asset::getFailHandle("texture");
        BX_ASSERT(ss->texHandle.isValid());

        Texture* tex = asset::getObjPtr<Texture>(ss->texHandle);
        float imgWidth = float(tex->info.width);
        float imgHeight = float(tex->info.height);

        ss->frames[0].filenameHash = 0;
        ss->frames[0].frame = rect(0, 0, 1.0f, 1.0f);
        ss->frames[0].pivot = vec2(0, 0);
        ss->frames[0].posOffset = vec2(0, 0);
        ss->frames[0].sizeOffset = vec2(1.0f, 1.0f);
        ss->frames[0].sourceSize = vec2(imgWidth, imgHeight);
        ss->frames[0].rotOffset = 0;

        static uint16_t tris[] = {
            0, 1, 2,
            2, 1, 3
        };

        static vec2_t verts[] = {
            vec2(-0.5f, 0.5f),
            vec2(0.5f, 0.5f),
            vec2(-0.5f, -0.5f),
            vec2(0.5f, -0.5f)   
        };

        static vec2_t uvs[] = {
            vec2(0, 0),
            vec2(1.0f, 0),
            vec2(0, 1.0f),
            vec2(1.0f, 1.0f)
        };

        ss->meshes[0].numTris = 2;
        ss->meshes[0].numVerts = 4;
        ss->meshes[0].tris = tris;
        ss->meshes[0].verts = verts;
        ss->meshes[0].uvs = uvs;

        return ss;
    }

    bool gfx::initSpriteSystem(GfxDriver* driver, bx::AllocatorI* alloc)
    {
        if (gSpriteMgr) {
            BX_ASSERT(false);
            return false;
        }

        gSpriteMgr = BX_NEW(alloc, SpriteMgr)(alloc);
        if (!gSpriteMgr) {
            return false;
        }

        gSpriteMgr->driver = driver;

        SpriteVertex::init();

        gSpriteMgr->spriteProg = 
            driver->createProgram(driver->createShader(driver->makeRef(sprite_vso, sizeof(sprite_vso), nullptr, nullptr)),
                                  driver->createShader(driver->makeRef(sprite_fso, sizeof(sprite_fso), nullptr, nullptr)),
                                  true);
        if (!gSpriteMgr->spriteProg.isValid())
            return false;

        gSpriteMgr->u_texture = driver->createUniform("u_texture", UniformType::Int1, 1);

        // Create fail spritesheet
        gSpriteMgr->failSheet = createDummySpriteSheet(asset::getFailHandle("texture"), gSpriteMgr->alloc);    
        gSpriteMgr->asyncSheet = createDummySpriteSheet(asset::getAsyncHandle("texture"), gSpriteMgr->alloc);
        if (!gSpriteMgr->failSheet || !gSpriteMgr->asyncSheet) {
            TEE_ERROR("Creating async/fail spritesheets failed");
            return false;
        }

        return true;
    }

    Sprite* sprite::create(bx::AllocatorI* alloc, const vec2_t halfSize)
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
        gSpriteMgr->spriteList.addToEnd(&sprite->lnode);
        return sprite;
    }

    void sprite::destroy(Sprite* sprite)
    {
        // Destroy frames
        for (int i = 0, c = sprite->frames.getCount(); i < c; i++) {
            SpriteFrame& frame = sprite->frames[i];
            if (frame.flags & SpriteFlag::DestroyResource) {
                if (frame.ssHandle.isValid())
                    asset::unload(frame.ssHandle);
                else
                    asset::unload(frame.texHandle);
            }
        }
        sprite->frames.destroy();

        // Remove from list
        gSpriteMgr->spriteList.remove(&sprite->lnode);

        BX_DELETE(sprite->alloc, sprite);
    }

    void gfx::shutdownSpriteSystem()
    {
        if (!gSpriteMgr)
            return;

        bx::AllocatorI* alloc = gSpriteMgr->alloc;
        shutdownSpriteSystemGraphics();

        // Move through the remaining sprites and unload their resources
        Sprite::LNode* node = gSpriteMgr->spriteList.getFirst();
        while (node) {
            Sprite::LNode* next = node->next;
            sprite::destroy(node->data);
            node = next;
        }

        if (gSpriteMgr->failSheet)
            gSpriteMgr->loader.unloadObj(uintptr_t(gSpriteMgr->failSheet), gSpriteMgr->alloc);
        if (gSpriteMgr->asyncSheet)
            gSpriteMgr->loader.unloadObj(uintptr_t(gSpriteMgr->asyncSheet), gSpriteMgr->alloc);

        BX_DELETE(alloc, gSpriteMgr);
        gSpriteMgr = nullptr;
    }

    bool gfx::initSpriteSystemGraphics(GfxDriver* driver)
    {
        gSpriteMgr->driver = driver;

        gSpriteMgr->spriteProg =
            driver->createProgram(driver->createShader(driver->makeRef(sprite_vso, sizeof(sprite_vso), nullptr, nullptr)),
                                  driver->createShader(driver->makeRef(sprite_fso, sizeof(sprite_fso), nullptr, nullptr)),
                                  true);
        if (!gSpriteMgr->spriteProg.isValid())
            return false;

        gSpriteMgr->u_texture = driver->createUniform("u_texture", UniformType::Int1, 1);

        // Recreate all sprite cache buffers
        SpriteCache::LNode* node = gSpriteMgr->spriteCacheList.getFirst();
        while (node) {
            SpriteCache* sc = node->data;

            BX_ASSERT(sc->verts);
            BX_ASSERT(sc->indices);
            sc->vb = driver->createVertexBuffer(driver->makeRef(sc->verts, sc->vbSize, nullptr, nullptr), SpriteVertex::Decl, 0);
            sc->ib = driver->createIndexBuffer(driver->makeRef(sc->indices, sc->ibSize, nullptr, nullptr), 0);

            node = node->next;
        }

        return true;
    }

    void gfx::shutdownSpriteSystemGraphics()
    {
        GfxDriver* driver = gSpriteMgr->driver;

        if (gSpriteMgr->spriteProg.isValid())
            driver->destroyProgram(gSpriteMgr->spriteProg);
        if (gSpriteMgr->u_texture.isValid())
            driver->destroyUniform(gSpriteMgr->u_texture);

        // Destroy all sprite cache buffers
        SpriteCache::LNode* node = gSpriteMgr->spriteCacheList.getFirst();
        while (node) {
            SpriteCache* sc = node->data;
            if (sc->vb.isValid())
                driver->destroyVertexBuffer(sc->vb);
            if (sc->ib.isValid())
                driver->destroyIndexBuffer(sc->ib);
            sc->vb = VertexBufferHandle();
            sc->ib = IndexBufferHandle();
            node = node->next;
        }
    }

    void sprite::setRenderMode(SpriteRenderMode::Enum mode)
    {
        gSpriteMgr->renderMode = mode;
    }

    SpriteRenderMode::Enum sprite::getRenderMode()
    {
        return gSpriteMgr->renderMode;
    }

    void sprite::addFrame(Sprite* sprite,
                          AssetHandle texHandle, SpriteFlag::Bits flags, const vec2_t pivot /*= vec2_t(0, 0)*/, 
                          const vec2_t topLeftCoords /*= vec2_t(0, 0)*/, const vec2_t bottomRightCoords /*= vec2_t(1.0f, 1.0f)*/, 
                          const char* frameTag /*= nullptr*/)
    {
        if (texHandle.isValid()) {
            BX_ASSERT(asset::getState(texHandle) != AssetState::LoadInProgress);
 
            SpriteFrame* frame = BX_PLACEMENT_NEW(sprite->frames.push(), SpriteFrame);
            if (frame) {
                frame->texHandle = texHandle;
                frame->flags = flags;
                frame->pivot = pivot;
                frame->frame = rect(topLeftCoords, bottomRightCoords);
                frame->tagHash = !frameTag ? 0 : tinystl::hash_string(frameTag, strlen(frameTag)) ;

                Texture* tex = asset::getObjPtr<Texture>(texHandle);
                frame->sourceSize = vec2(float(tex->info.width), float(tex->info.height));
                frame->posOffset = vec2(0, 0);
                frame->sizeOffset = vec2(1.0f, 1.0f);
                frame->rotOffset = 0;
                frame->pixelRatio = ((bottomRightCoords.x - topLeftCoords.x)*frame->sourceSize.x) /
                    ((bottomRightCoords.y - topLeftCoords.y)*frame->sourceSize.y);
            }
        }
    }

    void sprite::addFrame(Sprite* sprite,
                          AssetHandle ssHandle, const char* name, SpriteFlag::Bits flags, 
                          const char* frameTag /*= nullptr*/)
    {
        BX_ASSERT(name);
        if (ssHandle.isValid()) {
            BX_ASSERT(asset::getState(ssHandle) != AssetState::LoadInProgress);

            SpriteFrame* frame = BX_PLACEMENT_NEW(sprite->frames.push(), SpriteFrame);
            if (frame) {
                frame->ssHandle = ssHandle;
                SpriteSheet* sheet = asset::getObjPtr<SpriteSheet>(ssHandle);

                // find frame name in spritesheet, if found fill the frame data
#ifdef _DEBUG
                frame->name = name;
#endif
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
                    frame->texHandle = asset::getFailHandle("texture");
                    Texture* tex = asset::getObjPtr<Texture>(frame->texHandle);
                    frame->flags = flags;
                    frame->pivot = vec2(0, 0);
                    frame->frame = rect(0, 0, 1.0f, 1.0f);
                    frame->sourceSize = vec2(float(tex->info.width), float(tex->info.height));
                    frame->posOffset = vec2(0, 0);
                    frame->sizeOffset = vec2(1.0f, 1.0f);
                    frame->rotOffset = 0;
                    frame->pixelRatio = 1.0f;
                    frame->meshId = 0;
                }
            }
        }
    }

    void sprite::addAllFrames(Sprite* sprite, AssetHandle ssHandle, SpriteFlag::Bits flags)
    {
        if (ssHandle.isValid()) {
            BX_ASSERT(asset::getState(ssHandle) != AssetState::LoadInProgress);

            SpriteSheet* sheet = asset::getObjPtr<SpriteSheet>(ssHandle);
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

    void sprite::animate(Sprite** sprites, uint16_t numSprites, float dt)
    {
        for (int i = 0; i < numSprites; i++) {
            Sprite* sprite = sprites[i];
            bool playReverse = sprite->playReverse;

            if (!bx::equal(sprite->playSpeed, 0, 0.00001f)) {
                float t = sprite->animTm;
                t += dt;
                float progress = t * sprite->playSpeed;
                float frames = bx::floor(progress);
                float reminder = frames > 0 ? bx::mod(progress, frames) : progress;
                t = reminder / sprite->playSpeed;

                // Progress sprite frame
                int curFrameIdx = sprite->curFrameIdx;
                int frameIdx = curFrameIdx;
                int iframes = int(frames);
                if (sprite->endCallback == nullptr) {
                    frameIdx = tmath::iwrap(!playReverse ? (frameIdx + iframes) : (frameIdx - iframes),
                                     0, sprite->frames.getCount() - 1);
                } else {
                    if (sprite->triggerEndCallback && iframes > 0) {
                        sprite->triggerEndCallback = false;
                        sprite->endCallback(sprite, frameIdx, sprite->endUserData);
                    }

                    int nextFrame = !playReverse ? (frameIdx + iframes) : (frameIdx - iframes);
                    frameIdx = bx::clamp<int>(nextFrame, 0, sprite->frames.getCount() - 1);

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

    void sprite::invertAnim(Sprite* sprite)
    {
        sprite->playReverse = !sprite->playReverse;
    }

    void sprite::setAnimSpeed(Sprite* sprite, float speed)
    {
        sprite->playSpeed = sprite->resumeSpeed = speed;
    }

    float sprite::getAnimSpeed(Sprite* sprite)
    {
        return sprite->resumeSpeed;
    }

    void sprite::pauseAnim(Sprite* sprite)
    {
        sprite->playSpeed = 0;
    }

    void sprite::resumeAnim(Sprite* sprite)
    {
        sprite->playSpeed = sprite->resumeSpeed;
    }

    void sprite::stopAnim(Sprite* sprite)
    {
        sprite->triggerEndCallback = false;
        sprite->curFrameIdx = 0;
        sprite->playSpeed = 0;
    }

    void sprite::replayAnim(Sprite* sprite)
    {
        sprite->triggerEndCallback = false;
        sprite->curFrameIdx = 0;
        sprite->playSpeed = sprite->resumeSpeed;
    }

    void sprite::setFrameEvent(Sprite* sprite, const char* name, sprite::FrameCallback callback, void* userData)
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

    void sprite::setFrameEvent(Sprite* sprite, int frameIdx, sprite::FrameCallback callback, void* userData)
    {
        BX_ASSERT(frameIdx < sprite->frames.getCount());
        SpriteFrame* frame = sprite->frames.itemPtr(frameIdx);
        frame->frameCallback = callback;
        frame->frameCallbackUserData = userData;
    }

    void sprite::setEndEvent(Sprite* sprite, sprite::FrameCallback callback, void* userData)
    {
        sprite->endCallback = callback;
        sprite->endUserData = userData;
        sprite->triggerEndCallback = false;
    }

    void sprite::setHalfSize(Sprite* sprite, const vec2_t& halfSize)
    {
        sprite->halfSize = halfSize;
    }

    vec2_t sprite::getHalfSize(Sprite* sprite)
    {
        return sprite->halfSize;
    }

    void sprite::setScale(Sprite* sprite, const vec2_t& scale)
    {
        sprite->scale = scale;
    }

    vec2_t sprite::getScale(Sprite* sprite)
    {
        return sprite->scale;
    }

    void sprite::goFrame(Sprite* sprite, int frameIdx)
    {
        BX_ASSERT(frameIdx < sprite->frames.getCount());
        sprite->curFrameIdx = frameIdx;
    }

    void sprite::goFrame(Sprite* sprite, const char* name)
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

    void sprite::goTag(Sprite* sprite, const char* tag)
    {
        size_t nameHash = tinystl::hash_string(tag, strlen(tag));
        int curIdx = sprite->curFrameIdx;
        for (int i = 0, c = sprite->frames.getCount(); i < c; i++) {
            int idx = (i + curIdx) % c;
            if (sprite->frames[idx].tagHash == nameHash) {
                sprite->curFrameIdx = idx;
                return;
            }
        }
    }

    int sprite::getFrame(Sprite* sprite)
    {
        return sprite->curFrameIdx;
    }

    int sprite::getFrameCount(Sprite* sprite)
    {
        return sprite->frames.getCount();
    }

    void sprite::setFlip(Sprite* sprite, SpriteFlag::Bits flip)
    {
        sprite->flip = flip;
    }

    SpriteFlip::Bits sprite::getFlip(Sprite* sprite)
    {
        return sprite->flip;
    }

    void sprite::setPosOffset(Sprite* sprite, const vec2_t posOffset)
    {
        sprite->posOffset = posOffset;
    }

    vec2_t sprite::getPosOffset(Sprite* sprite)
    {
        return sprite->posOffset;
    }

    void sprite::setOrder(Sprite* sprite, uint8_t order)
    {
        sprite->order = order;
    }

    int sprite::getOrder(Sprite* sprite)
    {
        return sprite->order;
    }

    void sprite::setPivot(Sprite* sprite, const vec2_t pivot)
    {
        for (int i = 0, c = sprite->frames.getCount(); i < c; i++)
            sprite->frames[i].pivot = pivot;
    }

    void sprite::setColor(Sprite* sprite, ucolor_t color)
    {
        sprite->color = color;
    }

    void sprite::setColorAdd(Sprite* sprite, ucolor_t color)
    {
        sprite->colorAdd = color;
    }

    ucolor_t sprite::getColor(Sprite* sprite)
    {
        return sprite->color;
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
        halfSize = halfSize * sprite->scale;
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

        return rect(offset - halfSize, halfSize + offset);
    }

    rect_t sprite::getDrawRect(Sprite* sprite)
    {
        return getSpriteDrawRectFrame(sprite, sprite->curFrameIdx);
    }

    void sprite::getRealRect(Sprite* sprite, vec2_t* pHalfSize, vec2_t* pCenter)
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

    vec2_t sprite::getImageSize(Sprite* sprite)
    {
        return sprite->getCurFrame().sourceSize;
    }

    rect_t sprite::getTexelCoords(Sprite* sprite)
    {
        return sprite->getCurFrame().frame;
    }

    void sprite::setUserData(Sprite* sprite, void* userData)
    {
        sprite->userData = userData;
    }

    void* sprite::getUserData(Sprite* sprite)
    {
        return sprite->userData;
    }

    void sprite::getDrawData(Sprite* sprite, int frameIdx, rect_t* drawRect, rect_t* textureRect, AssetHandle* textureHandle)
    {
        *drawRect = getSpriteDrawRectFrame(sprite, frameIdx);
        *textureRect = sprite->frames[frameIdx].frame;
        *textureHandle = sprite->frames[frameIdx].texHandle;
    }

    void sprite::convertCoordsPixelToLogical(vec2_t* ptsOut, const vec2_t* ptsIn, int numPts, Sprite* sprite)
    {
        vec2_t halfSize;
        vec2_t center;
        vec2_t imgSize = getImageSize(sprite);
        getRealRect(sprite, &halfSize, &center);
        SpriteFlip::Bits flip = sprite->flip;

        const SpriteFrame& frame = sprite->getCurFrame();
        float scale = 1.0f;
        if (frame.ssHandle.isValid())
            scale = asset::getObjPtr<SpriteSheet>(frame.ssHandle)->scale;

        for (int i = 0; i < numPts; i++) {
            vec2_t pt = ptsIn[i];
            if (flip & SpriteFlip::FlipX)
                pt.x = -pt.x;
            if (flip & SpriteFlip::FlipY)
                pt.y = -pt.y;

            pt = vec2(pt.x/imgSize.x, pt.y/imgSize.y);
            ptsOut[i] = (pt * halfSize * 2.0f - center) * scale;
        }
    }

    static void drawSpritesWireframe(uint8_t viewId, Sprite** sprites, uint16_t numSprites, const mat3_t* mats)
    {
        if (numSprites <= 0)
            return;
        BX_ASSERT(sprites);

        GfxDriver* gDriver = gSpriteMgr->driver;

        // Evaluate final vertices and indexes
        int numVerts = 0, numIndices = 0;
        for (int si = 0; si < numSprites; si++) {
            Sprite* sprite = sprites[si];
            const SpriteFrame& frame = sprite->frames[sprite->curFrameIdx];

            if (frame.ssHandle.isValid()) {
                BX_ASSERT(frame.meshId != -1);
                SpriteSheet* sheet = asset::getObjPtr<SpriteSheet>(frame.ssHandle);
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
        mat3_t preMat, finalMat;
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
            vec2_t scale = vec2(scalex, scaley) * sprite->scale;

            if (flipX & SpriteFlip::FlipX)
                scale.x = -scale.x;
            if (flipY & SpriteFlip::FlipY)
                scale.y = -scale.y;

            // Transform (offset x sprite's own matrix)
            // PreMat = TranslateMat * ScaleMat
            preMat.m11 = scale.x;       preMat.m12 = 0;             preMat.m13 = 0;
            preMat.m21 = 0;             preMat.m22 = scale.y;       preMat.m23 = 0;
            preMat.m31 = scale.x*pos.x; preMat.m32 = scale.y*pos.y; preMat.m33 = 1.0f;
            bx::mat3Mul(finalMat.f, preMat.f, mats[ss.index].f);

            vec3_t transform1 = vec3(finalMat.m11, finalMat.m12, finalMat.m21);
            vec3_t transform2 = vec3(finalMat.m22, finalMat.m31, finalMat.m32);
            uint32_t color = sprite->color.n;
            uint32_t colorAdd = sprite->colorAdd.n;

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
                SpriteSheet* sheet = asset::getObjPtr<SpriteSheet>(frame.ssHandle);
                BX_ASSERT(frame.meshId != -1);
                const SpriteMesh& mesh = sheet->meshes[frame.meshId];

                for (int i = 0, c = mesh.numVerts; i < c; i++) {
                    int k = i + vertexIdx;
                    verts[k].pos = mesh.verts[i];
                    verts[k].transform1 = transform1;
                    verts[k].transform2 = transform2;
                    verts[k].coords = vec2(mesh.uvs[i].x, mesh.uvs[i].y);
                    verts[k].color = color;
                    verts[k].colorAdd = colorAdd;
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
                v0.pos = vec2(-0.5f, 0.5f);
                v0.coords = vec2(0, 0);

                v1.pos = vec2(0.5f, 0.5f);
                v1.coords = vec2(1.0f, 0);

                v2.pos = vec2(-0.5f, -0.5f);
                v2.coords = vec2(0, 1.0f);

                v3.pos = vec2(0.5f, -0.5f);
                v3.coords = vec2(1.0f, 1.0f);

                v0.transform1 = v1.transform1 = v2.transform1 = v3.transform1 = transform1;
                v0.transform2 = v1.transform2 = v2.transform2 = v3.transform2 = transform2;
                v0.color = v1.color = v2.color = v3.color = color;
                v0.colorAdd = v1.colorAdd = v2.colorAdd = v3.colorAdd = colorAdd;

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
        GfxState::Bits state = 
            gfx::stateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite | GfxState::PrimitiveLines;
        ProgramHandle prog = gSpriteMgr->spriteProg;
        for (int i = 0, c = batches.getCount(); i < c; i++) {
            const SpriteDrawBatch batch = batches[i];
            gDriver->setState(state, 0);
            gDriver->setTransientVertexBufferI(0, &tvb, 0, batch.numVerts);
            gDriver->setTransientIndexBufferI(&tib, batch.startIdx, batch.numIndices);
            gDriver->setTexture(0, gSpriteMgr->u_texture, gfx::getWhiteTexture1x1(), TextureFlag::FromTexture);
            gDriver->submit(viewId, prog, 0, false);
        }

        batches.destroy();
    }


    void sprite::draw(uint8_t viewId, Sprite** sprites, uint16_t numSprites, const mat3_t* mats,
                      ProgramHandle progOverride /*= ProgramHandle()*/, sprite::StateCallback stateCallback /*= nullptr*/,
                      void* stateUserData, const ucolor_t* colors)
    {
    #if termite_DEV
        if (gSpriteMgr->renderMode == SpriteRenderMode::Wireframe)
            return drawSpritesWireframe(viewId, sprites, numSprites, mats);
    #endif

        // Filter out invalid sprites
        for (int si = 0; si < numSprites; si++) {
            if (sprites[si]->curFrameIdx < sprites[si]->frames.getCount())
                continue;
            bx::xchg(sprites[si], sprites[numSprites-1]);
            --numSprites;
        }

        if (numSprites <= 0)
            return;
        BX_ASSERT(sprites);

        GfxDriver* gDriver = gSpriteMgr->driver;

        // Evaluate final vertices and indexes
        int numVerts = 0, numIndices = 0;
        for (int si = 0; si < numSprites; si++) {
            Sprite* sprite = sprites[si];
            const SpriteFrame& frame = sprite->frames[sprite->curFrameIdx];

            if (frame.ssHandle.isValid()) {
                BX_ASSERT(frame.meshId != -1);
                SpriteSheet* sheet = asset::getObjPtr<SpriteSheet>(frame.ssHandle);
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
        mat3_t preMat, finalMat;
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
            vec2_t scale = vec2(scalex, scaley) * sprite->scale;

            bool reorder = false;
            if (flipX & SpriteFlip::FlipX) {
                scale.x = -scale.x;
                reorder = !reorder;
            }
            if (flipY & SpriteFlip::FlipY) {
                scale.y = -scale.y;
                reorder = !reorder;
            }

            // Transform (offset x sprite's own matrix)
            // PreMat = TranslateMat * ScaleMat
            preMat.m11 = scale.x;       preMat.m12 = 0;             preMat.m13 = 0;
            preMat.m21 = 0;             preMat.m22 = scale.y;       preMat.m23 = 0;
            preMat.m31 = scale.x*pos.x; preMat.m32 = scale.y*pos.y; preMat.m33 = 1.0f;
            bx::mat3Mul(finalMat.f, preMat.f, mats[ss.index].f);

            vec3_t transform1 = vec3(finalMat.m11, finalMat.m12, finalMat.m21);
            vec3_t transform2 = vec3(finalMat.m22, finalMat.m31, finalMat.m32);
            uint32_t color;
            if (!colors)    color = ss.sprite->color.n;
            else            color = colors[ss.index].n;
            uint32_t colorAdd = sprite->colorAdd.n;

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
        
            int startIdx = indexIdx;
            // Fill geometry data
            if (frame.ssHandle.isValid()) {
                SpriteSheet* sheet = asset::getObjPtr<SpriteSheet>(frame.ssHandle);
                BX_ASSERT(frame.meshId != -1);
                const SpriteMesh& mesh = sheet->meshes[frame.meshId];

                for (int i = 0, c = mesh.numVerts; i < c; i++) {
                    int k = i + vertexIdx;
                    verts[k].pos = mesh.verts[i];
                    verts[k].transform1 = transform1;
                    verts[k].transform2 = transform2;
                    verts[k].coords = vec2(mesh.uvs[i].x, mesh.uvs[i].y);
                    verts[k].color = color;
                    verts[k].colorAdd = colorAdd;
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
                v0.pos = vec2(-0.5f, 0.5f);
                v0.coords = vec2(0, 0);

                v1.pos = vec2(0.5f, 0.5f);
                v1.coords = vec2(1.0f, 0);

                v2.pos = vec2(-0.5f, -0.5f);
                v2.coords = vec2(0, 1.0f);

                v3.pos = vec2(0.5f, -0.5f);
                v3.coords = vec2(1.0f, 1.0f);

                v0.transform1 = v1.transform1 = v2.transform1 = v3.transform1 = transform1;
                v0.transform2 = v1.transform2 = v2.transform2 = v3.transform2 = transform2;
                v0.color = v1.color = v2.color = v3.color = color;
                v0.colorAdd = v1.colorAdd = v2.colorAdd = v3.colorAdd = colorAdd;

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

            if (reorder) {
                for (int i = startIdx; i < indexIdx; i+=3) {
                    bx::xchg(indices[i], indices[i + 2]);
                }
            }
        }

        // Draw
        GfxState::Bits state = gfx::stateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite | GfxState::CullCCW;
        ProgramHandle prog = !progOverride.isValid() ? gSpriteMgr->spriteProg : progOverride;
        for (int i = 0, c = batches.getCount(); i < c; i++) {
            const SpriteDrawBatch batch = batches[i];
            gDriver->setState(state, 0);
            gDriver->setTransientVertexBufferI(0, &tvb, 0, batch.numVerts);
            gDriver->setTransientIndexBufferI(&tib, batch.startIdx, batch.numIndices);

            bool textureOverride = false;
            if (stateCallback) {
                stateCallback(gDriver, stateUserData, &textureOverride);
            }
            if (batch.texHandle.isValid() & !textureOverride)
                gDriver->setTexture(0, gSpriteMgr->u_texture, asset::getObjPtr<Texture>(batch.texHandle)->handle, TextureFlag::FromTexture);
            gDriver->submit(viewId, prog, 0, false);
        }

        batches.destroy();
    }

    SpriteCache* sprite::createCache(bx::AllocatorI* alloc, Sprite** sprites, uint16_t numSprites, const mat3_t* mats,
                                     const ucolor_t* colors /*= nullptr*/)
    {
        // Filter out invalid sprites
        for (int si = 0; si < numSprites; si++) {
            if (sprites[si]->curFrameIdx < sprites[si]->frames.getCount())
                continue;
            bx::xchg(sprites[si], sprites[numSprites-1]);
            --numSprites;
        }

        if (numSprites <= 0)
            return nullptr;
        BX_ASSERT(sprites);

        // Evaluate final vertices and indexes
        int numVerts = 0, numIndices = 0;
        for (int si = 0; si < numSprites; si++) {
            Sprite* sprite = sprites[si];
            const SpriteFrame& frame = sprite->frames[sprite->curFrameIdx];

            if (frame.ssHandle.isValid()) {
                BX_ASSERT(frame.meshId != -1);
                SpriteSheet* sheet = asset::getObjPtr<SpriteSheet>(frame.ssHandle);
                const SpriteMesh& mesh = sheet->meshes[frame.meshId];
                numVerts += mesh.numVerts;
                numIndices += mesh.numTris*3;
            } else {
                numVerts += 4;
                numIndices += 6;
            }
        }

        // Create vertex and index buffers
        SpriteCache* sc = BX_NEW(alloc, SpriteCache)(alloc);
        sc->vbSize = sizeof(SpriteVertex)*numVerts;
        sc->verts = BX_ALLOC(alloc, sc->vbSize);
        sc->ibSize = sizeof(uint16_t)*numIndices;
        sc->indices = BX_ALLOC(alloc, sc->ibSize);

        SpriteVertex* verts = (SpriteVertex*)sc->verts;
        uint16_t* indices = (uint16_t*)sc->indices;

        bx::AllocatorI* tmpAlloc = getTempAlloc();

        // Calculate bounds
        sc->bounds = rect_t::Null;
        for (int i = 0; i < numSprites; i++) {
            rect_t spriteBounds = getDrawRect(sprites[i]);
            vec2_t bounds[4] = {spriteBounds.vmin,
                vec2(spriteBounds.xmax, spriteBounds.ymin),
                vec2(spriteBounds.xmin, spriteBounds.ymax),
                spriteBounds.vmax};
            for (int k = 0; k < 4; k++) {
                vec2_t tpt;
                bx::vec2MulMat3(tpt.f, bounds[k].f, mats[i].f);
                tmath::rectPushPoint(&sc->bounds, tpt);
            }
        }

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
        mat3_t preMat, finalMat;
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
            vec2_t scale = vec2(scalex, scaley) * sprite->scale;

            bool reorder = false;
            if (flipX & SpriteFlip::FlipX) {
                scale.x = -scale.x;
                reorder = !reorder;
            }
            if (flipY & SpriteFlip::FlipY) {
                scale.y = -scale.y;
                reorder = !reorder;
            }

            // Transform (offset x sprite's own matrix)
            // PreMat = TranslateMat * ScaleMat
            preMat.m11 = scale.x;       preMat.m12 = 0;             preMat.m13 = 0;
            preMat.m21 = 0;             preMat.m22 = scale.y;       preMat.m23 = 0;
            preMat.m31 = scale.x*pos.x; preMat.m32 = scale.y*pos.y; preMat.m33 = 1.0f;
            bx::mat3Mul(finalMat.f, preMat.f, mats[ss.index].f);

            vec3_t transform1 = vec3(finalMat.m11, finalMat.m12, finalMat.m21);
            vec3_t transform2 = vec3(finalMat.m22, finalMat.m31, finalMat.m32);
            uint32_t color;
            if (!colors)    color = ss.sprite->color.n;
            else            color = colors[ss.index].n;
            uint32_t colorAdd = ss.sprite->colorAdd.n;

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
            int startIdx = indexIdx;
            if (frame.ssHandle.isValid()) {
                SpriteSheet* sheet = asset::getObjPtr<SpriteSheet>(frame.ssHandle);
                BX_ASSERT(frame.meshId != -1);
                const SpriteMesh& mesh = sheet->meshes[frame.meshId];

                for (int i = 0, c = mesh.numVerts; i < c; i++) {
                    int k = i + vertexIdx;
                    verts[k].pos = mesh.verts[i];
                    verts[k].transform1 = transform1;
                    verts[k].transform2 = transform2;
                    verts[k].coords = vec2(mesh.uvs[i].x, mesh.uvs[i].y);
                    verts[k].color = color;
                    verts[k].colorAdd = colorAdd;
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
                v0.pos = vec2(-0.5f, 0.5f);
                v0.coords = vec2(0, 0);

                v1.pos = vec2(0.5f, 0.5f);
                v1.coords = vec2(1.0f, 0);

                v2.pos = vec2(-0.5f, -0.5f);
                v2.coords = vec2(0, 1.0f);

                v3.pos = vec2(0.5f, -0.5f);
                v3.coords = vec2(1.0f, 1.0f);

                v0.transform1 = v1.transform1 = v2.transform1 = v3.transform1 = transform1;
                v0.transform2 = v1.transform2 = v2.transform2 = v3.transform2 = transform2;
                v0.color = v1.color = v2.color = v3.color = color;
                v0.colorAdd = v1.colorAdd = v2.colorAdd = v3.colorAdd = colorAdd;

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

            if (reorder) {
                for (int i = startIdx; i < indexIdx; i += 3) {
                    bx::xchg(indices[i], indices[i + 2]);
                }
            }
        }

        // save batches
        sc->numBatches = batches.getCount();
        sc->batches = (SpriteDrawBatch*)BX_ALLOC(alloc, sizeof(SpriteDrawBatch)*sc->numBatches);
        bx::memCopy(sc->batches, batches.getBuffer(), batches.getCount()*sizeof(SpriteDrawBatch));
        batches.destroy();

        // Create buffers
        GfxDriver* gDriver = getGfxDriver();
        sc->vb = gDriver->createVertexBuffer(gDriver->makeRef(sc->verts, sc->vbSize, nullptr, nullptr), SpriteVertex::Decl, 0);
        sc->ib = gDriver->createIndexBuffer(gDriver->makeRef(sc->indices, sc->ibSize, nullptr, nullptr), 0);

        // Add to SpriteCache list for graphics restore
        gSpriteMgr->spriteCacheList.add(&sc->lnode);

        return sc;
    }

    void sprite::drawCache(uint8_t viewId, SpriteCache* spriteCache,
                           ProgramHandle progOverride /*= ProgramHandle()*/, 
                           sprite::StateCallback stateCallback /*= nullptr*/, void* stateUserData /*= nullptr*/)
    {
        GfxDriver* gDriver = getGfxDriver();
        SpriteCache* sc = spriteCache;

        GfxState::Bits state = gfx::stateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite | GfxState::CullCCW;
        ProgramHandle prog = !progOverride.isValid() ? gSpriteMgr->spriteProg : progOverride;
        for (int i = 0, c = sc->numBatches; i < c; i++) {
            const SpriteDrawBatch batch = sc->batches[i];
            gDriver->setState(state, 0);
            gDriver->setVertexBufferI(0, sc->vb, 0, batch.numVerts);
            gDriver->setIndexBuffer(sc->ib, batch.startIdx, batch.numIndices);
            if (batch.texHandle.isValid())
                gDriver->setTexture(0, gSpriteMgr->u_texture, asset::getObjPtr<Texture>(batch.texHandle)->handle, TextureFlag::FromTexture);
            bool textureOverride = false;
            if (stateCallback) {
                stateCallback(gDriver, stateUserData, &textureOverride);
            }
            gDriver->submit(viewId, prog, 0, false);
        }
    }

    void sprite::destroyCache(SpriteCache* spriteCache)
    {
        SpriteCache* sc = spriteCache;
        if (sc->alloc) {
            gSpriteMgr->spriteCacheList.remove(&sc->lnode);

            if (sc->verts) {
                BX_FREE(sc->alloc, sc->verts);
            }

            if (sc->indices) {
                BX_FREE(sc->alloc, sc->indices);
            }

            if (sc->batches) {
                BX_FREE(sc->alloc, sc->batches);
            }

            GfxDriver* gDriver = getGfxDriver();
            if (sc->vb.isValid()) {
                gDriver->destroyVertexBuffer(sc->vb);
            }
            if (sc->ib.isValid()) {
                gDriver->destroyIndexBuffer(sc->ib);
            }

            BX_DELETE(sc->alloc, sc);
        }
    }

    rect_t sprite::getCacheBounds(SpriteCache* spriteCache)
    {
        return spriteCache->bounds;
    }

    void gfx::registerSpriteSheetToAssetLib()
    {
        AssetTypeHandle handle;
        handle = asset::registerType("spritesheet", &gSpriteMgr->loader, sizeof(LoadSpriteSheetParams), 
                                      uintptr_t(gSpriteMgr->failSheet), uintptr_t(gSpriteMgr->asyncSheet));
        BX_ASSERT(handle.isValid());
    }

    rect_t gfx::getSpriteSheetTextureFrame(AssetHandle spritesheet, int index)
    {
        SpriteSheet* ss = asset::getObjPtr<SpriteSheet>(spritesheet);
        BX_ASSERT(index < ss->numFrames);
        return ss->frames[index].frame;
    }

    rect_t gfx::getSpriteSheetTextureFrame(AssetHandle spritesheet, const char* name)
    {
        SpriteSheet* ss = asset::getObjPtr<SpriteSheet>(spritesheet);
        size_t filenameHash = tinystl::hash_string(name, strlen(name));
        for (int i = 0, c = ss->numFrames; i < c; i++) {
            if (ss->frames[i].filenameHash != filenameHash)
                continue;
        
            return ss->frames[i].frame;
        }
        return rect(0, 0, 1.0f, 1.0f);
    }

    AssetHandle gfx::getSpriteSheetTexture(AssetHandle spritesheet)
    {
        SpriteSheet* ss = asset::getObjPtr<SpriteSheet>(spritesheet);
        return ss->texHandle;
    }

    vec2_t gfx::getSpriteSheetFrameSize(AssetHandle spritesheet, int index)
    {
        SpriteSheet* ss = asset::getObjPtr<SpriteSheet>(spritesheet);
        BX_ASSERT(index < ss->numFrames);
        return ss->frames[index].sourceSize;
    }

    vec2_t gfx::getSpriteSheetFrameSize(AssetHandle spritesheet, const char* name)
    {
        SpriteSheet* ss = asset::getObjPtr<SpriteSheet>(spritesheet);
        size_t filenameHash = tinystl::hash_string(name, strlen(name));
        for (int i = 0, c = ss->numFrames; i < c; i++) {
            if (ss->frames[i].filenameHash != filenameHash)
                continue;

            return ss->frames[i].sourceSize;
        }
        return vec2(0, 0);
    }
}
