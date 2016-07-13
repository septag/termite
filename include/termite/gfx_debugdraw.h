#pragma once

#include "bx/allocator.h"
#include "vec_math.h"

namespace termite
{
    struct GfxDriverApi;
    struct DebugDrawContext;
    struct VectorGfxContext;
    struct Camera;
	struct Texture;
	class Font;

    result_t initDebugDraw(bx::AllocatorI* alloc, GfxDriverApi* driver);
    void shutdownDebugDraw();

    TERMITE_API DebugDrawContext* createDebugDrawContext(uint8_t viewId);
    TERMITE_API void destroyDebugDrawContext(DebugDrawContext* ctx);

    TERMITE_API void ddBegin(DebugDrawContext* ctx, float viewWidth, float viewHeight, Camera* cam, VectorGfxContext* vg = nullptr);
    TERMITE_API void ddEnd(DebugDrawContext* ctx);

    // Draws
    TERMITE_API void ddText(DebugDrawContext* ctx, const vec3_t pos, const char* text);
    TERMITE_API void ddTextf(DebugDrawContext* ctx, const vec3_t pos, const char* fmt, ...);
    TERMITE_API void ddImage(DebugDrawContext* ctx, const vec3_t pos, Texture* image);
    TERMITE_API void ddSnapGridXZ(DebugDrawContext* ctx, float spacing, float boldSpacing, float maxDepth);
    TERMITE_API void ddBoundingBox(DebugDrawContext* ctx, const aabb_t bb, bool showInfo = false);
    TERMITE_API void ddBoundingSphere(DebugDrawContext* ctx, const sphere_t sphere, bool showInfo = false);
    TERMITE_API void ddBox(DebugDrawContext* ctx, const aabb_t aabb, const mtx4x4_t* modelMtx = nullptr);
    TERMITE_API void ddSphere(DebugDrawContext* ctx, const sphere_t sphere, const mtx4x4_t* modelMtx = nullptr);
    TERMITE_API void ddAxis(DebugDrawContext* ctx, const vec3_t axis, const mtx4x4_t* modelMtx = nullptr);
    TERMITE_API void ddRect(DebugDrawContext* ctx, const vec3_t& vmin, const vec3_t& vmax);
	TERMITE_API void ddCircle(DebugDrawContext* ctx, const vec3_t& pos, float radius, const mtx4x4_t* modelMtx = nullptr,
							  bool showDir = false);
	TERMITE_API void ddRect(DebugDrawContext* ctx, const vec3_t& minpt, const vec3_t& maxpt, const mtx4x4_t* modelMtx = nullptr);
	TERMITE_API void ddLine(DebugDrawContext* ctx, const vec3_t& startPt, const vec3_t& endPt, const mtx4x4_t* modelMtx = nullptr);

    // State
    TERMITE_API void ddSetFont(DebugDrawContext* ctx, Font* font);
    TERMITE_API void ddAlpha(DebugDrawContext* ctx, float alpha);
    TERMITE_API void ddColor(DebugDrawContext* ctx, const vec4_t& color);
    TERMITE_API void ddTransform(DebugDrawContext* ctx, const mtx4x4_t& mtx);

    TERMITE_API void ddPushState(DebugDrawContext* ctx);
    TERMITE_API void ddPopState(DebugDrawContext* ctx);
    TERMITE_API void ddReset(DebugDrawContext* ctx);
} // namespace termite