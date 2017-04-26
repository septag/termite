#pragma once

#include <cstdarg>
#include "bx/bx.h"
#include "bx/allocator.h"

#include "gfx_defines.h"

namespace termite
{
    TERMITE_API VertexDecl* vdeclBegin(VertexDecl* vdecl, RendererType::Enum _type = RendererType::Noop);
    TERMITE_API void vdeclEnd(VertexDecl* vdecl);
    TERMITE_API VertexDecl* vdeclAdd(VertexDecl* vdecl, VertexAttrib::Enum _attrib, uint8_t _num, VertexAttribType::Enum _type,
                                     bool _normalized = false, bool _asInt = false);
    TERMITE_API VertexDecl* vdeclSkip(VertexDecl* vdecl, uint8_t _numBytes);
    TERMITE_API void vdeclDecode(VertexDecl* vdecl, VertexAttrib::Enum _attrib, uint8_t* _num, VertexAttribType::Enum* _type,
                                 bool* _normalized, bool* _asInt);
    TERMITE_API bool vdeclHas(VertexDecl* vdecl, VertexAttrib::Enum _attrib);
    TERMITE_API uint32_t vdeclGetSize(VertexDecl* vdecl, uint32_t _num);

    class VertexDeclHelper
    {
    private:
        VertexDecl m_declStub;
        VertexDecl* m_decl;

    public:
        VertexDeclHelper() :
            m_decl(&m_declStub)
        {
        }

        explicit VertexDeclHelper(VertexDecl* decl) :
            m_decl(decl)
        {
        }

        inline VertexDeclHelper& begin(RendererType::Enum _type = RendererType::Noop)
        {
            vdeclBegin(m_decl, _type);
            return *this;
        }

        inline void end()
        {
            vdeclEnd(m_decl);
        }

        inline VertexDeclHelper& add(VertexAttrib::Enum attrib, uint8_t num, VertexAttribType::Enum type, 
                                     bool normalized = false, bool asInt = false)
        {
            vdeclAdd(m_decl, attrib, num, type, normalized, asInt);
            return *this;
        }

        inline VertexDeclHelper& skip(uint8_t numBytes)
        {
            vdeclSkip(m_decl, numBytes);
            return *this;
        }

        inline void decode(VertexAttrib::Enum attrib, uint8_t* num, VertexAttribType::Enum* type, bool* normalized,
                           bool* asInt) const
        {
            vdeclDecode(m_decl, attrib, num, type, normalized, asInt);
        }

        inline bool has(VertexAttrib::Enum attrib) const
        {
            return vdeclHas(m_decl, attrib);
        }

        inline uint32_t getSize(uint32_t num) const
        {
            return vdeclGetSize(m_decl, num);
        }

        const VertexDecl& getDecl() const
        {
            return *m_decl;
        }
    };

    class BX_NO_VTABLE GfxDriverEventsI
    {
    public:
        virtual void onFatal(GfxFatalType::Enum type, const char* str) = 0;
        virtual void onTraceVargs(const char* filepath, int line, const char* format, va_list argList) = 0;
        virtual uint32_t onCacheReadSize(uint64_t id) = 0;
        virtual bool onCacheRead(uint64_t id, void* data, uint32_t size) = 0;
        virtual void onCacheWrite(uint64_t id, const void* data, uint32_t size) = 0;
        virtual void onScreenShot(const char *filePath, uint32_t width, uint32_t height, uint32_t pitch, 
                                  const void *data, uint32_t size, bool yflip) = 0;
        virtual void onCaptureBegin(uint32_t width, uint32_t height, uint32_t pitch, TextureFormat::Enum fmt, bool yflip) = 0;
        virtual void onCaptureEnd() = 0;
        virtual void onCaptureFrame(const void* data, uint32_t size) = 0;
    };

    typedef void (*gfxReleaseMemCallback)(void* ptr, void* userData);

    struct GfxDriverApi
    {
    public:
        // Init
        result_t (*init)(uint16_t deviceId, GfxDriverEventsI* callbacks, bx::AllocatorI* alloc);
        void (*shutdown)();

        void(*reset)(uint32_t width, uint32_t height, GfxResetFlag::Bits flags/* = GfxResetFlag::None*/);
        uint32_t(*frame)();
        void(*setDebug)(GfxDebugFlag::Bits debugFlags);
        RendererType::Enum (*getRendererType)();
        const GfxCaps& (*getCaps)();
        const GfxStats& (*getStats)();
        const HMDDesc& (*getHMD)();

        // Platform specific
        RenderFrameType::Enum(*renderFrame)();
        void(*setPlatformData)(const GfxPlatformData& data);
        const GfxInternalData& (*getInternalData)();
        void(*overrideInternal)(TextureHandle handle, uintptr_t ptr);
        void(*overrideInternal2)(TextureHandle handle, uint16_t width, uint16_t height, uint8_t numMips,
                                 TextureFormat::Enum fmt, TextureFlag::Bits flags);

        // Misc
        void(*discard)();
        uint32_t(*touch)(uint8_t id);
        void(*setPaletteColor)(uint8_t index, uint32_t rgba);
        void(*setPaletteColorRgba)(uint8_t index, float rgba[4]);
        void(*setPaletteColorRgbaf)(uint8_t index, float r, float g, float b, float a);
        void(*saveScreenshot)(const char* filepath);

        // Views
        void(*setViewName)(uint8_t id, const char* name);
        void(*setViewRect)(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
        void(*setViewRectRatio)(uint8_t id, uint16_t x, uint16_t y, BackbufferRatio::Enum ratio);
        void(*setViewScissor)(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
        void(*setViewClear)(uint8_t id, GfxClearFlag::Bits flags, uint32_t rgba, float depth, uint8_t stencil);
        void(*setViewClearPalette)(uint8_t id, GfxClearFlag::Bits flags, float depth, uint8_t stencil,
                                   uint8_t color0, uint8_t color1, uint8_t color2, uint8_t color3,
                                   uint8_t color4, uint8_t color5, uint8_t color6, uint8_t color7);
        void (*setViewSeq)(uint8_t id, bool enabled);
        void(*setViewTransform)(uint8_t id, const void* view, const void* projLeft,
                                GfxViewFlag::Bits flags/* = GfxViewFlag::Stereo*/, const void* projRight/* = nullptr*/);
        void(*setViewFrameBuffer)(uint8_t id, FrameBufferHandle handle);
        void(*resetView)(uint8_t id);

        // Draw
        void(*setMarker)(const char* marker);
        void(*setState)(GfxState::Bits state, uint32_t rgba/* = 0*/);
        void(*setStencil)(GfxStencilState::Bits frontStencil, GfxStencilState::Bits backStencil/* = GfxStencilState::None*/);
        uint16_t(*setScissor)(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
        void(*setScissorCache)(uint16_t cache);

        // Transform
        uint32_t(*allocTransform)(GpuTransform* transform, uint16_t num/* = 1*/);
        uint32_t(*setTransform)(const void* mtx, uint16_t num/* = 1*/);
        void(*setTransformCached)(uint32_t cache, uint16_t num/* = 1*/);

        // Conditional Rendering
        void(*setCondition)(OcclusionQueryHandle handle, bool visible);

        // Buffers
        void(*setIndexBuffer)(IndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices);
        void(*setDynamicIndexBuffer)(DynamicIndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices);
        void(*setTransientIndexBufferI)(const TransientIndexBuffer* tib, uint32_t firstIndex, uint32_t numIndices);
        void(*setTransientIndexBuffer)(const TransientIndexBuffer* tib);
        void(*setVertexBuffer)(VertexBufferHandle handle);
        void(*setVertexBufferI)(VertexBufferHandle handle, uint32_t vertexIndex, uint32_t numVertices);
        void(*setDynamicVertexBuffer)(DynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t numVertices);
        void(*setTransientVertexBuffer)(const TransientVertexBuffer* tvb);
        void(*setTransientVertexBufferI)(const TransientVertexBuffer* tvb, uint32_t startVertex, uint32_t numVertices);
        void(*setInstanceDataBuffer)(const InstanceDataBuffer* idb, uint32_t num);
        void(*setInstanceDataBufferVb)(VertexBufferHandle handle, uint32_t startVertex, uint32_t num);
        void(*setInstanceDataBufferDynamicVb)(DynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t num);

        // Textures
        void(*setTexture)(uint8_t stage, UniformHandle sampler, TextureHandle handle, TextureFlag::Bits flags/* = TextureFlag::FromTexture*/);

        // Submit
        uint32_t(*submit)(uint8_t viewId, ProgramHandle program, int32_t depth/* = 0*/, bool preserveState/* = false*/);
        uint32_t(*submitWithOccQuery)(uint8_t viewId, ProgramHandle program, OcclusionQueryHandle occQuery,
                                      int32_t depth/* = 0*/, bool preserveState/* = false*/);
        uint32_t(*submitIndirect)(uint8_t viewId, ProgramHandle program, IndirectBufferHandle indirectHandle,
                                  uint16_t start, uint16_t num, int32_t depth/* = 0*/, bool preserveState/* = false*/);

        // Compute
        void(*setComputeBufferIb)(uint8_t stage, IndexBufferHandle handle, GpuAccessFlag::Enum access);
        void(*setComputeBufferVb)(uint8_t stage, VertexBufferHandle handle, GpuAccessFlag::Enum access);
        void(*setComputeBufferDynamicIb)(uint8_t stage, DynamicIndexBufferHandle handle, GpuAccessFlag::Enum access);
        void(*setComputeBufferDynamicVb)(uint8_t stage, DynamicVertexBufferHandle handle, GpuAccessFlag::Enum access);
        void(*setComputeBufferIndirect)(uint8_t stage, IndirectBufferHandle handle, GpuAccessFlag::Enum access);

        // Compute Images
        void (*setComputeImage)(uint8_t stage, UniformHandle sampler, TextureHandle handle, uint8_t mip,
                                GpuAccessFlag::Enum access, TextureFormat::Enum fmt);

        // Compute Dispatch
        uint32_t(*computeDispatch)(uint8_t viewId, ProgramHandle handle, uint16_t numX, uint16_t numY, uint16_t numZ,
                                   GfxSubmitFlag::Bits flags/* = GfxSubmitFlag::Left*/);
        uint32_t(*computeDispatchIndirect)(uint8_t viewId, ProgramHandle handle, IndirectBufferHandle indirectHandle,
                                   uint16_t start, uint16_t num, GfxSubmitFlag::Bits flags/* = GfxSubmitFlag::Left*/);

        // Blit
        void (*blit)(uint8_t viewId, TextureHandle dest, uint16_t destX, uint16_t destY, TextureHandle src,
            uint16_t srcX/* = 0*/, uint16_t srcY/* = 0*/, uint16_t width/* = UINT16_MAX*/, uint16_t height/* = UINT16_MAX*/);
        void (*blitMip)(uint8_t viewId, TextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
            uint16_t destZ, TextureHandle src, uint8_t srcMip/* = 0*/, uint16_t srcX/* = 0*/, uint16_t srcY/* = 0*/,
            uint16_t srcZ/* = 0*/, uint16_t width/* = UINT16_MAX*/, uint16_t height/* = UINT16_MAX*/,
            uint16_t depth/* = UINT16_MAX*/);
        
        // Memory
        const GfxMemory* (*alloc)(uint32_t size);
        const GfxMemory* (*copy)(const void* data, uint32_t size);
        const GfxMemory* (*makeRef)(const void* data, uint32_t size, gfxReleaseMemCallback releaseFn/* = nullptr*/,
            void* userData/* = nullptr*/);

        // Shaders and Programs
        ShaderHandle(*createShader)(const GfxMemory* mem);
        uint16_t(*getShaderUniforms)(ShaderHandle handle, UniformHandle* uniforms/* = nullptr*/, uint16_t _max/* = 0*/);
        void(*destroyShader)(ShaderHandle handle);
        ProgramHandle(*createProgram)(ShaderHandle vsh, ShaderHandle fsh, bool destroyShaders);
        void(*destroyProgram)(ProgramHandle handle);
        void(*destroyUniform)(UniformHandle handle);

        // Uniforms
        UniformHandle(*createUniform)(const char* name, UniformType::Enum type, uint16_t num/* = 1*/);
        void(*setUniform)(UniformHandle handle, const void* value, uint16_t num/* = 1*/);

        // Vertex Buffers
        VertexBufferHandle (*createVertexBuffer)(const GfxMemory* mem, const VertexDecl& decl, 
            GpuBufferFlag::Bits flags/* = GpuBufferFlag::None*/) = 0;
        DynamicVertexBufferHandle (*createDynamicVertexBuffer)(uint32_t numVertices, const VertexDecl& decl,
            GpuBufferFlag::Bits flags/* = GpuBufferFlag::None*/);
        DynamicVertexBufferHandle (*createDynamicVertexBufferMem)(const GfxMemory* mem, const VertexDecl& decl,
            GpuBufferFlag::Bits flags/* = GpuBufferFlag::None*/);
        void(*updateDynamicVertexBuffer)(DynamicVertexBufferHandle handle, uint32_t startVertex, const GfxMemory* mem);
        void(*destroyVertexBuffer)(VertexBufferHandle handle);
        void(*destroyDynamicVertexBuffer)(DynamicVertexBufferHandle handle);
        uint32_t(*getAvailTransientVertexBuffer)(uint32_t num, const VertexDecl& decl);
        void(*allocTransientVertexBuffer)(TransientVertexBuffer* tvb, uint32_t num, const VertexDecl& decl);

        // Index buffers
        IndexBufferHandle(*createIndexBuffer)(const GfxMemory* mem, GpuBufferFlag::Bits flags/* = GpuBufferFlag::None*/);
        DynamicIndexBufferHandle(*createDynamicIndexBuffer)(uint32_t num, GpuBufferFlag::Bits flags/* = GpuBufferFlag::None*/);
        DynamicIndexBufferHandle(*createDynamicIndexBufferMem)(const GfxMemory* mem, GpuBufferFlag::Bits flags/* = GpuBufferFlag::None*/);
        void(*updateDynamicIndexBuffer)(DynamicIndexBufferHandle handle, uint32_t startIndex, const GfxMemory* mem);
        void(*destroyIndexBuffer)(IndexBufferHandle handle);
        void(*destroyDynamicIndexBuffer)(DynamicIndexBufferHandle handle);
        uint32_t(*getAvailTransientIndexBuffer)(uint32_t num);
        void(*allocTransientIndexBuffer)(TransientIndexBuffer* tib, uint32_t num);

        // Textures
        void(*calcTextureSize)(TextureInfo* info, uint16_t width, uint16_t height, uint16_t depth,
            bool cubemap, bool hasMips, uint16_t numLayers, TextureFormat::Enum fmt);
        TextureHandle(*createTexture)(const GfxMemory* mem, TextureFlag::Bits flags, uint8_t skipMips, TextureInfo* info);
        TextureHandle(*createTexture2D)(uint16_t width, uint16_t height, bool hasMips, uint16_t numLayers,
            TextureFormat::Enum fmt, TextureFlag::Bits flags/* = TextureFlag::None*/, const GfxMemory* mem/* = nullptr*/);
        TextureHandle(*createTexture2DRatio)(BackbufferRatio::Enum ratio, bool hasMips, uint16_t numLayers, TextureFormat::Enum fmt,
            TextureFlag::Bits flags/* = TextureFlag::None*/);

        void (*updateTexture2D)(TextureHandle handle, uint16_t layer, uint8_t mip, uint16_t x, uint16_t y, uint16_t width,
                                uint16_t height, const GfxMemory* mem, uint16_t pitch/* = UINT16_MAX*/) = 0;
        TextureHandle(*createTexture3D)(uint16_t width, uint16_t height, uint16_t depth, bool hasMips,
                                        TextureFormat::Enum fmt, TextureFlag::Bits flags,
                                        const GfxMemory* mem/* = nullptr*/);
        void(*updateTexture3D)(TextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t z,
                               uint16_t width, uint16_t height, uint16_t depth, const GfxMemory* mem);
        TextureHandle(*createTextureCube)(uint16_t size, bool hasMips, uint16_t numLayers, TextureFormat::Enum fmt,
                                          TextureFlag::Bits flags, const GfxMemory* mem/* = nullptr*/);
        void(*updateTextureCube)(TextureHandle handle, uint16_t layer, CubeSide::Enum side, uint8_t mip, uint16_t x, uint16_t y,
                                 uint16_t width, uint16_t height, const GfxMemory* mem, uint16_t pitch);
        void(*readTexture)(TextureHandle handle, void* data, uint8_t mip/* = 0*/);
        void(*destroyTexture)(TextureHandle handle);

        // Frame Buffers
        FrameBufferHandle(*createFrameBuffer)(uint16_t width, uint16_t height, TextureFormat::Enum fmt, TextureFlag::Bits flags/* = TextureFlag::U_Clamp | TextureFlag::V_Clamp*/);
        FrameBufferHandle(*createFrameBufferRatio)(BackbufferRatio::Enum ratio, TextureFormat::Enum fmt, TextureFlag::Bits flags/* = TextureFlag::U_Clamp | TextureFlag::V_Clamp*/);
        FrameBufferHandle(*createFrameBufferMRT)(uint8_t num, const TextureHandle* handles, bool destroyTextures/* = false*/);
        FrameBufferHandle(*createFrameBufferAttachment)(uint8_t num, const GfxAttachment* attachment, bool destroyTextures/* = false*/);
        FrameBufferHandle(*createFrameBufferNative)(void* nwh, uint16_t width, uint16_t height, TextureFormat::Enum depthFmt/* = TextureFormat::UnknownDepth*/);
        void(*destroyFrameBuffer)(FrameBufferHandle handle);
        TextureHandle(*getFrameBufferTexture)(FrameBufferHandle handle, uint8_t attachment/* = 0*/);

        // Instance Buffer
        uint32_t(*getAvailInstanceDataBuffer)(uint32_t num, uint16_t stride);
        const InstanceDataBuffer* (*allocInstanceDataBuffer)(uint32_t num, uint16_t stride);

        // Indirect Buffer
        IndirectBufferHandle(*createIndirectBuffer)(uint32_t num);
        void(*destroyIndirectBuffer)(IndirectBufferHandle handle);
        
        // Occlusion Query
        OcclusionQueryHandle(*createOccQuery)();
        OcclusionQueryResult::Enum(*getResult)(OcclusionQueryHandle handle);
        void(*destroyOccQuery)(OcclusionQueryHandle handle);

        // Debug
        void(*dbgTextClear)(uint8_t attr, bool small/* = false*/);
        void(*dbgTextPrintf)(uint16_t x, uint16_t y, uint8_t attr, const char* format, ...);
        void(*dbgTextImage)(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const void* data, uint16_t pitch);
    };
}   // namespace termite

