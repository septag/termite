#pragma once

#include "vec_math.h"

namespace termite
{
    class gfxDriverI;
    struct vgContext;
    class fntFont;
    struct gfxTexture;

    result_t vgInit(bx::AllocatorI* alloc, gfxDriverI* driver);
    void vgShutdown();

    TERMITE_API vgContext* vgCreateContext(uint8_t viewId, int maxVerts = 0, int maxBatches = 0);
    TERMITE_API void vgDestroyContext(vgContext* ctx);

    TERMITE_API void vgBegin(vgContext* ctx, float viewWidth, float viewHeight);
    TERMITE_API void vgEnd(vgContext* ctx);

    // Text
    TERMITE_API void vgSetFont(vgContext* ctx, const fntFont* font);
    TERMITE_API void vgText(vgContext* ctx, float x, float y, const char* text);
    TERMITE_API void vgTextf(vgContext* ctx, float x, float y, const char* fmt, ...);

    // Rect/Image
    TERMITE_API void vgRectf(vgContext* ctx, float x, float y, float width, float height);
    TERMITE_API void vgRect(vgContext* ctx, const rect_t& rect);
    TERMITE_API void vgImageRect(vgContext* ctx, const rect_t& rect, gfxTexture* image);
    TERMITE_API void vgImage(vgContext* ctx, float x, float y, gfxTexture* image);

    // States
    TERMITE_API void vgScissor(vgContext* ctx, const rect_t& rect);
    TERMITE_API void vgAlpha(vgContext* ctx, float alpha);
    TERMITE_API void vgTextColor(vgContext* ctx, color_t color);
    TERMITE_API void vgStrokeColor(vgContext* ctx, color_t color);
    TERMITE_API void vgFillColor(vgContext* ctx, color_t color);
    TERMITE_API void vgTranslate(vgContext* ctx, float x, float y);
    TERMITE_API void vgScale(vgContext* ctx, float sx, float sy);
    TERMITE_API void vgRotate(vgContext* ctx, float theta);
    TERMITE_API void vgResetTransform(vgContext* ctx);

    TERMITE_API void vgPushState(vgContext* ctx);
    TERMITE_API void vgPopState(vgContext* ctx);
    TERMITE_API void vgReset(vgContext* ctx);
};