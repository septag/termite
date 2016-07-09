#pragma once

#include "vec_math.h"

namespace termite
{
    struct GfxApi;
    struct VectorGfxContext;
    class Font;
    struct Texture;

    result_t initVectorGfx(bx::AllocatorI* alloc, GfxApi* driver);
    void shutdownVectorGfx();

    TERMITE_API VectorGfxContext* createVectorGfxContext(uint8_t viewId, int maxVerts = 0, int maxBatches = 0);
    TERMITE_API void destroyVectorGfxContext(VectorGfxContext* ctx);

    TERMITE_API void vgBegin(VectorGfxContext* ctx, float viewWidth, float viewHeight);
    TERMITE_API void vgEnd(VectorGfxContext* ctx);

    // Text
    TERMITE_API void vgSetFont(VectorGfxContext* ctx, const Font* font);
    TERMITE_API void vgText(VectorGfxContext* ctx, float x, float y, const char* text);
    TERMITE_API void vgTextf(VectorGfxContext* ctx, float x, float y, const char* fmt, ...);

    // Rect/Image
    TERMITE_API void vgRectf(VectorGfxContext* ctx, float x, float y, float width, float height);
    TERMITE_API void vgRect(VectorGfxContext* ctx, const rect_t& rect);
    TERMITE_API void vgImageRect(VectorGfxContext* ctx, const rect_t& rect, Texture* image);
    TERMITE_API void vgImage(VectorGfxContext* ctx, float x, float y, Texture* image);

    // States
    TERMITE_API void vgScissor(VectorGfxContext* ctx, const rect_t& rect);
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