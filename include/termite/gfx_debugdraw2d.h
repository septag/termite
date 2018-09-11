#pragma once

#include "math.h"
#include "assetlib.h"

namespace tee
{
    struct DebugDraw2D;
    struct Texture;
    
    namespace gfx {
        TEE_API DebugDraw2D* createDebugDraw2D(int maxVerts = 0, int maxBatches = 0);
        TEE_API void destroyDebugDraw2D(DebugDraw2D* ctx);

        TEE_API void beginDebugDraw2D(DebugDraw2D* ctx, uint8_t viewId, const irect_t& viewport,
                                      const mat4_t* viewMtx = nullptr, const mat4_t* projMtx = nullptr);
        TEE_API void endDebugDraw2D(DebugDraw2D* ctx);

        // Text
        TEE_API void fontDbg2D(DebugDraw2D* ctx, AssetHandle fontHandle);
        TEE_API void textDbg2D(DebugDraw2D* ctx, float x, float y, const char* text);
        TEE_API void textfDbg2D(DebugDraw2D* ctx, float x, float y, const char* fmt, ...);
        TEE_API void textDbg2D(DebugDraw2D* ctx, float x, float y, const char* fmt, va_list argList);

        // Rect/Image/Line
        TEE_API void rectDbg2D(DebugDraw2D* ctx, float x, float y, float width, float height);
        TEE_API void rectDbg2D(DebugDraw2D* ctx, const rect_t& rect);
        TEE_API void imageDbg2D(DebugDraw2D* ctx, const rect_t& rect, const Texture* image);
        TEE_API void imageDbg2D(DebugDraw2D* ctx, float x, float y, const Texture* image);
        TEE_API void lineDbg2D(DebugDraw2D* ctx, const vec2_t& p1, const vec2_t& p2, float lineWidth);
        TEE_API void arrowDbg2D(DebugDraw2D* ctx, const vec2_t& p1, const vec2_t& p2, float lineWidth, float arrowLength);
        TEE_API void arrowTwoSidedDbg2D(DebugDraw2D* ctx, const vec2_t& p1, const vec2_t& p2, float lineWidth, float arrowLength);

        // States
        TEE_API void scissorDbg2D(DebugDraw2D* ctx, const irect_t& rect);
        TEE_API void alphaDbg2D(DebugDraw2D* ctx, float alpha);
        TEE_API void textColorDbg2D(DebugDraw2D* ctx, ucolor_t color);
        TEE_API void strokeColorDbg2D(DebugDraw2D* ctx, ucolor_t color);
        TEE_API void fillColorDbg2D(DebugDraw2D* ctx, ucolor_t color);
        TEE_API void translateDbg2D(DebugDraw2D* ctx, float x, float y);
        TEE_API void scaleDbg2D(DebugDraw2D* ctx, float sx, float sy);
        TEE_API void rotateDbg2D(DebugDraw2D* ctx, float theta);
        TEE_API void resetTransformDbg2D(DebugDraw2D* ctx);

        TEE_API void pushDbg2D(DebugDraw2D* ctx);
        TEE_API void popDbg2D(DebugDraw2D* ctx);
        TEE_API void resetDbg2D(DebugDraw2D* ctx);
    }
};