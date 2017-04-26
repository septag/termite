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

    TERMITE_API void vgBegin(VectorGfxContext* ctx, uint8_t viewId, float viewWidth, float viewHeight,
                             const mtx4x4_t* viewMtx = nullptr, const mtx4x4_t* projMtx = nullptr);
    TERMITE_API void vgEnd(VectorGfxContext* ctx);

    // Text
    TERMITE_API void vgSetFont(VectorGfxContext* ctx, ResourceHandle fontHandle);
    TERMITE_API void vgText(VectorGfxContext* ctx, float x, float y, const char* text);
    TERMITE_API void vgTextf(VectorGfxContext* ctx, float x, float y, const char* fmt, ...);
    TERMITE_API void vgTextv(VectorGfxContext* ctx, float x, float y, const char* fmt, va_list argList);

    // Rect/Image
    TERMITE_API void vgRectf(VectorGfxContext* ctx, float x, float y, float width, float height);
    TERMITE_API void vgRect(VectorGfxContext* ctx, const rect_t& rect);
    TERMITE_API void vgImageRect(VectorGfxContext* ctx, const rect_t& rect, const Texture* image);
    TERMITE_API void vgImage(VectorGfxContext* ctx, float x, float y, const Texture* image);

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

    class VectorGfx
    {
    public:
        VectorGfx() :
            m_ctx(nullptr)
        {
        }

        explicit VectorGfx(VectorGfxContext* ctx) :
            m_ctx(ctx)
        {
        }

        inline bool createContext(int maxVerts = 0, int maxBatches = 0)
        {
            m_ctx = createVectorGfxContext(maxVerts, maxBatches);
            return m_ctx != nullptr;
        }

        inline void destroyContext()
        {
            if (m_ctx)
                destroyVectorGfxContext(m_ctx);
            m_ctx = nullptr;
        }

        inline VectorGfx& begin(uint8_t viewId, float viewWidth, float viewHeight,
                                const mtx4x4_t* viewMtx = nullptr, const mtx4x4_t* projMtx = nullptr)
        {
            vgBegin(m_ctx, viewId, viewWidth, viewHeight, viewMtx, projMtx);
            return *this;
        }

        inline void end()
        {
            vgEnd(m_ctx);
        }

        inline VectorGfx& setFont(ResourceHandle fontHandle)
        {
            vgSetFont(m_ctx, fontHandle);
            return *this;
        }

        VectorGfx& setScissor(const rect_t& rc)
        {
            vgScissor(m_ctx, rc);
            return *this;
        }

        VectorGfx& setAlpha(float alpha)
        {
            vgAlpha(m_ctx, alpha);
            return *this;
        }

        VectorGfx& setTextColor(const color_t color)
        {
            vgTextColor(m_ctx, color);
            return *this;
        }

        VectorGfx& setStrokeColor(const color_t color)
        {
            vgStrokeColor(m_ctx, color);
            return *this;
        }

        VectorGfx& setFillColor(const color_t color)
        {
            vgFillColor(m_ctx, color);
            return *this;
        }

        VectorGfx& translate(float x, float y)
        {
            vgTranslate(m_ctx, x, y);
            return *this;
        }

        VectorGfx& scale(float sx, float sy)
        {
            vgScale(m_ctx, sx, sy);
            return *this;
        }

        VectorGfx& rotate(float alpha)
        {
            vgRotate(m_ctx, alpha);
            return *this;
        }

        void pushState()
        {
            vgPushState(m_ctx);
        }

        void popState()
        {
            vgPopState(m_ctx);
        }

        void reset()
        {
            vgReset(m_ctx);
        }

        VectorGfx& text(float x, float y, const char* text)
        {
            vgText(m_ctx, x, y, text);
            return *this;
        }

        VectorGfx& textf(float x, float y, const char* fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            vgTextv(m_ctx, x, y, fmt, args);
            va_end(args);
            return *this;
        }

        VectorGfx& rectangle(float x, float y, float width, float height)
        {
            vgRectf(m_ctx, x, y, width, height);
            return *this;
        }

        VectorGfx& rectangle(const rect_t& rc)
        {
            vgRect(m_ctx, rc);
            return *this;
        }

        VectorGfx& imageRect(const rect_t& rc, const Texture* image)
        {
            vgImageRect(m_ctx, rc, image);
            return *this;
        }

        VectorGfx& image(float x, float y, const Texture* image)
        {
            vgImage(m_ctx, x, y, image);
            return *this;
        }

        inline VectorGfxContext* getContext() const
        {
            return m_ctx;
        }

    private:
        VectorGfxContext* m_ctx;
    };
};