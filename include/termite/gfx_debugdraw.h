#pragma once

#include "bx/allocator.h"

namespace tee
{
    struct DebugDraw;
    struct DebugDraw2D;
    struct Camera;
    struct Camera2D;
	struct Texture;

    namespace gfx {
        TEE_API DebugDraw* createDebugDraw();
        TEE_API void destroyDebugDraw(DebugDraw* ctx);

        // Automatically 'Begin's the VectorGfx if provided in arguments
        // VectorGfx will be set to screen-space 2d coordinates
        TEE_API void beginDebugDraw(DebugDraw* ctx, uint8_t viewId,
                                         const irect_t& viewport,
                                         const mat4_t& viewMtx, const mat4_t& projMtx,
                                         DebugDraw2D* vg = nullptr);
        TEE_API void endDebugDraw(DebugDraw* ctx);

        // Draws
        TEE_API void textDbg(DebugDraw* ctx, const vec3_t pos, const char* text);
        TEE_API void textfDbg(DebugDraw* ctx, const vec3_t pos, const char* fmt, ...);
        TEE_API void textDbg(DebugDraw* ctx, const vec3_t pos, const char* fmt, va_list argList);
        TEE_API void imageDbg(DebugDraw* ctx, const vec3_t pos, Texture* image);
        TEE_API void xzGridDbg(DebugDraw* ctx, const Camera& cam, float spacing, float boldSpacing, float maxDepth,
                                    ucolor_t color = ucolor(0xff808080), ucolor_t boldColor = ucolor(0xffffffff));
        TEE_API void xyGridDbg(DebugDraw* ctx, const Camera2D& cam, float spacing, float boldSpacing,
                                    ucolor_t color = ucolor(0xff808080), ucolor_t boldColor = ucolor(0xffffffff),
                                    bool showVerticalInfo = false);
        TEE_API void bboxDbg(DebugDraw* ctx, const aabb_t bb, bool showInfo = false);
        TEE_API void bsphereDbg(DebugDraw* ctx, const sphere_t sphere, bool showInfo = false);
        TEE_API void rectDbg(DebugDraw* ctx, const vec3_t& vmin, const vec3_t& vmax);
        TEE_API void circleDbg(DebugDraw* ctx, const vec3_t& pos, float radius, const mat4_t* modelMtx = nullptr,
                                    bool showDir = false);
        TEE_API void rectDbg(DebugDraw* ctx, const vec3_t& minpt, const vec3_t& maxpt, const mat4_t* modelMtx = nullptr);
        TEE_API void lineDbg(DebugDraw* ctx, const vec3_t& startPt, const vec3_t& endPt, const mat4_t* modelMtx = nullptr);

        // State
        TEE_API void fontDbg(DebugDraw* ctx, AssetHandle fontHandle);
        TEE_API void alphaDbg(DebugDraw* ctx, float alpha);
        TEE_API void colorDbg(DebugDraw* ctx, const vec4_t& color);
        TEE_API void transformDbg(DebugDraw* ctx, const mat4_t& mtx);

        TEE_API void pushDbg(DebugDraw* ctx);
        TEE_API void popDbg(DebugDraw* ctx);
        TEE_API void resetDbg(DebugDraw* ctx);
    }

} // namespace tee