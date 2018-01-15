

#include "pch.h"
#include "gfx_driver.h"
#include "gfx_debugdraw2d.h"
#include "gfx_font.h"
#include "gfx_texture.h"
#include "internal.h"

#include "bx/hash.h"
#include "bxx/pool.h"
#include "bxx/stack.h"
#include "bxx/logger.h"

#include <stdarg.h>

#include TEE_MAKE_SHADER_PATH(shaders_h, vg.fso)
#include TEE_MAKE_SHADER_PATH(shaders_h, vg.vso)

#define MAX_BATCHES 256
#define MAX_VERTICES 2048
#define MAX_TEXT_SIZE 256
#define MAX_PARAM_SIZE 384
#define STATE_POOL_SIZE 8
#define TEXTHANDLER_ID 0x26d5 
#define RECTHANDLER_ID 0xed2c
#define LINEHANDLER_ID 0xbd2a

namespace tee
{
    struct vgVertexPosCoordColor
    {
        float x;
        float y;
        float tx;
        float ty;
        uint32_t color;

        static void init()
        {
            gfx::beginDecl(&Decl);
            gfx::addAttrib(&Decl, VertexAttrib::Position, 2, VertexAttribType::Float);
            gfx::addAttrib(&Decl, VertexAttrib::TexCoord0, 2, VertexAttribType::Float);
            gfx::addAttrib(&Decl, VertexAttrib::Color0, 4, VertexAttribType::Uint8, true);
            gfx::endDecl(&Decl);
        }
        static VertexDecl Decl;
    };
    VertexDecl vgVertexPosCoordColor::Decl;

    class BX_NO_VTABLE DrawHandler
    {
    public:
        virtual uint32_t getHash(const void* params) = 0;
        virtual void writePrimitives(DebugDraw2D* ctx, const void* params, vgVertexPosCoordColor* verts, int maxVerts,
                                     uint16_t* indices, int firstVertIdx, int maxIndices,
                                     int* numVertsWritten, int* numIndicesWritten) = 0;
        virtual GfxState::Bits setStates(DebugDraw2D* ctx, GfxDriver* driver, const void* params) = 0;
    };

    struct Batch
    {
        uint32_t hash;
        uint8_t params[MAX_PARAM_SIZE];
        DrawHandler* handler;
        int startVertex;
        int numVerts;
        int firstIdx;
        int numIndices;
        irect_t scissorRect;
        mat3_t xformMtx;
    };

    struct VgState
    {
        typedef bx::Stack<VgState*>::Node SNode;

        mat3_t mtx;
        ucolor_t textColor;
        ucolor_t strokeColor;
        ucolor_t fillColor;
        float alpha;
        irect_t scissor;
        AssetHandle fontHandle;
        SNode snode;

        VgState() :
            snode(this)
        {
        }

        void setDefault(DebugDraw2D* ctx);
    };

    struct DebugDraw2D
    {
        bx::AllocatorI* alloc;
        GfxDriver* driver;
        uint8_t viewId;

        vgVertexPosCoordColor* vertexBuff;
        int numVerts; 
        int maxVerts;

        uint16_t* indexBuff;
        int numIndices;
        int maxIndices;

        Batch* batches;
        int numBatches;
        int maxBatches;

        irect_t viewport;
        AssetHandle defaultFontHandle;
        bool readyToDraw;

        bx::FixedPool<VgState> statePool;
        bx::Stack<VgState*> stateStack;

        // Draw stuff
        ProgramHandle program;
        UniformHandle uTexture;

        mat4_t viewMtx;
        mat4_t projMtx;

        DebugDraw2D(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
            vertexBuff = nullptr;
            batches = nullptr;
            readyToDraw = false;
            maxVerts = maxBatches = 0;
            numVerts = numBatches = 0;
            numIndices = maxIndices = 0;
            viewport = irect(0, 0, 0, 0);
        }
    };

    struct BatchParams
    {
        mat3_t mtx;
        irect_t scissor;
        ucolor_t color;
    };

    struct TextParams : public BatchParams
    {
        AssetHandle fontHandle;
        char text[MAX_TEXT_SIZE];
        vec2_t pos;
    };

    class TextHandler : public DrawHandler
    {
    public:
        TextHandler();
        uint32_t getHash(const void* params) override;
        void writePrimitives(DebugDraw2D* ctx, const void* params, vgVertexPosCoordColor* verts, int maxVerts,
                             uint16_t* indices, int firstVertIdx, int maxIndices, 
                             int* numVertsWritten, int* numIndicesWritten) override;
        GfxState::Bits setStates(DebugDraw2D* ctx, GfxDriver* driver, const void* params) override;
    };

    struct RectParams : public BatchParams
    {
        rect_t rect;
        const Texture* image;
    };

    struct LineParams : public BatchParams
    {
        vec2_t p1;
        vec2_t p2;
        float width;
    };

    class RectHandler : public DrawHandler
    {
    public:
        RectHandler();
        uint32_t getHash(const void* params) override;
        void writePrimitives(DebugDraw2D* ctx, const void* params, vgVertexPosCoordColor* verts, int maxVerts,
            uint16_t* indices, int firstVertIdx, int maxIndices,
            int* numVertsWritten, int* numIndicesWritten) override;
        GfxState::Bits setStates(DebugDraw2D* ctx, GfxDriver* driver, const void* params) override;
    };

    class LineHandler : public DrawHandler
    {
    public:
        LineHandler();
        uint32_t getHash(const void* params) override;
        void writePrimitives(DebugDraw2D* ctx, const void* params, vgVertexPosCoordColor* verts, int maxVerts,
                             uint16_t* indices, int firstVertIdx, int maxIndices,
                             int* numVertsWritten, int* numIndicesWritten) override;
        GfxState::Bits setStates(DebugDraw2D* ctx, GfxDriver* driver, const void* params) override;
    };

    struct DebugDraw2DMgr
    {
        GfxDriver* driver;
        bx::AllocatorI* alloc;
        ProgramHandle program;
        TextureHandle whiteTexture;
        UniformHandle uTexture;
    
        TextHandler textHandler;
        RectHandler rectHandler;
        LineHandler lineHandler;
    
        DebugDraw2DMgr(bx::AllocatorI* _alloc)
        {
            driver = nullptr;
            alloc = _alloc;
        }
    };

    static DebugDraw2DMgr* gDebugDraw2D = nullptr;

    void VgState::setDefault(DebugDraw2D* ctx)
    {
        mtx = mat3I();
        textColor = ucolor(0, 255, 0);
        strokeColor = ucolor(0, 0, 0);
        fillColor = ucolor(255, 255, 255);
        alpha = 1.0f;
        scissor = ctx->viewport;
        fontHandle = ctx->defaultFontHandle;
    }

    static void pushBatch(DebugDraw2D* ctx, DrawHandler* handler, const void* params, size_t paramsSize)
    {
        if (ctx->numBatches == ctx->maxBatches)
            return;

        int firstVert = ctx->numVerts;
        int maxVerts = ctx->maxVerts - firstVert;
        if (maxVerts <= 0)
            return;
        int firstIdx = ctx->numIndices;
        int maxIndices = ctx->maxIndices - firstIdx;
        if (maxIndices <= 0)
            return;
        int numVertsWritten;
        int numIndicesWritten;
        handler->writePrimitives(ctx, params, &ctx->vertexBuff[firstVert], maxVerts, &ctx->indexBuff[firstIdx], 
                                 firstVert, maxIndices, &numVertsWritten, &numIndicesWritten);
        if (numVertsWritten == 0 || numIndicesWritten == 0)
            return;
        ctx->numVerts += numVertsWritten;
        ctx->numIndices += numIndicesWritten;

        // Hash batch based on states that break the drawcall
        const BatchParams* bparams = (const BatchParams*)params;
        VgState* state;
        ctx->stateStack.peek(&state);
        bx::HashMurmur2A hasher;
        hasher.begin();
        hasher.add<uint32_t>(handler->getHash(params));
        hasher.add<irect_t>(bparams->scissor);
        hasher.add<mat3_t>(bparams->mtx);
        uint32_t hash = hasher.end();

        // If hash is different from the previous one, create a new batch
        // we can only check with the previous call, because drawing is supposed to be sequential
        int batchIdx = ctx->numBatches;
        if (batchIdx > 0 && ctx->batches[batchIdx - 1].hash == hash) {
            // Expand the previous batch
            ctx->batches[batchIdx - 1].numVerts += numVertsWritten;
            ctx->batches[batchIdx - 1].numIndices += numIndicesWritten;
        } else {
            // Create a new batch
            Batch& batch = ctx->batches[batchIdx];
            batch.hash = hash;
            assert(paramsSize < MAX_PARAM_SIZE);
            memcpy(batch.params, params, paramsSize);
            batch.handler = handler;
            batch.startVertex = firstVert;
            batch.numVerts = numVertsWritten;
            batch.firstIdx = firstIdx;
            batch.numIndices = numIndicesWritten;
            batch.scissorRect = bparams->scissor;
            batch.xformMtx = bparams->mtx;

            ctx->numBatches++;
        }
    }

    static void drawBatches(DebugDraw2D* ctx)
    {
        GfxDriver* driver = ctx->driver;
        GfxState::Bits baseState = gfx::stateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite;

        uint8_t viewId = ctx->viewId;
        irect_t vp = ctx->viewport;
        int numVerts = ctx->numVerts;
        int numIndices = ctx->numIndices;

        driver->setViewRect(viewId, vp.xmin, vp.ymin, vp.xmax - vp.xmin, vp.ymax - vp.ymin);

        driver->setViewTransform(viewId, ctx->viewMtx.f, ctx->projMtx.f, GfxViewFlag::Stereo, nullptr);
        driver->setViewMode(viewId, ViewMode::Sequential);

        // Allocate and fill vertices
        TransientVertexBuffer tvb;
        if (driver->getAvailTransientVertexBuffer(numVerts, vgVertexPosCoordColor::Decl) != numVerts)
            return;
        driver->allocTransientVertexBuffer(&tvb, numVerts, vgVertexPosCoordColor::Decl);
        memcpy(tvb.data, ctx->vertexBuff, sizeof(vgVertexPosCoordColor)*numVerts);

        // Allocate and fill indices
        TransientIndexBuffer tib;
        if (driver->getAvailTransientIndexBuffer(numIndices) != numIndices)
            return;
        driver->allocTransientIndexBuffer(&tib, numIndices);
        memcpy(tib.data, ctx->indexBuff, sizeof(uint16_t)*numIndices);

        for (int i = 0, c = ctx->numBatches; i < c; i++) {
            const Batch& batch = ctx->batches[i];

            GfxState::Bits state = baseState | batch.handler->setStates(ctx, driver, batch.params);

            const mat3_t xf = batch.xformMtx;
            mat4_t worldMtx = mat4(xf.m11, xf.m12, 0.0f,
                                         xf.m21, xf.m22, 0.0f,
                                         0.0f,   0.0f,   1.0f,
                                         xf.m31, xf.m32, 0.0f);
            driver->setTransform(&worldMtx, 1);
            driver->setState(state, 0);
            driver->setScissor(batch.scissorRect.xmin,
                               batch.scissorRect.ymin,
                               batch.scissorRect.xmax - batch.scissorRect.xmin,
                               batch.scissorRect.ymax - batch.scissorRect.ymin);
            driver->setTransientIndexBufferI(&tib, batch.firstIdx, batch.numIndices);
            driver->setTransientVertexBufferI(0, &tvb, 0, batch.numVerts);       
            driver->submit(viewId, ctx->program, 0, false); 
        }
    }

    bool gfx::initDebugDraw2D(bx::AllocatorI* alloc, GfxDriver* driver)
    {
        assert(driver);
        if (gDebugDraw2D) {
            assert(false);
            return false;
        }

        gDebugDraw2D = BX_NEW(alloc, DebugDraw2DMgr)(alloc);
        if (!gDebugDraw2D)
            return false;

        gDebugDraw2D->driver = driver;
    
        // Load program
        {
            ShaderHandle vertexShader = driver->createShader(driver->makeRef(vg_vso, sizeof(vg_vso), nullptr, nullptr));
            ShaderHandle fragmentShader = driver->createShader(driver->makeRef(vg_fso, sizeof(vg_fso), nullptr, nullptr));
            if (!vertexShader.isValid() || !fragmentShader.isValid()) {
                TEE_ERROR("Creating shaders failed");
                return false;
            }
            gDebugDraw2D->program = driver->createProgram(vertexShader, fragmentShader, true);
            if (!gDebugDraw2D->program.isValid()) {
                TEE_ERROR("Creating GPU program failed");
                return false;
            }
        }

        vgVertexPosCoordColor::init();

        gDebugDraw2D->uTexture = driver->createUniform("u_texture", UniformType::Int1, 1);
        assert(gDebugDraw2D->uTexture.isValid());

        // Create a 1x1 white texture 
        gDebugDraw2D->whiteTexture = getWhiteTexture1x1();
        assert(gDebugDraw2D->whiteTexture.isValid());

        return true;
    }

    void gfx::shutdownDebugDraw2D()
    {
        if (!gDebugDraw2D)
            return;

        if (gDebugDraw2D->program.isValid())
            gDebugDraw2D->driver->destroyProgram(gDebugDraw2D->program);
        if (gDebugDraw2D->uTexture.isValid())
            gDebugDraw2D->driver->destroyUniform(gDebugDraw2D->uTexture);
        BX_DELETE(gDebugDraw2D->alloc, gDebugDraw2D);
        gDebugDraw2D = nullptr;
    }

    DebugDraw2D* gfx::createDebugDraw2D(int maxVerts, int maxBatches)
    {
        assert(gDebugDraw2D);

        bx::AllocatorI* alloc = gDebugDraw2D->alloc;
        DebugDraw2D* ctx = BX_NEW(alloc, DebugDraw2D)(alloc);
        ctx->driver = gDebugDraw2D->driver;

        if (maxVerts == 0)
            maxVerts = MAX_VERTICES;
        if (maxBatches == 0)
            maxBatches = MAX_BATCHES;
        int maxIndices = (maxVerts / 4) * 6;    // Normally we have quads for each 4 batch of verts, so it's 6 indices for each quad
    
        ctx->vertexBuff = (vgVertexPosCoordColor*)BX_ALLOC(alloc, sizeof(vgVertexPosCoordColor)*maxVerts);
        if (!ctx->vertexBuff) {
            gfx::destroyDebugDraw2D(ctx);
            return nullptr;
        }
        ctx->maxVerts = maxVerts;

        ctx->indexBuff = (uint16_t*)BX_ALLOC(alloc, sizeof(uint16_t)*maxIndices);
        if (!ctx->indexBuff) {
            gfx::destroyDebugDraw2D(ctx);
            return nullptr;
        }
        ctx->maxIndices = maxIndices;

        ctx->batches = (Batch*)BX_ALLOC(alloc, sizeof(Batch)*maxBatches);
        if (!ctx->batches) {
            gfx::destroyDebugDraw2D(ctx);
            return nullptr;
        }
        ctx->maxBatches = maxBatches;
    
        LoadFontParams fparams;
        fparams.format = FontFileFormat::Binary;
        ctx->defaultFontHandle = asset::load("font", "fonts/fixedsys.fnt", &fparams);
        if (!ctx->defaultFontHandle.isValid())
            BX_WARN("Default font 'fixedsys' not found. Make sure to set a font to VectorGfxContext before draw");

        if (!ctx->statePool.create(STATE_POOL_SIZE, alloc)) {
            gfx::destroyDebugDraw2D(ctx);
            return nullptr;
        }

        // Push one state into state-stack
        VgState* state = ctx->statePool.newInstance();
        assert(state);
        ctx->stateStack.push(&state->snode);

        ctx->program = gDebugDraw2D->program;
        ctx->uTexture = gDebugDraw2D->uTexture;

        return ctx;
    }

    void gfx::destroyDebugDraw2D(DebugDraw2D* ctx)
    {
        assert(gDebugDraw2D);
        assert(ctx);

        if (!ctx->alloc)
            return;

        if (ctx->defaultFontHandle.isValid())
            asset::unload(ctx->defaultFontHandle);
        if (ctx->batches)
            BX_FREE(ctx->alloc, ctx->batches);
        if (ctx->vertexBuff)
            BX_FREE(ctx->alloc, ctx->vertexBuff);
        if (ctx->indexBuff)
            BX_FREE(ctx->alloc, ctx->indexBuff);
        ctx->statePool.destroy();    

        BX_DELETE(ctx->alloc, ctx);
    }

    void gfx::beginDebugDraw2D(DebugDraw2D* ctx, uint8_t viewId, const irect_t& viewport,
                          const mat4_t* viewMtx, const mat4_t* projMtx)
    {
        assert(ctx);
        if (ctx->readyToDraw)
            return;

        ctx->viewport = viewport;
        gfx::resetDbg2D(ctx);
        ctx->numVerts = 0;
        ctx->numBatches = 0;
        ctx->numIndices = 0;
        ctx->viewId = viewId;
        ctx->readyToDraw = true;
        if (viewMtx)
            ctx->viewMtx = *viewMtx;
        else
            ctx->viewMtx = mat4I();

        if (projMtx)
            ctx->projMtx = *projMtx;
        else
            bx::mtxOrtho(ctx->projMtx.f, 0,
                         float(viewport.xmax - viewport.xmin), 
                         float(viewport.ymax - viewport.ymin), 0, -1.0f, 1.0f, 0 , false);
    }

    void gfx::endDebugDraw2D(DebugDraw2D* ctx)
    {
        if (!ctx->readyToDraw)
            return;

        if (ctx->numBatches)
            drawBatches(ctx);
        ctx->readyToDraw = false;
    }

    void gfx::fontDbg2D(DebugDraw2D* ctx, AssetHandle fontHandle)
    {
        VgState* state;
        ctx->stateStack.peek(&state);
        state->fontHandle = fontHandle.isValid() ? fontHandle : ctx->defaultFontHandle;
    }

    void gfx::textDbg2D(DebugDraw2D* ctx, float x, float y, const char* text)
    {
        if (!ctx->readyToDraw)
            return;
        if (text[0] == 0)
            return;

        VgState* state;
        ctx->stateStack.peek(&state);

        TextParams textParams;
        bx::strCopy(textParams.text, sizeof(textParams.text), text);
        textParams.mtx = state->mtx;
        textParams.scissor = state->scissor;
        textParams.color = tmath::colorPremultiplyAlpha(state->textColor, state->alpha);
        textParams.fontHandle = state->fontHandle;
        textParams.pos = vec2(x, y);

        pushBatch(ctx, &gDebugDraw2D->textHandler, &textParams, sizeof(textParams));
    }

    void gfx::textfDbg2D(DebugDraw2D* ctx, float x, float y, const char* fmt, ...)
    {
        if (!ctx->readyToDraw)
            return;

        char text[MAX_TEXT_SIZE];   text[0] = 0;

        va_list args;
        va_start(args, fmt);
        vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);

        gfx::textDbg2D(ctx, x, y, text);
    }

    void gfx::textDbg2D(DebugDraw2D* ctx, float x, float y, const char* fmt, va_list argList)
    {
        if (!ctx->readyToDraw)
            return;

        char text[MAX_TEXT_SIZE];   text[0] = 0;
        vsnprintf(text, sizeof(text), fmt, argList);
        gfx::textDbg2D(ctx, x, y, text);
    }

    void gfx::rectDbg2D(DebugDraw2D* ctx, float x, float y, float width, float height)
    {
        if (!ctx->readyToDraw)
            return;
        gfx::rectDbg2D(ctx, rectwh(x, y, width, height));
    }

    void gfx::rectDbg2D(DebugDraw2D* ctx, const rect_t& rect)
    {
        if (!ctx->readyToDraw)
            return;
        VgState* state;
        ctx->stateStack.peek(&state);

        RectParams rectParams;
        rectParams.mtx = state->mtx;
        rectParams.scissor = state->scissor;
        rectParams.color = tmath::colorPremultiplyAlpha(state->fillColor, state->alpha);
        rectParams.image = nullptr;
        rectParams.rect = rect;

        pushBatch(ctx, &gDebugDraw2D->rectHandler, &rectParams, sizeof(rectParams));
    }

    void gfx::imageDbg2D(DebugDraw2D* ctx, float x, float y, const Texture* image)
    {
        if (!ctx->readyToDraw)
            return;
        if (!image)
            return;
    
        gfx::imageDbg2D(ctx, rectwh(x, y, image->info.width, image->info.height), image);
    }

    void gfx::lineDbg2D(DebugDraw2D* ctx, const vec2_t& p1, const vec2_t& p2, float lineWidth)
    {
        if (!ctx->readyToDraw)
            return;
        VgState* state;
        ctx->stateStack.peek(&state);
    
        LineParams lineParams;
        lineParams.p1 = p1;
        lineParams.p2 = p2;
        lineParams.width = lineWidth;
        lineParams.color = tmath::colorPremultiplyAlpha(state->strokeColor, state->alpha);
        lineParams.scissor = state->scissor;
        lineParams.mtx = state->mtx;

        pushBatch(ctx, &gDebugDraw2D->lineHandler, &lineParams, sizeof(lineParams));
    }

    void gfx::arrowDbg2D(DebugDraw2D* ctx, const vec2_t& p1, const vec2_t& p2, float lineWidth, float arrowLength)
    {
        if (!ctx->readyToDraw)
            return;
        VgState* state;
        ctx->stateStack.peek(&state);

        LineParams lineParams;

        // main line
        lineParams.p1 = p1;
        lineParams.p2 = p2;
        lineParams.width = lineWidth;
        lineParams.color = tmath::colorPremultiplyAlpha(state->strokeColor, state->alpha);
        lineParams.scissor = state->scissor;
        lineParams.mtx = state->mtx;
        pushBatch(ctx, &gDebugDraw2D->lineHandler, &lineParams, sizeof(lineParams));

        vec2_t d = p2 - p1;
        float totalLen = bx::vec2Length(d.f);
        float t = bx::min(1.0f, arrowLength/totalLen);

        vec2_t arrowStart;
        bx::vec2Lerp(arrowStart.f, p2.f, p1.f, t);
        d = d*(1.0f/totalLen);
        vec2_t normal1 = vec2(-d.y, d.x);
        vec2_t normal2 = vec2(d.y, -d.x);

        // arrow line 1
        lineParams.p1 = arrowStart + normal1*lineWidth*4.0f;
        pushBatch(ctx, &gDebugDraw2D->lineHandler, &lineParams, sizeof(lineParams));

        // arrow line 2
        lineParams.p1 = arrowStart + normal2*lineWidth*4.0f;
        pushBatch(ctx, &gDebugDraw2D->lineHandler, &lineParams, sizeof(lineParams));
    }

    void gfx::imageDbg2D(DebugDraw2D* ctx, const rect_t& rect, const Texture* image)
    {
        if (!ctx->readyToDraw)
            return;
        if (!image)
            return;

        VgState* state;
        ctx->stateStack.peek(&state);

        RectParams rectParams;
        rectParams.mtx = state->mtx;
        rectParams.scissor = state->scissor;
        rectParams.color = tmath::colorPremultiplyAlpha(state->fillColor, state->alpha);
        rectParams.image = image;
        rectParams.rect = rect;

        pushBatch(ctx, &gDebugDraw2D->rectHandler, &rectParams, sizeof(rectParams));
    }

    void gfx::scissorDbg2D(DebugDraw2D* ctx, const irect_t& rect)
    {
        VgState* state;
        ctx->stateStack.peek(&state);
        state->scissor = rect;
    }

    void gfx::alphaDbg2D(DebugDraw2D* ctx, float alpha)
    {
        VgState* state;
        ctx->stateStack.peek(&state);
        state->alpha = alpha;
    }

    void gfx::textColorDbg2D(DebugDraw2D* ctx, ucolor_t color)
    {
        VgState* state;
        ctx->stateStack.peek(&state);
        state->textColor = color;
    }

    void gfx::strokeColorDbg2D(DebugDraw2D* ctx, ucolor_t color)
    {
        VgState* state;
        ctx->stateStack.peek(&state);
        state->strokeColor = color;
    }

    void gfx::fillColorDbg2D(DebugDraw2D* ctx, ucolor_t color)
    {
        VgState* state;
        ctx->stateStack.peek(&state);
        state->fillColor = color;
    }

    void gfx::translateDbg2D(DebugDraw2D* ctx, float x, float y)
    {
        VgState* state;
        ctx->stateStack.peek(&state);
        mat3_t m;
        mat3_t cur = state->mtx;
        bx::mtx3x3Translate(m.f, x, y);
        bx::mtx3x3Mul(state->mtx.f, cur.f, m.f);
    }

    void gfx::scaleDbg2D(DebugDraw2D* ctx, float sx, float sy)
    {
        VgState* state;
        ctx->stateStack.peek(&state);
        mat3_t m;
        mat3_t cur = state->mtx;
        bx::mtx3x3Scale(m.f, sx, sy);
        bx::mtx3x3Mul(state->mtx.f, cur.f, m.f);
    }

    void gfx::rotateDbg2D(DebugDraw2D* ctx, float theta)
    {
        VgState* state;
        ctx->stateStack.peek(&state);
        mat3_t m;
        mat3_t cur = state->mtx;
        bx::mtx3x3Rotate(m.f, theta);
        bx::mtx3x3Mul(state->mtx.f, cur.f, m.f);
    }

    void gfx::resetTransformDbg2D(DebugDraw2D* ctx)
    {
        VgState* state;
        ctx->stateStack.peek(&state);
        state->mtx = mat3I();
    }

    void gfx::pushDbg2D(DebugDraw2D* ctx)
    {
        VgState* curState;
        ctx->stateStack.peek(&curState);

        VgState* s = ctx->statePool.newInstance();
        if (s) {
            ctx->stateStack.push(&s->snode);
            memcpy(s, curState, sizeof(VgState));
        }
    }

    void gfx::popDbg2D(DebugDraw2D* ctx)
    {
        if (ctx->stateStack.getHead()->down) {
            VgState* s;
            ctx->stateStack.pop(&s);
            ctx->statePool.deleteInstance(s);
        }
    }

    void gfx::resetDbg2D(DebugDraw2D* ctx)
    {
        while (ctx->stateStack.getHead()->down) {
            VgState* s;
            ctx->stateStack.pop(&s);
            ctx->statePool.deleteInstance(s);
        }

        VgState* head;
        ctx->stateStack.peek(&head);
        head->setDefault(ctx);
    }

    TextHandler::TextHandler()
    {

    }

    uint32_t TextHandler::getHash(const void* params)
    {
        const TextParams* textParams = (const TextParams*)params;
        return (uint32_t(textParams->fontHandle.value) << 16) | TEXTHANDLER_ID;
    }

    void TextHandler::writePrimitives(DebugDraw2D* ctx, const void* params, vgVertexPosCoordColor* verts, 
                                      int maxVerts, uint16_t* indices, int firstVertIdx, int maxIndices,
                                      int* numVertsWritten, int* numIndicesWritten)
    {
        const TextParams* textParams = (const TextParams*)params;
        Font* font = asset::getObjPtr<Font>(textParams->fontHandle);
        const char* text = textParams->text;
        vec2_t pos = textParams->pos;
        ucolor_t color = textParams->color;

        vec2_t texSize = gfx::getFontTextureSize(font);
        int len = (int)strlen(text);
        int vertexIdx = 0;
        int indexIdx = 0;

        for (int i = 0; i < len && (vertexIdx + 4) <= maxVerts && (indexIdx + 6) <= maxIndices; i++) {
            int gIdx = gfx::findFontCharGlyph(font, text[i]);
            if (gIdx != -1) {
                const FontGlyph& glyph = gfx::getFontGlyph(font, gIdx);

                vgVertexPosCoordColor& v0 = verts[vertexIdx];
                vgVertexPosCoordColor& v1 = verts[vertexIdx + 1];
                vgVertexPosCoordColor& v2 = verts[vertexIdx + 2];
                vgVertexPosCoordColor& v3 = verts[vertexIdx + 3];

                // Top-Left
                v0.x = pos.x + glyph.xoffset;
                v0.y = pos.y + glyph.yoffset;
                v0.tx = glyph.x / texSize.x;
                v0.ty = glyph.y / texSize.y;

                // Top-Right
                v1.x = pos.x + glyph.xoffset + glyph.width;
                v1.y = pos.y + glyph.yoffset;
                v1.tx = (glyph.x + glyph.width) / texSize.x;
                v1.ty = glyph.y / texSize.y;

                // Bottom-Left
                v2.x = pos.x + glyph.xoffset;
                v2.y = pos.y + glyph.yoffset + glyph.height;
                v2.tx = glyph.x / texSize.x;
                v2.ty = (glyph.y + glyph.height) / texSize.y;

                // Bottom-Right
                v3.x = pos.x + glyph.xoffset + glyph.width;
                v3.y = pos.y + glyph.yoffset + glyph.height;
                v3.tx = (glyph.x + glyph.width) / texSize.x;
                v3.ty = (glyph.y + glyph.height) / texSize.y;

                v0.color = v1.color = v2.color = v3.color = color.n;

                // Advance
                pos.x += glyph.xadvance;

                // Kerning
                if (i < len - 1) {
                    int nextIdx = gfx::findFontCharGlyph(font, text[i + 1]);
                    if (nextIdx != -1)
                        pos.x += gfx::getFontGlyphKerning(font, gIdx, nextIdx);
                }

                // Make a quad from 4 verts
                int startVertex = firstVertIdx + vertexIdx;
                indices[indexIdx] = startVertex;
                indices[indexIdx + 1] = startVertex + 1;
                indices[indexIdx + 2] = startVertex + 2;
                indices[indexIdx + 3] = startVertex + 2;
                indices[indexIdx + 4] = startVertex + 1;
                indices[indexIdx + 5] = startVertex + 3;

                //
                vertexIdx += 4;
                indexIdx += 6;
            }
        }

        *numVertsWritten = vertexIdx;
        *numIndicesWritten = indexIdx;
    }

    GfxState::Bits TextHandler::setStates(DebugDraw2D* ctx, GfxDriver* driver, const void* params)
    {
        const TextParams* textParams = (const TextParams*)params;
        Texture* texture = asset::getObjPtr<Texture>(gfx::getFontTexture(asset::getObjPtr<Font>(textParams->fontHandle)));
        driver->setTexture(0, ctx->uTexture, texture->handle, TextureFlag::FromTexture);
        return GfxState::None;
    }

    RectHandler::RectHandler()
    {
    }

    uint32_t RectHandler::getHash(const void* params)
    {
        const RectParams* rectParams = (const RectParams*)params;
        const Texture* texture = rectParams->image;
        return (uint32_t(texture ? texture->handle.value : UINT16_MAX) << 16) | RECTHANDLER_ID;
    }

    void RectHandler::writePrimitives(DebugDraw2D* ctx, const void* params, vgVertexPosCoordColor* verts, int maxVerts, uint16_t* indices, int firstVertIdx, int maxIndices, int* numVertsWritten, int* numIndicesWritten)
    {
        const RectParams* rectParams = (const RectParams*)params;
        ucolor_t color = rectParams->color;
        rect_t rect = rectParams->rect;

        int vertexIdx = 0;
        int indexIdx = 0;

        if ((vertexIdx + 4) <= maxVerts && (indexIdx + 6) <= maxIndices) {
            vgVertexPosCoordColor& v0 = verts[vertexIdx];
            vgVertexPosCoordColor& v1 = verts[vertexIdx + 1];
            vgVertexPosCoordColor& v2 = verts[vertexIdx + 2];
            vgVertexPosCoordColor& v3 = verts[vertexIdx + 3];

            // Top-Left
            v0.x = rect.xmin;               v0.y = rect.ymin;
            v0.tx = 0;                      v0.ty = 0;

            // Top-Right
            v1.x = rect.xmax;               v1.y = rect.ymin;
            v1.tx = 1.0f;                   v1.ty = 0;

            // Bottom-Left
            v2.x = rect.xmin;               v2.y = rect.ymax;
            v2.tx = 0;                      v2.ty = 1.0f;

            // Bottom-Right
            v3.x = rect.xmax;               v3.y = rect.ymax;
            v3.tx = 1.0f;                   v3.ty = 1.0f;

            v0.color = v1.color = v2.color = v3.color = color.n;

            // Make a quad from 4 verts
            int startVertex = firstVertIdx + vertexIdx;
            indices[indexIdx] = startVertex;
            indices[indexIdx + 1] = startVertex + 1;
            indices[indexIdx + 2] = startVertex + 2;
            indices[indexIdx + 3] = startVertex + 2;
            indices[indexIdx + 4] = startVertex + 1;
            indices[indexIdx + 5] = startVertex + 3;

            vertexIdx += 4;
            indexIdx += 6;
        }

        *numVertsWritten = vertexIdx;
        *numIndicesWritten = indexIdx;
    }

    GfxState::Bits RectHandler::setStates(DebugDraw2D* ctx, GfxDriver* driver, const void* params)
    {
        const RectParams* rectParams = (const RectParams*)params;
        const Texture* texture = rectParams->image;
        driver->setTexture(0, ctx->uTexture, texture ? texture->handle : gDebugDraw2D->whiteTexture, TextureFlag::FromTexture);
        return GfxState::None;
    }

    LineHandler::LineHandler()
    {
    }

    uint32_t LineHandler::getHash(const void* params)
    {
        return LINEHANDLER_ID;
    }

    void LineHandler::writePrimitives(DebugDraw2D* ctx, const void* params, vgVertexPosCoordColor* verts, int maxVerts,
                                      uint16_t* indices, int firstVertIdx, int maxIndices, int* numVertsWritten, int* numIndicesWritten)
    {
        const LineParams* lineParams = (const LineParams*)params;
        ucolor_t color = lineParams->color;
        vec2_t p1 = lineParams->p1;
        vec2_t p2 = lineParams->p2;
        float width = lineParams->width;

        int vertexIdx = 0;
        int indexIdx = 0;

        if ((vertexIdx + 4) <= maxVerts && (indexIdx + 6) <= maxIndices) {
            vgVertexPosCoordColor& v0 = verts[vertexIdx];
            vgVertexPosCoordColor& v1 = verts[vertexIdx + 1];
            vgVertexPosCoordColor& v2 = verts[vertexIdx + 2];
            vgVertexPosCoordColor& v3 = verts[vertexIdx + 3];

            vec2_t d = p2 - p1;
            bx::vec2Norm(d.f, d.f);
            vec2_t normal1 = vec2(-d.y, d.x);
            vec2_t normal2 = vec2(d.y, -d.x);

            v0.x = p1.x + normal1.x*width;               v0.y = p1.y + normal1.y*width;
            v0.tx = 0;                      v0.ty = 0;

            v1.x = p2.x + normal1.x*width;               v1.y = p2.y + normal1.y*width;
            v1.tx = 1.0f;                   v1.ty = 0;

            v2.x = p1.x + normal2.x*width;               v2.y = p1.y + normal2.y*width;
            v2.tx = 0;                      v2.ty = 1.0f;

            v3.x = p2.x + normal2.x*width;               v3.y = p2.y + normal2.y*width;
            v3.tx = 1.0f;                   v3.ty = 1.0f;

            v0.color = v1.color = v2.color = v3.color = color.n;

            // Make a quad from 4 verts
            int startVertex = firstVertIdx + vertexIdx;
            indices[indexIdx] = startVertex;
            indices[indexIdx + 1] = startVertex + 1;
            indices[indexIdx + 2] = startVertex + 2;
            indices[indexIdx + 3] = startVertex + 2;
            indices[indexIdx + 4] = startVertex + 1;
            indices[indexIdx + 5] = startVertex + 3;

            vertexIdx += 4;
            indexIdx += 6;
        }

        *numVertsWritten = vertexIdx;
        *numIndicesWritten = indexIdx;
    }

    GfxState::Bits LineHandler::setStates(DebugDraw2D* ctx, GfxDriver* driver, const void* params)
    {
        driver->setTexture(0, ctx->uTexture, gDebugDraw2D->whiteTexture, TextureFlag::FromTexture);
        return GfxState::None;
    }

} // namespace tee
