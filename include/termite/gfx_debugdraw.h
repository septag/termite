#pragma once

#include "bx/allocator.h"
#include "vec_math.h"

namespace termite
{
    struct GfxDriverApi;
    struct DebugDrawContext;
    struct VectorGfxContext;
    struct Camera;
    struct Camera2D;
	struct Texture;
	class Font;

    result_t initDebugDraw(bx::AllocatorI* alloc, GfxDriverApi* driver);
    void shutdownDebugDraw();

    TERMITE_API DebugDrawContext* createDebugDrawContext();
    TERMITE_API void destroyDebugDrawContext(DebugDrawContext* ctx);

    // Automatically 'Begin's the VectorGfx if provided in arguments
    // VectorGfx will be set to screen-space 2d coordinates
    TERMITE_API void ddBegin(DebugDrawContext* ctx, uint8_t viewId, 
                             float viewWidth, float viewHeight, 
                             const mtx4x4_t& viewMtx, const mtx4x4_t& projMtx, 
                             VectorGfxContext* vg = nullptr);
    TERMITE_API void ddEnd(DebugDrawContext* ctx);

    // Draws
    TERMITE_API void ddText(DebugDrawContext* ctx, const vec3_t pos, const char* text);
    TERMITE_API void ddTextf(DebugDrawContext* ctx, const vec3_t pos, const char* fmt, ...);
    TERMITE_API void ddTextv(DebugDrawContext* ctx, const vec3_t pos, const char* fmt, va_list argList);
    TERMITE_API void ddImage(DebugDrawContext* ctx, const vec3_t pos, Texture* image);
    TERMITE_API void ddSnapGridXZ(DebugDrawContext* ctx, const Camera& cam, float spacing, float boldSpacing, float maxDepth,
                                  color_t color = color1n(0xff808080), color_t boldColor = color1n(0xffffffff));
    TERMITE_API void ddSnapGridXY(DebugDrawContext* ctx, const Camera2D& cam, float spacing, float boldSpacing,
                                  color_t color = color1n(0xff808080), color_t boldColor = color1n(0xffffffff),
                                  bool showVerticalInfo = false);
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

    class DebugDraw
    {
    private:
        DebugDrawContext* m_ctx;

    public:
        DebugDraw() :
            m_ctx(nullptr)
        {
        }

        explicit DebugDraw(DebugDrawContext* _ctx) :
            m_ctx(_ctx)
        {
        }

        inline bool createContext()
        {
            assert(!m_ctx);
            m_ctx = createDebugDrawContext();
            return m_ctx != nullptr;
        }

        inline void destroyContext()
        {
            if (m_ctx)
                destroyDebugDrawContext(m_ctx);
            m_ctx = nullptr;
        }

        inline DebugDraw& begin(uint8_t viewId, float viewWidth, float viewHeight,
                                const mtx4x4_t& viewMtx, const mtx4x4_t& projMtx,
                                VectorGfxContext* vg = nullptr)
        {
            ddBegin(m_ctx, viewId, viewWidth, viewHeight, viewMtx, projMtx, vg);
            return *this;
        }

        inline void end()
        {
            ddEnd(m_ctx);
        }

        inline DebugDraw& text(const vec3_t& pos, const char* text)
        {
            ddText(m_ctx, pos, text);
            return *this;
        }

        inline DebugDraw& textf(const vec3_t& pos, const char* fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            ddTextv(m_ctx, pos, fmt, args);
            va_end(args);
            return *this;
        }

        inline DebugDraw& image(const vec3_t& pos, Texture* image)
        {
            ddImage(m_ctx, pos, image);
            return *this;
        }

        inline DebugDraw& snapGridXZ(const Camera& cam, float spacing, float boldSpacing, float maxDepth,
                                     color_t color = color1n(0xff808080), color_t boldColor = color1n(0xffffffff))
        {
            ddSnapGridXZ(m_ctx, cam, spacing, boldSpacing, maxDepth, color, boldColor);
            return *this;
        }

        inline DebugDraw& snapGridXY(const Camera2D& cam, float spacing, float boldSpacing,
                                     color_t color = color1n(0xff808080), color_t boldColor = color1n(0xffffffff),
                                     bool showVerticalInfo = false)
        {
            ddSnapGridXY(m_ctx, cam, spacing, boldSpacing, color, boldColor, showVerticalInfo);
            return *this;
        }

        inline DebugDraw& boundingBox(const aabb_t& bb, bool showInfo = false)
        {
            ddBoundingBox(m_ctx, bb, showInfo);
            return *this;
        }

        inline DebugDraw& boundingSphere(const sphere_t& sphere, bool showInfo = false)
        {
            ddBoundingSphere(m_ctx, sphere, showInfo);
            return *this;
        }

        inline DebugDraw& box(const aabb_t& aabb, const mtx4x4_t* modelMtx = nullptr)
        {
            ddBox(m_ctx, aabb, modelMtx);
            return *this;
        }

        inline DebugDraw& sphere(const sphere_t& sphere, const mtx4x4_t* modelMtx = nullptr)
        {
            ddSphere(m_ctx, sphere, modelMtx);
            return *this;
        }

        inline DebugDraw& axis(const vec3_t& axis, const mtx4x4_t* modelMtx = nullptr)
        {
            ddAxis(m_ctx, axis, modelMtx);
            return *this;
        }

        inline DebugDraw& rect(const vec3_t& vmin, const vec3_t& vmax, const mtx4x4_t* modelMtx = nullptr)
        {
            ddRect(m_ctx, vmin, vmax, modelMtx);
            return *this;
        }

        inline DebugDraw& circle(const vec3_t& pos, float radius, const mtx4x4_t* modelMtx = nullptr, bool showDir = false)
        {
            ddCircle(m_ctx, pos, radius, modelMtx, showDir);
            return *this;
        }

        inline DebugDraw& line(const vec3_t& startPt, const vec3_t& endPt, const mtx4x4_t* modelMtx = nullptr)
        {
            ddLine(m_ctx, startPt, endPt, modelMtx);
            return *this;
        }

        inline DebugDraw& setFont(Font* font)
        {
            ddSetFont(m_ctx, font);
            return *this;
        }

        inline DebugDraw& setAlpha(float alpha)
        {
            ddAlpha(m_ctx, alpha);
            return *this;
        }

        inline DebugDraw& setColor(const vec4_t& color)
        {
            ddColor(m_ctx, color);
            return *this;
        }

        inline DebugDraw& setTransform(const mtx4x4_t& mtx)
        {
            ddTransform(m_ctx, mtx);
            return *this;
        }

        inline void pushState()
        {
            ddPushState(m_ctx);
        }

        inline void popState()
        {
            ddPopState(m_ctx);
        }

        inline void reset()
        {
            ddReset(m_ctx);
        }

        inline DebugDrawContext* getContext() const
        {
            return m_ctx;
        }
    };
} // namespace termite