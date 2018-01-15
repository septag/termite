#include "pch.h"

#include "gfx_driver.h"
#include "gfx_font.h"
#include "gfx_texture.h"
#include "gfx_debugdraw.h"
#include "gfx_debugdraw2d.h"
#include "camera.h"
#include "internal.h"

#include "bx/uint32_t.h"
#include "bxx/pool.h"
#include "bxx/stack.h"

#include <stdarg.h>

#include TEE_MAKE_SHADER_PATH(shaders_h, ddraw.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, ddraw.fso)

#define STATE_POOL_SIZE 8
#define MAX_TEXT_SIZE 256

namespace tee
{
    struct eddVertexPosCoordColor
    {
        float x, y, z;
        float tx, ty;
        uint32_t color;

        void setPos(float _x, float _y, float _z)
        {
            x = _x;     y = _y;     z = _z;
        }

        void setPos(const vec3_t& p)
        {
            x = p.x;    y = p.y;    z = p.z;
        }

        static void init()
        {
            gfx::beginDecl(&Decl);
            gfx::addAttrib(&Decl, VertexAttrib::Position, 3, VertexAttribType::Float);
            gfx::addAttrib(&Decl, VertexAttrib::TexCoord0, 2, VertexAttribType::Float);
            gfx::addAttrib(&Decl, VertexAttrib::Color0, 4, VertexAttribType::Uint8, true);
            gfx::endDecl(&Decl);
        }

        static VertexDecl Decl;
    };
    VertexDecl eddVertexPosCoordColor::Decl;

    class BX_NO_VTABLE DrawHandler
    {
    public:
        virtual bool init(bx::AllocatorI* alloc, GfxDriver* driver) = 0;
        virtual void shutdown() = 0;

        virtual uint32_t getHash(const void* params) = 0;
        virtual GfxState::Bits setStates(DebugDraw2D* ctx, GfxDriver* driver, const void* params) = 0;
    };

    struct DebugDrawState
    {
        typedef bx::Stack<DebugDrawState*>::Node SNode;

        mat4_t mtx;
        vec4_t color;
        float alpha;
        irect_t scissor;
        AssetHandle fontHandle;
        SNode snode;

        DebugDrawState() :
            snode(this)
        {
        }

        void setDefault(DebugDraw* ctx);
    };

    struct DebugDraw
    {
        bx::AllocatorI* alloc;
        GfxDriver* driver;
        uint8_t viewId;
        bx::FixedPool<DebugDrawState> statePool;
        bx::Stack<DebugDrawState*> stateStack;
        irect_t viewport;
        AssetHandle defaultFontHandle;
        bool readyToDraw;
        DebugDraw2D* vgCtx;
        mat4_t billboardMtx;
        mat4_t viewProjMtx;

        DebugDraw(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
            driver = nullptr;
            viewId = 0;
            viewport = irect(0, 0, 0, 0);
            readyToDraw = false;
            vgCtx = nullptr;
            viewProjMtx = mat4I();
            billboardMtx = mat4I();
        }
    };


    struct Shape
    {
        VertexBufferHandle vb;
        uint32_t numVerts;

        Shape()
        {
            numVerts = 0;
        }

        Shape(VertexBufferHandle _vb, uint32_t _numVerts)
        {
            vb = _vb;
            numVerts = _numVerts;
        }
    };

    struct DebugDrawMgr
    {
        GfxDriver* driver;
        bx::AllocatorI* alloc;
        ProgramHandle program;
        TextureHandle whiteTexture;

        UniformHandle uTexture;
        UniformHandle uColor;

        Shape bbShape;
        Shape bsphereShape;
        Shape sphereShape;

        DebugDrawMgr(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
            driver = nullptr;
        }
    };

    static DebugDrawMgr* gDbgDraw = nullptr;

    static bool projectToScreen(vec2_t* result, const vec3_t point, const irect_t& rect, const mat4_t& viewProjMtx)
    {
        float w = float(rect.xmax - rect.xmin);
        float h = float(rect.ymax - rect.ymin);
        float wh = w*0.5f;
        float hh = h*0.5f;

        vec4_t proj;
        bx::vec4MulMtx(proj.f, vec4(point.x, point.y, point.z, 1.0f).f, viewProjMtx.f);
        bx::vec3Mul(proj.f, proj.f, 1.0f / proj.w);     proj.w = 1.0f;

        float x = bx::ffloor(proj.x*wh + wh + 0.5f);
        float y = bx::ffloor(-proj.y*hh + hh + 0.5f);

        // ZCull
        if (proj.z < 0.0f || proj.z > 1.0f)
            return false;

        *result = vec2(x, y);
        return true;
    }

    static Shape createSolidAABB()
    {
        aabb_t box = aabbZero();
        vec3_t pts[8];

        const int numVerts = 36;
        tmath::aabbPushPoint(&box, vec3(-0.5f, -0.5f, -0.5f));
        tmath::aabbPushPoint(&box, vec3(0.5f, 0.5f, 0.5f));
        for (int i = 0; i < 8; i++)
            pts[i] = tmath::aabbGetCorner(box, i);

        eddVertexPosCoordColor* verts = (eddVertexPosCoordColor*)alloca(sizeof(eddVertexPosCoordColor) * numVerts);
        bx::memSet(verts, 0x00, sizeof(eddVertexPosCoordColor)*numVerts);

        // Z-
        verts[0].setPos(pts[0]); verts[1].setPos(pts[2]); verts[2].setPos(pts[3]);
        verts[3].setPos(pts[3]); verts[4].setPos(pts[1]); verts[5].setPos(pts[0]);
        // Z+
        verts[6].setPos(pts[5]); verts[7].setPos(pts[7]); verts[8].setPos(pts[6]);
        verts[9].setPos(pts[6]); verts[10].setPos(pts[4]); verts[11].setPos(pts[5]);
        // X+
        verts[12].setPos(pts[1]); verts[13].setPos(pts[3]); verts[14].setPos(pts[7]);
        verts[15].setPos(pts[7]); verts[16].setPos(pts[5]); verts[17].setPos(pts[1]);
        // X-
        verts[18].setPos(pts[6]); verts[19].setPos(pts[2]); verts[20].setPos(pts[0]);
        verts[21].setPos(pts[0]); verts[22].setPos(pts[4]); verts[23].setPos(pts[6]);
        // Y-
        verts[24].setPos(pts[1]); verts[25].setPos(pts[5]); verts[26].setPos(pts[4]);
        verts[27].setPos(pts[4]); verts[28].setPos(pts[0]); verts[29].setPos(pts[1]);
        // Y+
        verts[30].setPos(pts[3]); verts[31].setPos(pts[2]); verts[32].setPos(pts[6]);
        verts[33].setPos(pts[6]); verts[34].setPos(pts[7]); verts[35].setPos(pts[3]);

        for (int i = 0; i < numVerts; i++)
            verts[i].color = 0xffffffff;

        return Shape(gDbgDraw->driver->createVertexBuffer(gDbgDraw->driver->copy(verts, sizeof(eddVertexPosCoordColor) * numVerts),
                                                       eddVertexPosCoordColor::Decl, GfxBufferFlag::None), numVerts);
    }

    static Shape createAABB()
    {
        aabb_t box = aabbZero();
        vec3_t pts[8];

        const int numVerts = 24;
        tmath::aabbPushPoint(&box, vec3(-0.5f, -0.5f, -0.5f));
        tmath::aabbPushPoint(&box, vec3(0.5f, 0.5f, 0.5f));
        for (int i = 0; i < 8; i++)
            pts[i] = tmath::aabbGetCorner(box, i);

        eddVertexPosCoordColor* verts = (eddVertexPosCoordColor*)alloca(sizeof(eddVertexPosCoordColor) * numVerts);
        bx::memSet(verts, 0x00, sizeof(eddVertexPosCoordColor)*numVerts);

        // Bottom edges
        verts[0].setPos(pts[0]);    verts[1].setPos(pts[1]);
        verts[2].setPos(pts[1]);    verts[3].setPos(pts[5]);
        verts[4].setPos(pts[5]);    verts[5].setPos(pts[4]);
        verts[6].setPos(pts[4]);    verts[7].setPos(pts[0]);

        // Middle edges
        verts[8].setPos(pts[0]);    verts[9].setPos(pts[2]);
        verts[10].setPos(pts[1]);   verts[11].setPos(pts[3]);
        verts[12].setPos(pts[5]);   verts[13].setPos(pts[7]);
        verts[14].setPos(pts[4]);   verts[15].setPos(pts[6]);

        // Top edges
        verts[16].setPos(pts[2]);   verts[17].setPos(pts[3]);
        verts[18].setPos(pts[3]);   verts[19].setPos(pts[7]);
        verts[20].setPos(pts[7]);   verts[21].setPos(pts[6]);
        verts[22].setPos(pts[6]);   verts[23].setPos(pts[2]);

        for (int i = 0; i < numVerts; i++)
            verts[i].color = 0xffffffff;

        return Shape(gDbgDraw->driver->createVertexBuffer(gDbgDraw->driver->copy(verts, sizeof(eddVertexPosCoordColor) * numVerts),
                                                       eddVertexPosCoordColor::Decl, GfxBufferFlag::None), numVerts);
    }


    static Shape createBoundingSphere(int numSegs)
    {
        numSegs = bx::uint32_iclamp(numSegs, 4, 35);
        const int numVerts = numSegs * 2;

        // Create buffer and fill it with data
        int size = numVerts * sizeof(eddVertexPosCoordColor);
        eddVertexPosCoordColor* verts = (eddVertexPosCoordColor*)alloca(size);
        assert(verts);
        bx::memSet(verts, 0x00, size);

        const float dt = bx::kPi2 / float(numSegs);
        float theta = 0.0f;
        int idx = 0;

        // Circle on the XY plane (center = (0, 0, 0), radius = 1)
        for (int i = 0; i < numSegs; i++) {
            verts[idx].setPos(bx::fcos(theta), bx::fsin(theta), 0);
            verts[idx + 1].setPos(bx::fcos(theta + dt), bx::fsin(theta + dt), 0);
            idx += 2;
            theta += dt;
        }

        for (int i = 0; i < numVerts; i++)
            verts[i].color = 0xffffffff;

        return Shape(gDbgDraw->driver->createVertexBuffer(
            gDbgDraw->driver->copy(verts, size), eddVertexPosCoordColor::Decl, GfxBufferFlag::None),
            numVerts);
    }

    static Shape createSphere(int numSegsX, int numSegsY)
    {
        // in solid sphere we have horozontal segments and vertical segments horizontal
        numSegsX = bx::uint32_iclamp(numSegsX, 4, 30);
        numSegsY = bx::uint32_iclamp(numSegsY, 3, 30);
        if (numSegsX % 2 != 0)
            numSegsX++;        // horozontal must be even number 
        if (numSegsY % 2 == 0)
            numSegsY++;        // vertical must be odd number
        const int numVerts = numSegsX * 3 * 2 + (numSegsY - 3) * 3 * 2 * numSegsX;

        /* set extreme points (radius = 1.0f) */
        vec3_t y_max = vec3(0.0f, 1.0f, 0.0f);
        vec3_t y_min = vec3(0.0f, -1.0f, 0.0f);

        // start from lower extreme point and draw slice of circles
        // connect them to the lower level
        // if we are on the last level (i == numIter-1) connect to upper extreme
        // else just make triangles connected to lower level
        int numIter = numSegsY - 1;
        int idx = 0;
        int lower_idx = idx;
        int delta_idx;
        float r;

        // Phi: vertical angle 
        float delta_phi = bx::kPi / (float)numIter;
        float phi = -bx::kPiHalf + delta_phi;

        // Theta: horizontal angle 
        float delta_theta = (bx::kPi*2.0f) / (float)numSegsX;
        float theta = 0.0f;
        float y;

        /* create buffer and fill it with data */
        int size = numVerts * sizeof(eddVertexPosCoordColor);
        eddVertexPosCoordColor *verts = (eddVertexPosCoordColor*)alloca(size);
        if (verts == nullptr)
            return Shape();
        bx::memSet(verts, 0x00, size);

        for (int i = 0; i < numIter; i++) {
            /* calculate z and slice radius */
            r = bx::fcos(phi);
            y = bx::fsin(phi);
            phi += delta_phi;

            /* normal drawing (quad between upper level and lower level) */
            if (i != 0 && i != numIter - 1) {
                theta = 0.0f;
                for (int k = 0; k < numSegsX; k++) {
                    /* current level verts */
                    verts[idx].setPos(r*bx::fcos(theta), y, r*bx::fsin(theta));
                    verts[idx + 1].setPos(r*bx::fcos(theta + delta_theta), y, r*bx::fsin(theta + delta_theta));
                    verts[idx + 2].setPos(verts[lower_idx].x, verts[lower_idx].y, verts[lower_idx].z);
                    verts[idx + 3].setPos(verts[idx + 1].x, verts[idx + 1].y, verts[idx + 1].z);
                    verts[idx + 4].setPos(verts[lower_idx + 1].x, verts[lower_idx + 1].y, verts[lower_idx + 1].z);
                    verts[idx + 5].setPos(verts[lower_idx].x, verts[lower_idx].y, verts[lower_idx].z);

                    idx += 6;
                    theta += delta_theta;
                    lower_idx += delta_idx;
                }
                delta_idx = 6;
                continue;
            }

            /* lower cap */
            if (i == 0) {
                theta = 0.0f;
                lower_idx = idx;
                delta_idx = 3;
                for (int k = 0; k < numSegsX; k++) {
                    verts[idx].setPos(r*bx::fcos(theta), y, r*bx::fsin(theta));
                    verts[idx + 1].setPos(r*bx::fcos(theta + delta_theta), y, r*bx::fsin(theta + delta_theta));
                    verts[idx + 2].setPos(y_min);
                    idx += delta_idx;
                    theta += delta_theta;
                }
            }

            /* higher cap */
            if (i == numIter - 1) {
                for (int k = 0; k < numSegsX; k++) {
                    verts[idx].setPos(y_max);
                    verts[idx + 1].setPos(verts[lower_idx + 1].x, verts[lower_idx + 1].y, verts[lower_idx + 1].z);
                    verts[idx + 2].setPos(verts[lower_idx].x, verts[lower_idx].y, verts[lower_idx].z);
                    idx += 3;
                    theta += delta_theta;
                    lower_idx += delta_idx;
                }
            }
        }

        for (int i = 0; i < numVerts; i++)
            verts[i].color = 0xffffffff;

        return Shape(gDbgDraw->driver->createVertexBuffer(
            gDbgDraw->driver->copy(verts, size), eddVertexPosCoordColor::Decl, GfxBufferFlag::None),
            numVerts);
    }

    bool gfx::initDebugDraw(bx::AllocatorI* alloc, GfxDriver* driver)
    {
        if (gDbgDraw) {
            assert(false);
            return false;
        }

        gDbgDraw = BX_NEW(alloc, DebugDrawMgr)(alloc);
        if (!gDbgDraw)
            return false;
        gDbgDraw->alloc = alloc;
        gDbgDraw->driver = driver;
        gDbgDraw->whiteTexture = getWhiteTexture1x1();
        if (!gDbgDraw->whiteTexture.isValid())
            return false;

        // Load program
        {
            ShaderHandle vertexShader = driver->createShader(driver->makeRef(ddraw_vso, sizeof(ddraw_vso), nullptr, nullptr));
            ShaderHandle fragmentShader = driver->createShader(driver->makeRef(ddraw_fso, sizeof(ddraw_fso), nullptr, nullptr));
            if (!vertexShader.isValid() || !fragmentShader.isValid()) {
                TEE_ERROR("Creating shaders failed");
                return false;
            }
            gDbgDraw->program = driver->createProgram(vertexShader, fragmentShader, true);
            if (!gDbgDraw->program.isValid()) {
                TEE_ERROR("Creating GPU program failed");
                return false;
            }
        }

        eddVertexPosCoordColor::init();

        gDbgDraw->uTexture = driver->createUniform("u_texture", UniformType::Int1, 1);
        assert(gDbgDraw->uTexture.isValid());
        gDbgDraw->uColor = driver->createUniform("u_color", UniformType::Vec4, 1);
        assert(gDbgDraw->uColor.isValid());

        // Create a 1x1 white texture 
        gDbgDraw->whiteTexture = getWhiteTexture1x1();
        assert(gDbgDraw->whiteTexture.isValid());

        gDbgDraw->bbShape = createAABB();
        gDbgDraw->bsphereShape = createBoundingSphere(30);
        gDbgDraw->sphereShape = createSphere(12, 9);

        return true;
    }

    void gfx::shutdownDebugDraw()
    {
        if (!gDbgDraw)
            return;
        GfxDriver* driver = gDbgDraw->driver;

        if (gDbgDraw->bbShape.vb.isValid())
            driver->destroyVertexBuffer(gDbgDraw->bbShape.vb);
        if (gDbgDraw->sphereShape.vb.isValid())
            driver->destroyVertexBuffer(gDbgDraw->sphereShape.vb);
        if (gDbgDraw->bsphereShape.vb.isValid())
            driver->destroyVertexBuffer(gDbgDraw->bsphereShape.vb);

        if (gDbgDraw->uColor.isValid())
            gDbgDraw->driver->destroyUniform(gDbgDraw->uColor);
        if (gDbgDraw->program.isValid())
            gDbgDraw->driver->destroyProgram(gDbgDraw->program);
        if (gDbgDraw->uTexture.isValid())
            gDbgDraw->driver->destroyUniform(gDbgDraw->uTexture);
        BX_DELETE(gDbgDraw->alloc, gDbgDraw);
        gDbgDraw = nullptr;
    }

    DebugDraw* gfx::createDebugDraw()
    {
        assert(gDbgDraw);

        bx::AllocatorI* alloc = gDbgDraw->alloc;
        DebugDraw* ctx = BX_NEW(alloc, DebugDraw)(alloc);
        ctx->driver = gDbgDraw->driver;

        LoadFontParams fparams;
        fparams.format = FontFileFormat::Binary;
        ctx->defaultFontHandle = asset::load("font", "fonts/fixedsys.fnt", &fparams);
        if (!ctx->defaultFontHandle.isValid()) {
            gfx::destroyDebugDraw(ctx);
            return nullptr;
        }

        if (!ctx->statePool.create(STATE_POOL_SIZE, alloc)) {
            gfx::destroyDebugDraw(ctx);
            return nullptr;
        }

        // Push one state into state-stack
        DebugDrawState* state = ctx->statePool.newInstance();
        ctx->stateStack.push(&state->snode);

        return ctx;
    }

    void gfx::destroyDebugDraw(DebugDraw* ctx)
    {
        assert(gDbgDraw);
        assert(ctx);

        if (!ctx->alloc)
            return;

        if (ctx->defaultFontHandle.isValid())
            asset::unload(ctx->defaultFontHandle);

        ctx->statePool.destroy();
        BX_DELETE(ctx->alloc, ctx);
    }

    void gfx::beginDebugDraw(DebugDraw* ctx, uint8_t viewId, const irect_t& viewport,
                             const mat4_t& viewMtx, const mat4_t& projMtx, DebugDraw2D* vg)
    {
        assert(ctx);
        gfx::resetDbg(ctx);
        ctx->viewId = viewId;
        ctx->vgCtx = vg;
        ctx->viewport = viewport;
        ctx->readyToDraw = true;

        bx::mtxMul(ctx->viewProjMtx.f, viewMtx.f, projMtx.f);
        ctx->billboardMtx = mat4(viewMtx.m11, viewMtx.m21, viewMtx.m31,
                                     viewMtx.m12, viewMtx.m22, viewMtx.m32,
                                     viewMtx.m13, viewMtx.m23, viewMtx.m33,
                                     0.0f, 0.0f, 0.0f);

        if (vg) {
            gfx::beginDebugDraw2D(ctx->vgCtx, viewId + 1, viewport);
        }

        GfxDriver* driver = ctx->driver;
        driver->setViewRect(viewId, viewport.xmin, viewport.ymin, viewport.xmax-viewport.xmin, viewport.ymax-viewport.ymin);
        driver->setViewTransform(viewId, viewMtx.f, projMtx.f, GfxViewFlag::Stereo, nullptr);
    }

    void gfx::endDebugDraw(DebugDraw* ctx)
    {
        if (ctx->vgCtx)
            gfx::endDebugDraw2D(ctx->vgCtx);
        ctx->readyToDraw = false;
    }

    void gfx::textDbg(DebugDraw* ctx, const vec3_t pos, const char* text)
    {
        if (ctx->vgCtx) {
            vec2_t screenPt;
            if (projectToScreen(&screenPt, pos, ctx->viewport, ctx->viewProjMtx)) {
                DebugDrawState* state;
                ctx->stateStack.peek(&state);

                gfx::fontDbg2D(ctx->vgCtx, state->fontHandle);
                vec4_t c = state->color;
                gfx::textColorDbg2D(ctx->vgCtx, ucolorf(c.x, c.y, c.z, c.w));
                gfx::textDbg2D(ctx->vgCtx, screenPt.x, screenPt.y, text);
            }
        }
    }

    void gfx::textfDbg(DebugDraw* ctx, const vec3_t pos, const char* fmt, ...)
    {
        if (ctx->vgCtx) {
            char text[MAX_TEXT_SIZE];   text[0] = 0;

            va_list args;
            va_start(args, fmt);
            vsnprintf(text, sizeof(text), fmt, args);
            va_end(args);

            gfx::textDbg(ctx, pos, text);
        }
    }

    TEE_API void gfx::textDbg(DebugDraw* ctx, const vec3_t pos, const char* fmt, va_list argList)
    {
        if (ctx->vgCtx) {
            char text[MAX_TEXT_SIZE];   text[0] = 0;
            vsnprintf(text, sizeof(text), fmt, argList);
            gfx::textDbg(ctx, pos, text);
        }
    }

    void gfx::imageDbg(DebugDraw* ctx, const vec3_t pos, Texture* image)
    {
        if (ctx->vgCtx) {
            vec2_t screenPt;
            if (projectToScreen(&screenPt, pos, ctx->viewport, ctx->viewProjMtx)) {
                DebugDrawState* state;
                ctx->stateStack.peek(&state);
                vec4_t c = state->color;
                gfx::fillColorDbg2D(ctx->vgCtx, ucolorf(c.x, c.y, c.z, c.w));
                gfx::imageDbg2D(ctx->vgCtx, screenPt.x, screenPt.y, image);
            }
        }
    }

    void gfx::rectDbg(DebugDraw* ctx, const vec3_t& vmin, const vec3_t& vmax)
    {
        if (ctx->vgCtx) {
            vec2_t minPt, maxPt;
            if (projectToScreen(&minPt, vmin, ctx->viewport, ctx->viewProjMtx) &&
                projectToScreen(&maxPt, vmax, ctx->viewport, ctx->viewProjMtx)) {
                DebugDrawState* state;
                ctx->stateStack.peek(&state);
                vec4_t c = state->color;
                gfx::fillColorDbg2D(ctx->vgCtx, ucolorf(c.x, c.y, c.z, c.w));
                gfx::rectDbg2D(ctx->vgCtx, rect(minPt, maxPt));
            }
        }
    }

    void gfx::lineDbg(DebugDraw* ctx, const vec3_t& startPt, const vec3_t& endPt, const mat4_t* modelMtx /*= nullptr*/)
    {
        assert(0);
    }

    void gfx::circleDbg(DebugDraw* ctx, const vec3_t& pos, float radius, const mat4_t* modelMtx /*= nullptr*/, bool showDir /*= false*/)
    {
        assert(0);
    }

    void gfx::rectDbg(DebugDraw* ctx, const vec3_t& minpt, const vec3_t& maxpt, const mat4_t* modelMtx /*= nullptr*/)
    {
        assert(0);
    }

    void gfx::xzGridDbg(DebugDraw* ctx, const Camera& cam, float spacing, float boldSpacing, float maxDepth,
                        ucolor_t color, ucolor_t boldColor)
    {
        spacing = bx::fceil(bx::clamp(spacing, 1.0f, 20.0f));

        vec3_t corners[8];
        float ratio = float(ctx->viewport.xmax - ctx->viewport.xmin) / float(ctx->viewport.ymax - ctx->viewport.ymin);
        cam.calcFrustumCorners(corners, ratio, -2.0f, bx::min(maxDepth, cam.ffar));

        mat4_t projToXz;
        tmath::mtxProjPlane(&projToXz, vec3(0, 1.0f, 0));

        // project frustum corners to XZ plane add them to bounding box
        aabb_t bb = aabbZero();
        for (int i = 0; i < 8; i++) {
            vec3_t tmp;
            bx::vec3MulMtx(tmp.f, corners[i].f, projToXz.f);
            tmath::aabbPushPoint(&bb, tmp);
        }

        // Snap grid bounds to 'spacing'
        // Example: spacing = 5, snap bounds to -5, 0, 5, ...
        int nspace = (int)spacing;
        vec3_t& minpt = bb.vmin;
        vec3_t& maxpt = bb.vmax;
        aabb_t snapbox = aabb(float(int(minpt.x) - int(minpt.x) % nspace),
                               0,
                               float(int(minpt.z) - int(minpt.z) % nspace),
                               float(int(maxpt.x) - int(maxpt.x) % nspace),
                               0,
                               float(int(maxpt.z) - int(maxpt.z) % nspace));
        float w = snapbox.xmax - snapbox.xmin;
        float d = snapbox.zmax - snapbox.zmin;
        if (bx::fequal(w, 0, 0.00001f) || bx::fequal(d, 0, 0.00001f))
            return;

        int xlines = int(w) / nspace + 1;
        int ylines = int(d) / nspace + 1;
        int numVerts = (xlines + ylines) * 2;

        // Draw
        GfxDriver* driver = ctx->driver;
        if (driver->getAvailTransientVertexBuffer(numVerts, eddVertexPosCoordColor::Decl) != numVerts)
            return;
        TransientVertexBuffer tvb;
        driver->allocTransientVertexBuffer(&tvb, numVerts, eddVertexPosCoordColor::Decl);
        eddVertexPosCoordColor* verts = (eddVertexPosCoordColor*)tvb.data;

        DebugDrawState* state;
        ctx->stateStack.peek(&state);

        int i = 0;
        for (float zoffset = snapbox.zmin; zoffset <= snapbox.zmax; zoffset += spacing, i += 2) {
            verts[i].x = snapbox.xmin;
            verts[i].y = 0;
            verts[i].z = zoffset;

            int ni = i + 1;
            verts[ni].x = snapbox.xmax;
            verts[ni].y = 0;
            verts[ni].z = zoffset;

            verts[i].color = verts[ni].color = !bx::fequal(bx::fmod(zoffset, boldSpacing), 0.0f, 0.0001f) ? color.n : boldColor.n;
        }

        for (float xoffset = snapbox.xmin; xoffset <= snapbox.xmax; xoffset += spacing, i += 2) {
            verts[i].x = xoffset;
            verts[i].y = 0;
            verts[i].z = snapbox.zmin;

            int ni = i + 1;
            verts[ni].x = xoffset;
            verts[ni].y = 0;
            verts[ni].z = snapbox.zmax;

            verts[i].color = verts[ni].color = !bx::fequal(bx::fmod(xoffset, boldSpacing), 0.0f, 0.0001f) ? color.n : boldColor.n;
        }

        mat4_t ident = mat4I();

        driver->setTransientVertexBuffer(0, &tvb);
        driver->setTransform(ident.f, 1);
        driver->setState(GfxState::RGBWrite | GfxState::DepthTestLess | GfxState::PrimitiveLines | GfxState::DepthWrite, 0);
        driver->setUniform(gDbgDraw->uColor, state->color.f, 1);
        driver->setTexture(0, gDbgDraw->uTexture, gDbgDraw->whiteTexture, TextureFlag::FromTexture);
        driver->submit(ctx->viewId, gDbgDraw->program, 0, false);
    }

    void gfx::xyGridDbg(DebugDraw* ctx, const Camera2D& cam, float spacing, float boldSpacing,
                        ucolor_t color, ucolor_t boldColor, bool showVerticalInfo)
    {
        spacing = bx::fceil(bx::clamp(spacing, 1.0f, 20.0f));

        rect_t rc = cam.getRect();

        // Snap grid bounds to 'spacing'
        // Example: spacing = 5, snap bounds to -5, 0, 5, ...
        int nspace = (int)spacing;

        vec2_t& minpt = rc.vmin;
        vec2_t& maxpt = rc.vmax;
        rect_t snapRect = rect(float(int(minpt.x) - int(minpt.x) % nspace) - spacing,
                                float(int(minpt.y) - int(minpt.y) % nspace) - spacing,
                                float(int(maxpt.x) - int(maxpt.x) % nspace) + spacing,
                                float(int(maxpt.y) - int(maxpt.y) % nspace) + spacing);

        float w = snapRect.xmax - snapRect.xmin;
        float h = snapRect.ymax - snapRect.ymin;
        if (bx::fequal(w, 0, 0.00001f) || bx::fequal(h, 0, 0.00001f))
            return;

        int xlines = int(w) / nspace + 1;
        int ylines = int(h) / nspace + 1;
        int numVerts = (xlines + ylines) * 2;

        // Draw
        GfxDriver* driver = ctx->driver;
        if (driver->getAvailTransientVertexBuffer(numVerts, eddVertexPosCoordColor::Decl) != numVerts)
            return;
        TransientVertexBuffer tvb;
        driver->allocTransientVertexBuffer(&tvb, numVerts, eddVertexPosCoordColor::Decl);
        eddVertexPosCoordColor* verts = (eddVertexPosCoordColor*)tvb.data;

        DebugDrawState* state;
        ctx->stateStack.peek(&state);

        int i = 0;

        float fontHH = 0;
        if (showVerticalInfo) {
            DebugDrawState* state;
            ctx->stateStack.peek(&state);

            gfx::fontDbg2D(ctx->vgCtx, state->fontHandle);
            vec4_t c = state->color;
            ucolor_t color = ucolorf(c.x, c.y, c.z, c.w);
            gfx::textColorDbg2D(ctx->vgCtx, color);
            fontHH = bx::ffloor(getFontLineHeight(asset::getObjPtr<Font>(state->fontHandle)) * 0.5f);
        }

        // Horizontal Lines
        for (float yoffset = snapRect.ymin; yoffset <= snapRect.ymax; yoffset += spacing, i += 2) {
            verts[i].x = snapRect.xmin;
            verts[i].y = yoffset;
            verts[i].z = 0;

            int ni = i + 1;
            verts[ni].x = snapRect.xmax;
            verts[ni].y = yoffset;
            verts[ni].z = 0;

            verts[i].tx = verts[i].ty = verts[ni].tx = verts[ni].ty = 0;
            if (!bx::fequal(bx::fmod(yoffset, boldSpacing), 0.0f, 0.0001f)) {
                verts[i].color = verts[ni].color = color.n;
            } else {
                verts[i].color = verts[ni].color = boldColor.n;
                if (showVerticalInfo) {
                    vec2_t screenPt;
                    projectToScreen(&screenPt, vec3(snapRect.xmin + spacing, yoffset, 0), ctx->viewport, ctx->viewProjMtx);
                    gfx::textfDbg2D(ctx->vgCtx, screenPt.x, screenPt.y - fontHH, "%.1f", yoffset);
                }
            }
        }

        // Vertical Lines
        for (float xoffset = snapRect.xmin; xoffset <= snapRect.xmax; xoffset += spacing, i += 2) {
            verts[i].x = xoffset;
            verts[i].y = snapRect.ymin;
            verts[i].z = 0;

            int ni = i + 1;
            verts[ni].x = xoffset;
            verts[ni].y = snapRect.ymax;
            verts[ni].z = 0;

            verts[i].tx = verts[i].ty = verts[ni].tx = verts[ni].ty = 0;
            verts[i].color = verts[ni].color = !bx::fequal(bx::fmod(xoffset, boldSpacing), 0.0f, 0.0001f) ? color.n : boldColor.n;
        }

        mat4_t ident = mat4I();

        driver->setTransientVertexBuffer(0, &tvb);
        driver->setTransform(ident.f, 1);
        driver->setState(GfxState::RGBWrite | GfxState::PrimitiveLines | gfx::stateBlendAlpha(), 0);
        driver->setUniform(gDbgDraw->uColor, state->color.f, 1);
        driver->setTexture(0, gDbgDraw->uTexture, gDbgDraw->whiteTexture, TextureFlag::FromTexture);
        driver->submit(ctx->viewId, gDbgDraw->program, 0, false);
    }

    void gfx::bboxDbg(DebugDraw* ctx, const aabb_t bb, bool showInfo /*= false*/)
    {
        vec3_t center = (bb.vmin + bb.vmax)*0.5f;
        float w = bb.vmax.x - bb.vmin.x;
        float h = bb.vmax.y - bb.vmin.y;
        float d = bb.vmax.z - bb.vmin.z;

        mat4_t mtx;
        bx::mtxSRT(mtx.f,
                   w, h, d,
                   0, 0, 0,
                   center.x, center.y, center.z);

        Shape shape = gDbgDraw->bbShape;
        DebugDrawState* state;
        ctx->stateStack.peek(&state);

        GfxDriver* driver = gDbgDraw->driver;
        driver->setVertexBuffer(0, shape.vb);
        driver->setTransform(mtx.f, 1);
        driver->setUniform(gDbgDraw->uColor, state->color.f, 1);
        driver->setState(GfxState::RGBWrite | GfxState::DepthTestLess | GfxState::PrimitiveLines, 0);
        driver->setTexture(0, gDbgDraw->uTexture, gDbgDraw->whiteTexture, TextureFlag::FromTexture);
        driver->submit(ctx->viewId, gDbgDraw->program, 0, false);

        if (showInfo) {
            vec2_t center2d;
            if (projectToScreen(&center2d, center, ctx->viewport, ctx->viewProjMtx)) {
                DebugDrawState* state;
                ctx->stateStack.peek(&state);

                gfx::fontDbg2D(ctx->vgCtx, state->fontHandle);
                vec4_t c = state->color;
                ucolor_t color = ucolorf(c.x, c.y, c.z, c.w);
                gfx::textColorDbg2D(ctx->vgCtx, color);
                gfx::fillColorDbg2D(ctx->vgCtx, color);
                gfx::rectDbg2D(ctx->vgCtx, rectwh(center2d.x - 5, center2d.y - 5, 10, 10));
                gfx::textfDbg2D(ctx->vgCtx, center2d.x, center2d.y, "aabb(%.1f, %.1f, %.1f)", w, h, d);
            }
        }
    }

    void gfx::bsphereDbg(DebugDraw* ctx, const sphere_t sphere, bool showInfo /*= false*/)
    {
        // translate by sphere center, and resize by sphere radius
        // combine with billboard mtx to face the camera
        mat4_t mtx;
        bx::mtxSRT(mtx.f,
                   sphere.r, sphere.r, sphere.r,
                   0, 0, 0,
                   sphere.x, sphere.y, sphere.z);
        mtx = ctx->billboardMtx * mtx;

        DebugDrawState* state;
        ctx->stateStack.peek(&state);

        GfxDriver* driver = gDbgDraw->driver;
        Shape shape = gDbgDraw->bsphereShape;
        driver->setVertexBuffer(0, shape.vb);
        driver->setTransform(mtx.f, 1);
        driver->setUniform(gDbgDraw->uColor, state->color.f, 1);
        driver->setState(GfxState::RGBWrite | GfxState::DepthTestLess | GfxState::PrimitiveLines, 0);
        driver->setTexture(0, gDbgDraw->uTexture, gDbgDraw->whiteTexture, TextureFlag::FromTexture);
        driver->submit(ctx->viewId, gDbgDraw->program, 0, false);

        if (showInfo) {
            vec2_t center2d;
            if (projectToScreen(&center2d, sphere.center, ctx->viewport, ctx->viewProjMtx)) {
                gfx::fontDbg2D(ctx->vgCtx, state->fontHandle);
                vec4_t c = state->color;
                ucolor_t color = ucolorf(c.x, c.y, c.z, c.w);
                gfx::textColorDbg2D(ctx->vgCtx, color);
                gfx::fillColorDbg2D(ctx->vgCtx, color);
                gfx::rectDbg2D(ctx->vgCtx, rectwh(center2d.x - 5, center2d.y - 5, 10, 10));
                gfx::textfDbg2D(ctx->vgCtx, center2d.x, center2d.y, "sphere(%.1f, %.1f, %.1f, %.1f)", sphere.x, sphere.y, sphere.z, sphere.r);
            }
        }
    }

    void gfx::fontDbg(DebugDraw* ctx, AssetHandle fontHandle)
    {
        DebugDrawState* state;
        ctx->stateStack.peek(&state);

        state->fontHandle = fontHandle.isValid() ? fontHandle : ctx->defaultFontHandle;
    }

    void gfx::alphaDbg(DebugDraw* ctx, float alpha)
    {
        DebugDrawState* state;
        ctx->stateStack.peek(&state);

        state->alpha = alpha;
    }

    void gfx::colorDbg(DebugDraw* ctx, const vec4_t& color)
    {
        DebugDrawState* state;
        ctx->stateStack.peek(&state);

        state->color = color;
    }

    void gfx::transformDbg(DebugDraw* ctx, const mat4_t& mtx)
    {
        DebugDrawState* state;
        ctx->stateStack.peek(&state);

        state->mtx = mtx;
    }

    void gfx::pushDbg(DebugDraw* ctx)
    {
        DebugDrawState* curState;
        ctx->stateStack.peek(&curState);

        DebugDrawState* s = ctx->statePool.newInstance();
        if (s) {
            ctx->stateStack.push(&s->snode);
            memcpy(s, curState, sizeof(DebugDrawState));
        }
    }

    void gfx::popDbg(DebugDraw* ctx)
    {
        if (ctx->stateStack.getHead()->down) {
            DebugDrawState* s;
            ctx->stateStack.pop(&s);
            ctx->statePool.deleteInstance(s);
        }
    }


    void DebugDrawState::setDefault(DebugDraw* ctx)
    {
        mtx = mat4I();
        color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        alpha = 1.0f;
        scissor = ctx->viewport;
        fontHandle = ctx->defaultFontHandle;
    }

    void gfx::resetDbg(DebugDraw* ctx)
    {
        while (ctx->stateStack.getHead()->down) {
            DebugDrawState* s;
            ctx->stateStack.pop(&s);
            ctx->statePool.deleteInstance(s);
        }
        DebugDrawState* head;
        ctx->stateStack.peek(&head);
        head->setDefault(ctx);
    }
}   // namespace tee