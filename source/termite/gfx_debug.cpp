#include "pch.h"

#include "gfx_driver.h"
#include "gfx_font.h"
#include "gfx_texture.h"
#include "gfx_debug.h"
#include "gfx_vg.h"
#include "camera.h"

#include "bxx/pool.h"
#include "bxx/stack.h"

#include <cstdarg>

#include "shaders_h/dbg.vso"
#include "shaders_h/dbg.fso"

#define STATE_POOL_SIZE 8
#define MAX_TEXT_SIZE 256

using namespace termite;

struct dbgVertexPosCoordColor
{
    float x, y, z;
    float tx, ty;
    color_t color;

    static void init()
    {
        Decl.begin()
            .add(gfxAttrib::Position, 3, gfxAttribType::Float)
            .add(gfxAttrib::TexCoord0, 2, gfxAttribType::Float)
            .add(gfxAttrib::Color0, 4, gfxAttribType::Uint8, true)
            .end();
    }

    static gfxVertexDecl Decl;
};
gfxVertexDecl dbgVertexPosCoordColor::Decl;

class BX_NO_VTABLE DrawHandler
{
public:
    virtual result_t init(bx::AllocatorI* alloc, gfxDriverI* driver) = 0;
    virtual void shutdown() = 0;

    virtual uint32_t getHash(const void* params) = 0;
    virtual gfxState setStates(vgContext* ctx, gfxDriverI* driver, const void* params) = 0;
};

class GridHandler : public DrawHandler
{
public:
    GridHandler();
    
    result_t init(bx::AllocatorI* alloc, gfxDriverI* driver) override;
    void shutdown() override;

    uint32_t getHash(const void* params) override;
    gfxState setStates(vgContext* ctx, gfxDriverI* driver, const void* params) override;

private:
    dbgVertexPosCoordColor* m_verts;

};

GridHandler::GridHandler()
{
    m_verts = nullptr;
}

result_t GridHandler::init(bx::AllocatorI* alloc, gfxDriverI* driver)
{
    return T_ERR_FAILED;
}

void GridHandler::shutdown()
{

}

uint32_t GridHandler::getHash(const void* params)
{
    return 0;
}

gfxState GridHandler::setStates(vgContext* ctx, gfxDriverI* driver, const void* params)
{
    return gfxState::None;
}

struct State
{
    mtx4x4_t mtx;
    color_t color;
    float alpha;
    rect_t scissor;
    const fntFont* font;

    typedef bx::StackNode<State*> SNode;
    SNode snode;
};

namespace termite
{
    struct dbgContext
    {
        bx::AllocatorI* alloc;
        gfxDriverI* driver;
        uint8_t viewId;
        bx::FixedPool<State> statePool;
        State::SNode* stateStack;
        rect_t viewport;
        const fntFont* defaultFont;
        bool readyToDraw;
        vgContext* vgCtx;
        Camera* cam;
        mtx4x4_t viewProjMtx;

        gfxProgramHandle program;
        gfxUniformHandle uTexture;

        dbgContext(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
            driver = nullptr;
            viewId = 0;
            stateStack = nullptr;
            viewport = rectf(0, 0, 0, 0);
            defaultFont = nullptr;
            readyToDraw = false;
            program = T_INVALID_HANDLE;
            uTexture = T_INVALID_HANDLE;
            vgCtx = nullptr;
            cam = nullptr;
            viewProjMtx = mtx4x4Ident();
        }
    };
} // namespace termite

struct dbgMgr
{
    gfxDriverI* driver;
    bx::AllocatorI* alloc;
    gfxProgramHandle program;
    gfxTextureHandle whiteTexture;

    gfxUniformHandle uTexture;

    dbgMgr(bx::AllocatorI* _alloc) : alloc(_alloc)
    {
        driver = nullptr;
        program = T_INVALID_HANDLE;
        whiteTexture = T_INVALID_HANDLE;
        uTexture = T_INVALID_HANDLE;
    }
};

static dbgMgr* g_dbg = nullptr;

static vec2_t projectToScreen(const vec3_t point, const rect_t& rect, const mtx4x4_t& viewProjMtx)
{
    float w = rect.xmax - rect.xmin;
    float h = rect.ymax - rect.ymin;
    float wh = w*0.5f;
    float hh = h*0.5f;

    vec4_t proj;
    bx::vec4MulMtx(proj.f, vec4f(point.x, point.y, point.z, 1.0f).f, viewProjMtx.f);
    bx::vec3Mul(proj.f, proj.f, 1.0f / proj.w);     proj.w = 1.0f;
    
    float x = bx::ffloor(proj.x*wh + wh + 0.5f);
    float y = bx::ffloor(-proj.y*hh + hh + 0.5f);

    // ZCull
    if (proj.z < 0.0f || proj.z > 1.0f)
        return vec2f(FLT_MAX, FLT_MAX);
    return vec2f(x, y);
}

result_t termite::dbgInit(bx::AllocatorI* alloc, gfxDriverI* driver)
{
    if (g_dbg) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    g_dbg = BX_NEW(alloc, dbgMgr)(alloc);
    if (!g_dbg)
        return T_ERR_OUTOFMEM;
    g_dbg->alloc = alloc;
    g_dbg->driver = driver;
    g_dbg->whiteTexture = textureGetWhite1x1();
    if (!T_ISVALID(g_dbg->whiteTexture))
        return T_ERR_FAILED;
    
    // Load program
    {
        gfxShaderHandle vertexShader = driver->createShader(driver->makeRef(dbg_vso, sizeof(dbg_vso)));
        gfxShaderHandle fragmentShader = driver->createShader(driver->makeRef(dbg_fso, sizeof(dbg_fso)));
        if (!T_ISVALID(vertexShader) || !T_ISVALID(fragmentShader)) {
            T_ERROR("Creating shaders failed");
            return T_ERR_FAILED;
        }
        g_dbg->program = driver->createProgram(vertexShader, fragmentShader, true);
        if (!T_ISVALID(g_dbg->program)) {
            T_ERROR("Creating GPU program failed");
            return T_ERR_FAILED;
        }
    }

    dbgVertexPosCoordColor::init();

    g_dbg->uTexture = driver->createUniform("u_texture", gfxUniformType::Int1);
    assert(T_ISVALID(g_dbg->uTexture));

    // Create a 1x1 white texture 
    g_dbg->whiteTexture = textureGetWhite1x1();
    assert(T_ISVALID(g_dbg->whiteTexture));

    return T_OK;
}

void termite::dbgShutdown()
{
    if (!g_dbg)
        return;
    if (T_ISVALID(g_dbg->program))
        g_dbg->driver->destroyProgram(g_dbg->program);
    if (T_ISVALID(g_dbg->uTexture))
        g_dbg->driver->destroyUniform(g_dbg->uTexture);
    BX_DELETE(g_dbg->alloc, g_dbg);
    g_dbg = nullptr;
}

dbgContext* termite::dbgCreateContext(uint8_t viewId)
{
    assert(g_dbg);

    bx::AllocatorI* alloc = g_dbg->alloc;
    dbgContext* ctx = BX_NEW(alloc, dbgContext)(alloc);
    ctx->driver = g_dbg->driver;
    ctx->viewId = viewId;

    ctx->defaultFont = fntGet("fixedsys");
    if (!ctx->defaultFont) {
        dbgDestroyContext(ctx);
        return nullptr;
    }

    if (!ctx->statePool.create(STATE_POOL_SIZE, alloc)) {
        dbgDestroyContext(ctx);
        return nullptr;
    }

    // Push one state into state-stack
    State* state = ctx->statePool.newInstance();
    bx::pushStackNode<State*>(&ctx->stateStack, &state->snode, state);

    ctx->program = g_dbg->program;
    ctx->uTexture = g_dbg->uTexture;

    return ctx;
}

void termite::dbgDestroyContext(dbgContext* ctx)
{
    assert(g_dbg);
    assert(ctx);

    if (!ctx->alloc)
        return;

    ctx->statePool.destroy();
    BX_DELETE(ctx->alloc, ctx);
}

void termite::dbgBegin(dbgContext* ctx, float viewWidth, float viewHeight, Camera* cam, vgContext* vg)
{
    assert(ctx);
    ctx->viewport = rectf(0, 0, viewWidth, viewHeight);
    dbgReset(ctx);
    ctx->vgCtx = vg;
    ctx->viewport = rectf(0, 0, viewWidth, viewHeight);
    ctx->readyToDraw = true;

    mtx4x4_t projMtx = camProjMtx(cam, viewWidth / viewHeight);
    mtx4x4_t viewMtx = camViewMtx(cam);
    bx::mtxMul(ctx->viewProjMtx.f, viewMtx.f, projMtx.f);
    ctx->cam = cam;    

    if (vg)
        vgBegin(ctx->vgCtx, viewWidth, viewHeight);

    gfxDriverI* driver = ctx->driver;
    uint8_t viewId = ctx->viewId;
    driver->touch(viewId);
    driver->setViewRect(viewId, 0, 0, uint16_t(viewWidth), uint16_t(viewHeight));
    driver->setViewSeq(viewId, false);
    driver->setViewTransform(viewId, viewMtx.f, projMtx.f);
}

void termite::dbgEnd(dbgContext* ctx)
{
    if (ctx->vgCtx)
        vgEnd(ctx->vgCtx);
    ctx->readyToDraw = false;
}

void termite::dbgText(dbgContext* ctx, const vec3_t pos, const char* text)
{
    if (ctx->vgCtx) {
        vec2_t screenPt = projectToScreen(pos, ctx->viewport, ctx->viewProjMtx);

        State* state = ctx->stateStack->data;
        vgSetFont(ctx->vgCtx, state->font);
        vgTextColor(ctx->vgCtx, state->color);
        vgText(ctx->vgCtx, screenPt.x, screenPt.y, text);
    }
}

void termite::dbgTextf(dbgContext* ctx, const vec3_t pos, const char* fmt, ...)
{
    if (ctx->vgCtx) {
        char text[MAX_TEXT_SIZE];   text[0] = 0;

        va_list args;
        va_start(args, fmt);
        vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);

        dbgText(ctx, pos, text);
    }
}

void termite::dbgImage(dbgContext* ctx, const vec3_t pos, gfxTexture* image)
{
    if (ctx->vgCtx) {
        vec2_t screenPt = projectToScreen(pos, ctx->viewport, ctx->viewProjMtx);
        vgImage(ctx->vgCtx, screenPt.x, screenPt.y, image);
    }
}

void termite::dbgSnapGridXZ(dbgContext* ctx, float spacing, float maxDepth)
{
    spacing = bx::fceil(bx::fclamp(spacing, 1.0f, 20.0f));

    vec3_t corners[8];
    float ratio = (ctx->viewport.xmax - ctx->viewport.xmin) / (ctx->viewport.ymax - ctx->viewport.ymin);
    camCalcFrustumCorners(ctx->cam, corners, ratio, -2.0f, bx::fmin(maxDepth, ctx->cam->ffar));

    mtx4x4_t projToXz;
    mtxProjPlane(&projToXz, vec3f(0, 1.0f, 0));

    // project frustum corners to XZ plane add them to bounding box
    aabb_t bb = aabb();
    for (int i = 0; i < 8; i++) {
        vec3_t tmp;
        bx::vec3MulMtx(tmp.f, corners[i].f, projToXz.f);
        aabbPushPoint(&bb, tmp);
    }

    // Snap grid bounds to 'spacing'
    // Example: spacing = 5, snap bounds to -5, 0, 5, ...
    int nspace = (int)spacing;
    vec3_t& minpt = bb.vmin;
    vec3_t& maxpt = bb.vmax;
    aabb_t snapbox = aabbf(float(int(minpt.x) - int(minpt.x) % nspace),
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
    gfxDriverI* driver = ctx->driver;
    if (!driver->checkAvailTransientVertexBuffer(numVerts, dbgVertexPosCoordColor::Decl))
        return;
    gfxTransientVertexBuffer tvb;
    driver->allocTransientVertexBuffer(&tvb, numVerts, dbgVertexPosCoordColor::Decl);
    dbgVertexPosCoordColor* verts = (dbgVertexPosCoordColor*)tvb.data;
    
    int i = 0;
    State* state = ctx->stateStack->data;
    color_t color = premultiplyAlpha(state->color, state->alpha);
    for (float zoffset = snapbox.zmin; zoffset <= snapbox.zmax; zoffset += spacing, i += 2) {
        verts[i].x = snapbox.xmin;
        verts[i].y = 0;
        verts[i].z = zoffset;

        int ni = i + 1;
        verts[ni].x = snapbox.xmax;
        verts[ni].y = 0;
        verts[ni].z = zoffset;

        verts[i].color = verts[ni].color = color;
    }

    for (float xoffset = snapbox.xmin; xoffset <= snapbox.xmax; xoffset += spacing, i += 2) {
        verts[i].x = xoffset;
        verts[i].y = 0;
        verts[i].z = snapbox.zmin;

        int ni = i + 1;
        verts[ni].x = xoffset;
        verts[ni].y = 0;
        verts[ni].z = snapbox.zmax;

        verts[i].color = verts[ni].color = color;
    }

    mtx4x4_t ident = mtx4x4Ident();
    driver->setVertexBuffer(&tvb);
    driver->setTransform(ident.f, 1);
    driver->setState(gfxState::RGBWrite | gfxState::DepthTestLess | gfxState::PrimitiveLines);
    driver->setTexture(0, ctx->uTexture, g_dbg->whiteTexture);
    driver->submit(ctx->viewId, ctx->program);
}

void termite::dbgBoundingBox(dbgContext* ctx, const aabb_t aabb, bool showInfo /*= false*/)
{

}

void termite::dbgBoundingSphere(dbgContext* ctx, const sphere_t sphere, bool showInfo /*= false*/)
{

}

void termite::dbgBox(dbgContext* ctx, const aabb_t aabb, const mtx4x4_t* modelMtx /*= nullptr*/)
{

}

void termite::dbgSphere(dbgContext* ctx, const sphere_t sphere, const mtx4x4_t* modelMtx /*= nullptr*/)
{

}

void termite::dbgAxis(dbgContext* ctx, const vec3_t axis, const mtx4x4_t* modelMtx /*= nullptr*/)
{

}

void termite::dbgSetFont(dbgContext* ctx, fntFont* font)
{
    State* state = ctx->stateStack->data;
    state->font = font ? font : ctx->defaultFont;
}

void termite::dbgAlpha(dbgContext* ctx, float alpha)
{
    State* state = ctx->stateStack->data;
    state->alpha = alpha;
}

void termite::dbgColor(dbgContext* ctx, color_t color)
{
    State* state = ctx->stateStack->data;
    state->color = color;
}

void termite::dbgTransform(dbgContext* ctx, const mtx4x4_t& mtx)
{
    State* state = ctx->stateStack->data;
    state->mtx = mtx;
}

void termite::dbgPushState(dbgContext* ctx)
{
    State* curState = bx::peekStack<State*>(ctx->stateStack);;
    State* s = ctx->statePool.newInstance();
    if (s) {
        bx::pushStackNode<State*>(&ctx->stateStack, &s->snode, s);
        memcpy(s, curState, sizeof(State));
    }
}

void termite::dbgPopState(dbgContext* ctx)
{
    if (ctx->stateStack->down) {
        State* s = bx::popStack<State*>(&ctx->stateStack);
        ctx->statePool.deleteInstance(s);
    }
}

static void setDefaultState(dbgContext* ctx, State* state)
{
    state->mtx = mtx4x4Ident();
    state->color = rgba(255, 255, 255);
    state->alpha = 1.0f;
    state->scissor = ctx->viewport;
    state->font = ctx->defaultFont;
}

void termite::dbgReset(dbgContext* ctx)
{
    State::SNode* node = ctx->stateStack;
    while (node->down) {
        State* s = bx::popStack<State*>(&ctx->stateStack);
        ctx->statePool.deleteInstance(s);
    }
    setDefaultState(ctx, node->data);
}
