#pragma once

#include "bx/allocator.h"
#include "vec_math.h"

namespace termite
{
    class gfxDriverI;
    struct dbgContext;
    struct vgContext;
    struct Camera;

    result_t dbgInit(bx::AllocatorI* alloc, gfxDriverI* driver);
    void dbgShutdown();

    TERMITE_API dbgContext* dbgCreateContext(uint8_t viewId);
    TERMITE_API void dbgDestroyContext(dbgContext* ctx);

    TERMITE_API void dbgBegin(dbgContext* ctx, float viewWidth, float viewHeight, Camera* cam, vgContext* vg = nullptr);
    TERMITE_API void dbgEnd(dbgContext* ctx);

    // Draws
    TERMITE_API void dbgText(dbgContext* ctx, const vec3_t pos, const char* text);
    TERMITE_API void dbgTextf(dbgContext* ctx, const vec3_t pos, const char* fmt, ...);
    TERMITE_API void dbgImage(dbgContext* ctx, const vec3_t pos, gfxTexture* image);
    TERMITE_API void dbgSnapGridXZ(dbgContext* ctx, float spacing, float maxDepth);
    TERMITE_API void dbgBoundingBox(dbgContext* ctx, const aabb_t bb, bool showInfo = false);
    TERMITE_API void dbgBoundingSphere(dbgContext* ctx, const sphere_t sphere, bool showInfo = false);
    TERMITE_API void dbgBox(dbgContext* ctx, const aabb_t aabb, const mtx4x4_t* modelMtx = nullptr);
    TERMITE_API void dbgSphere(dbgContext* ctx, const sphere_t sphere, const mtx4x4_t* modelMtx = nullptr);
    TERMITE_API void dbgAxis(dbgContext* ctx, const vec3_t axis, const mtx4x4_t* modelMtx = nullptr);
    TERMITE_API void dbgRect(dbgContext* ctx, const vec3_t& vmin, const vec3_t& vmax);

    // State
    TERMITE_API void dbgSetFont(dbgContext* ctx, fntFont* font);
    TERMITE_API void dbgAlpha(dbgContext* ctx, float alpha);
    TERMITE_API void dbgColor(dbgContext* ctx, const vec4_t& color);
    TERMITE_API void dbgTransform(dbgContext* ctx, const mtx4x4_t& mtx);

    TERMITE_API void dbgPushState(dbgContext* ctx);
    TERMITE_API void dbgPopState(dbgContext* ctx);
    TERMITE_API void dbgReset(dbgContext* ctx);
} // namespace termite