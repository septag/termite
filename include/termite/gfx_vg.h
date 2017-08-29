#pragma once

#include "vec_math.h"
#include "resource_lib.h"

namespace termite
{
    struct GfxDriverApi;
    struct VectorGfxContext;
    struct Texture;

    result_t initVectorGfx(bx::AllocatorI* alloc, GfxDriverApi* driver);
    void shutdownVectorGfx();

    TERMITE_API VectorGfxContext* createVectorGfxContext(int maxVerts = 0, int maxBatches = 0);
    TERMITE_API void destroyVectorGfxContext(VectorGfxContext* ctx);

    TERMITE_API void vgBegin(VectorGfxContext* ctx, uint8_t viewId, const recti_t& viewport,
                             const mtx4x4_t* viewMtx = nullptr, const mtx4x4_t* projMtx = nullptr);
    TERMITE_API void vgEnd(VectorGfxContext* ctx);

    // Text
    TERMITE_API void vgSetFont(VectorGfxContext* ctx, ResourceHandle fontHandle);
    TERMITE_API void vgText(VectorGfxContext* ctx, float x, float y, const char* text);
    TERMITE_API void vgTextf(VectorGfxContext* ctx, float x, float y, const char* fmt, ...);
    TERMITE_API void vgTextv(VectorGfxContext* ctx, float x, float y, const char* fmt, va_list argList);

    // Rect/Image/Line
    TERMITE_API void vgRectf(VectorGfxContext* ctx, float x, float y, float width, float height);
    TERMITE_API void vgRect(VectorGfxContext* ctx, const rect_t& rect);
    TERMITE_API void vgImageRect(VectorGfxContext* ctx, const rect_t& rect, const Texture* image);
    TERMITE_API void vgImage(VectorGfxContext* ctx, float x, float y, const Texture* image);
    TERMITE_API void vgLine(VectorGfxContext* ctx, const vec2_t& p1, const vec2_t& p2, float lineWidth);
    TERMITE_API void vgArrow(VectorGfxContext* ctx, const vec2_t& p1, const vec2_t& p2, float lineWidth, float arrowLength);

    // States
    TERMITE_API void vgScissor(VectorGfxContext* ctx, const recti_t& rect);
    TERMITE_API void vgAlpha(VectorGfxContext* ctx, float alpha);
    TERMITE_API void vgTextColor(VectorGfxContext* ctx, color_t color);
    TERMITE_API void vgStrokeColor(VectorGfxContext* ctx, color_t color);
    TERMITE_API void vgFillColor(VectorGfxContext* ctx, color_t color);
    TERMITE_API void vgTranslate(VectorGfxContext* ctx, float x, float y);
    TERMITE_API void vgScale(VectorGfxContext* ctx, float sx, float sy);
    TERMITE_API void vgRotate(VectorGfxContext* ctx, float theta);
    TERMITE_API void vgResetTransform(VectorGfxContext* ctx);

    TERMITE_API void vgPushState(VectorGfxContext* ctx);
    TERMITE_API void vgPopState(VectorGfxContext* ctx);
    TERMITE_API void vgReset(VectorGfxContext* ctx);

};