#pragma once

#include <cstdarg>
#include "bx/bx.h"

#include "gfx_defines.h"

namespace termite
{
    VertexDecl* vdeclBegin(VertexDecl* vdecl, RendererType _type = RendererType::Null);
    void vdeclEnd(VertexDecl* vdecl);
    VertexDecl* vdeclAdd(VertexDecl* vdecl, VertexAttrib _attrib, uint8_t _num, VertexAttribType _type, bool _normalized = false, bool _asInt = false);
    VertexDecl* vdeclSkip(VertexDecl* vdecl, uint8_t _numBytes);
    void vdeclDecode(VertexDecl* vdecl, VertexAttrib _attrib, uint8_t* _num, VertexAttribType* _type, bool* _normalized, bool* _asInt);
    bool vdeclHas(VertexDecl* vdecl, VertexAttrib _attrib);
    uint32_t vdeclGetSize(VertexDecl* vdecl, uint32_t _num);

    class BX_NO_VTABLE GfxDriverEventsI
    {
    public:
        virtual void onFatal(GfxFatalType type, const char* str) = 0;
        virtual void onTraceVargs(const char* filepath, int line, const char* format, va_list argList) = 0;
        virtual uint32_t onCacheReadSize(uint64_t id) = 0;
        virtual bool onCacheRead(uint64_t id, void* data, uint32_t size) = 0;
        virtual void onCacheWrite(uint64_t id, const void* data, uint32_t size) = 0;
        virtual void onScreenShot(const char *filePath, uint32_t width, uint32_t height, uint32_t pitch, 
                                  const void *data, uint32_t size, bool yflip) = 0;
        virtual void onCaptureBegin(uint32_t width, uint32_t height, uint32_t pitch, TextureFormat fmt, bool yflip) = 0;
        virtual void onCaptureEnd() = 0;
        virtual void onCaptureFrame(const void* data, uint32_t size) = 0;
    };

    typedef void (*gfxReleaseMemCallback)(void* ptr, void* userData);

    class BX_NO_VTABLE GfxDriverI
    {
    public:
        // Init
        virtual result_t init(uint16_t deviceId, GfxDriverEventsI* callbacks, bx::AllocatorI* alloc) = 0;
        virtual void shutdown() = 0;

        virtual void reset(uint32_t width, uint32_t height, GfxResetFlag flags = GfxResetFlag::None) = 0;
        virtual uint32_t frame() = 0;
        virtual void setDebug(GfxDebugFlag debugFlags) = 0;
        virtual RendererType getRendererType() const = 0;
        virtual const GfxCaps& getCaps() = 0;
        virtual const GfxStats& getStats() = 0;
        virtual const HMDDesc& getHMD() = 0;

        // Platform specific
        virtual RenderFrameType renderFrame() = 0;
        virtual void setPlatformData(const GfxPlatformData& data) = 0;
        virtual const GfxInternalData& getInternalData() = 0;
        virtual void overrideInternal(TextureHandle handle, uintptr_t ptr) = 0;
        virtual void overrideInternal(TextureHandle handle, uint16_t width, uint16_t height, uint8_t numMips,
                                      TextureFormat fmt, TextureFlag flags) = 0;

        // Misc
        virtual void discard() = 0;
        virtual uint32_t touch(uint8_t id) = 0;
        virtual void setPaletteColor(uint8_t index, uint32_t rgba) = 0;
        virtual void setPaletteColor(uint8_t index, float rgba[4]) = 0;
        virtual void setPaletteColor(uint8_t index, float r, float g, float b, float a) = 0;
        virtual void saveScreenshot(const char* filepath) = 0;

        // Views
        virtual void setViewName(uint8_t id, const char* name) = 0;
        virtual void setViewRect(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height) = 0;
        virtual void setViewRect(uint8_t id, uint16_t x, uint16_t y, BackbufferRatio ratio) = 0;
        virtual void setViewScissor(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height) = 0;
        virtual void setViewClear(uint8_t id, GfxClearFlag flags, uint32_t rgba, float depth, uint8_t stencil) = 0;
        virtual void setViewClear(uint8_t id, GfxClearFlag flags, float depth, uint8_t stencil,
                                  uint8_t color0, uint8_t color1, uint8_t color2, uint8_t color3,
                                  uint8_t color4, uint8_t color5, uint8_t color6, uint8_t color7) = 0;
        virtual void setViewSeq(uint8_t id, bool enabled) = 0;
        virtual void setViewTransform(uint8_t id, const void* view, const void* projLeft, 
                                      GfxViewFlag flags = GfxViewFlag::Stereo, const void* projRight = nullptr) = 0;
        virtual void setViewRemap(uint8_t id, uint8_t num, const void* remap) = 0;
        virtual void setViewFrameBuffer(uint8_t id, FrameBufferHandle handle) = 0;

        // Draw
        virtual void setMarker(const char* marker) = 0;
        virtual void setState(GfxState state, uint32_t rgba = 0) = 0;
        virtual void setStencil(GfxStencilState frontStencil, GfxStencilState backStencil = GfxStencilState::None) = 0;
        virtual uint16_t setScissor(uint16_t x, uint16_t y, uint16_t width, uint16_t height) = 0;
        virtual void setScissor(uint16_t cache) = 0;

        // Transform
        virtual uint32_t allocTransform(GpuTransform* transform, uint16_t num) = 0;
        virtual uint32_t setTransform(const void* mtx, uint16_t num) = 0;

        // Conditional Rendering
        virtual void setCondition(OcclusionQueryHandle handle, bool visible) = 0;

        // Buffers
        virtual void setIndexBuffer(IndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices) = 0;
        virtual void setIndexBuffer(DynamicIndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices) = 0;
        virtual void setIndexBuffer(const TransientIndexBuffer* tib, uint32_t firstIndex, uint32_t numIndices) = 0;
        virtual void setIndexBuffer(const TransientIndexBuffer* tib) = 0;
        virtual void setVertexBuffer(VertexBufferHandle handle) = 0;
        virtual void setVertexBuffer(VertexBufferHandle handle, uint32_t vertexIndex, uint32_t numVertices) = 0;
        virtual void setVertexBuffer(DynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t numVertices) = 0;
        virtual void setVertexBuffer(const TransientVertexBuffer* tvb) = 0;
        virtual void setVertexBuffer(const TransientVertexBuffer* tvb, uint32_t startVertex, uint32_t numVertices) = 0;
        virtual void setInstanceDataBuffer(const InstanceDataBuffer* idb, uint32_t num) = 0;
        virtual void setInstanceDataBuffer(VertexBufferHandle handle, uint32_t startVertex, uint32_t num) = 0;
        virtual void setInstanceDataBuffer(DynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t num) = 0;

        // Textures
        virtual void setTexture(uint8_t stage, UniformHandle sampler, TextureHandle handle, 
                                TextureFlag flags = TextureFlag::FromTexture) = 0;
        virtual void setTexture(uint8_t stage, UniformHandle sampler, FrameBufferHandle handle,
                                uint8_t attachment, TextureFlag flags = TextureFlag::FromTexture) = 0;

        // Submit
        virtual uint32_t submit(uint8_t viewId, ProgramHandle program, int32_t depth = 0, bool preserveState = false) = 0;
        virtual uint32_t submit(uint8_t viewId, ProgramHandle program, OcclusionQueryHandle occQuery, int32_t depth = 0,
                                bool preserveState = false) = 0;
        virtual uint32_t submit(uint8_t viewId, ProgramHandle program, IndirectBufferHandle indirectHandle,
                                uint16_t start, uint16_t num, int32_t depth = 0, bool preserveState = false) = 0;

        // Compute
        virtual void setBuffer(uint8_t stage, IndexBufferHandle handle, GpuAccessFlag access) = 0;
        virtual void setBuffer(uint8_t stage, VertexBufferHandle handle, GpuAccessFlag access) = 0;
        virtual void setBuffer(uint8_t stage, DynamicIndexBufferHandle handle, GpuAccessFlag access) = 0;
        virtual void setBuffer(uint8_t stage, DynamicVertexBufferHandle handle, GpuAccessFlag access) = 0;
        virtual void setBuffer(uint8_t stage, IndirectBufferHandle handle, GpuAccessFlag access) = 0;

        // Compute Images
        virtual void setImage(uint8_t stage, UniformHandle sampler, TextureHandle handle, uint8_t mip,
                              GpuAccessFlag access, TextureFormat fmt) = 0;
        virtual void setImage(uint8_t stage, UniformHandle sampler, FrameBufferHandle handle, uint8_t attachment,
                              GpuAccessFlag access, TextureFormat fmt) = 0;

        // Compute Dispatch
        virtual uint32_t dispatch(uint8_t viewId, ProgramHandle handle, uint16_t numX, uint16_t numY, uint16_t numZ, 
                                  GfxSubmitFlag flags = GfxSubmitFlag::Left) = 0;
        virtual uint32_t dispatch(uint8_t viewId, ProgramHandle handle, IndirectBufferHandle indirectHandle,
                                  uint16_t start, uint16_t num, GfxSubmitFlag flags = GfxSubmitFlag::Left) = 0;

        // Blit
        virtual void blit(uint8_t viewId, TextureHandle dest, uint16_t destX, uint16_t destY, TextureHandle src,
                          uint16_t srcX = 0, uint16_t srcY = 0, uint16_t width = UINT16_MAX, uint16_t height = UINT16_MAX) = 0;
        virtual void blit(uint8_t viewId, TextureHandle dest, uint16_t destX, uint16_t destY, FrameBufferHandle src,
                          uint8_t attachment = 0, uint16_t srcX = 0, uint16_t srcY = 0, uint16_t width = UINT16_MAX, 
                          uint16_t height = UINT16_MAX) = 0;
        virtual void blit(uint8_t viewId, TextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
                          uint16_t destZ, TextureHandle src, uint8_t srcMip = 0, uint16_t srcX = 0, uint16_t srcY = 0,
                          uint16_t srcZ = 0, uint16_t width = UINT16_MAX, uint16_t height = UINT16_MAX, 
                          uint16_t depth = UINT16_MAX) = 0;
        virtual void blit(uint8_t viewId, TextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
                          uint16_t destZ, FrameBufferHandle src, uint8_t attachment = 0, uint8_t srcMip = 0, 
                          uint16_t srcX = 0, uint16_t srcY = 0, uint16_t srcZ = 0, uint16_t width = UINT16_MAX, 
                          uint16_t height = UINT16_MAX, uint16_t depth = UINT16_MAX) = 0;
        
        // Memory
        virtual const GfxMemory* alloc(uint32_t size) = 0;
        virtual const GfxMemory* copy(const void* data, uint32_t size) = 0;
        virtual const GfxMemory* makeRef(const void* data, uint32_t size, gfxReleaseMemCallback releaseFn = nullptr, 
                                         void* userData = nullptr) = 0;

        // Shaders and Programs
        virtual ShaderHandle createShader(const GfxMemory* mem) = 0;
        virtual uint16_t getShaderUniforms(ShaderHandle handle, UniformHandle* uniforms = nullptr, uint16_t _max = 0) = 0;
        virtual void destroyShader(ShaderHandle handle) = 0;
        virtual ProgramHandle createProgram(ShaderHandle vsh, ShaderHandle fsh, bool destroyShaders) = 0;
        virtual void destroyProgram(ProgramHandle handle) = 0;
        virtual void destroyUniform(UniformHandle handle) = 0;

        // Uniforms
        virtual UniformHandle createUniform(const char* name, UniformType type, uint16_t num = 1) = 0;
        virtual void setUniform(UniformHandle handle, const void* value, uint16_t num = 1) = 0;

        // Vertex Buffers
        virtual VertexBufferHandle createVertexBuffer(const GfxMemory* mem, const VertexDecl& decl, 
                                                         GpuBufferFlag flags = GpuBufferFlag::None) = 0;
        virtual DynamicVertexBufferHandle createDynamicVertexBuffer(uint32_t numVertices, const VertexDecl& decl,
                                                                       GpuBufferFlag flags = GpuBufferFlag::None) = 0;
        virtual DynamicVertexBufferHandle createDynamicVertexBuffer(const GfxMemory* mem, const VertexDecl& decl,
                                                                       GpuBufferFlag flags = GpuBufferFlag::None) = 0;
        virtual void updateDynamicVertexBuffer(DynamicVertexBufferHandle handle, uint32_t startVertex, 
                                               const GfxMemory* mem) = 0;
        virtual void destroyVertexBuffer(VertexBufferHandle handle) = 0;
        virtual void destroyDynamicVertexBuffer(DynamicVertexBufferHandle handle) = 0;
        virtual bool checkAvailTransientVertexBuffer(uint32_t num, const VertexDecl& decl) = 0;
        virtual void allocTransientVertexBuffer(TransientVertexBuffer* tvb, uint32_t num, const VertexDecl& decl) = 0;

        // Index buffers
        virtual IndexBufferHandle createIndexBuffer(const GfxMemory* mem, GpuBufferFlag flags = GpuBufferFlag::None) = 0;
        virtual DynamicIndexBufferHandle createDynamicIndexBuffer(uint32_t num, GpuBufferFlag flags = GpuBufferFlag::None) = 0;
        virtual DynamicIndexBufferHandle createDynamicIndexBuffer(const GfxMemory* mem, GpuBufferFlag flags = GpuBufferFlag::None) = 0;
        virtual void updateDynamicIndexBuffer(DynamicIndexBufferHandle handle, uint32_t startIndex, const GfxMemory* mem) = 0;
        virtual void destroyIndexBuffer(IndexBufferHandle handle) = 0;
        virtual void destroyDynamicIndexBuffer(DynamicIndexBufferHandle handle) = 0;
        virtual bool checkAvailTransientIndexBuffer(uint32_t num) = 0;
        virtual void allocTransientIndexBuffer(TransientIndexBuffer* tib, uint32_t num) = 0;

        // Textures
        virtual void calcTextureSize(TextureInfo* info, uint16_t width, uint16_t height, uint16_t depth,
                                     bool cubemap, uint8_t numMips, TextureFormat fmt) = 0;
        virtual TextureHandle createTexture(const GfxMemory* mem, TextureFlag flags, uint8_t skipMips, 
                                               TextureInfo* info) = 0;
        virtual TextureHandle createTexture2D(uint16_t width, uint16_t height, uint8_t numMips, TextureFormat fmt,
                                                 TextureFlag flags, const GfxMemory* mem = nullptr) = 0;
        virtual TextureHandle createTexture2D(BackbufferRatio ratio, uint8_t numMips, TextureFormat fmt,
                                                 TextureFlag flags) = 0;

        virtual void updateTexture2D(TextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t width,
                                     uint16_t height, const GfxMemory* mem, uint16_t pitch) = 0;
        virtual TextureHandle createTexture3D(uint16_t width, uint16_t height, uint16_t depth, uint8_t numMips,
                                                 TextureFormat fmt, TextureFlag flags, 
                                                 const GfxMemory* mem = nullptr) = 0;
        virtual void updateTexture3D(TextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t z,
                                     uint16_t width, uint16_t height, uint16_t depth, const GfxMemory* mem) = 0;
        virtual TextureHandle createTextureCube(uint16_t size, uint8_t numMips, TextureFormat fmt, 
                                                   TextureFlag flags, const GfxMemory* mem = nullptr) = 0;
        virtual void updateTextureCube(TextureHandle handle, CubeSide side, uint8_t mip, uint16_t x, uint16_t y,
                                       uint16_t width, uint16_t height, const GfxMemory* mem, uint16_t pitch) = 0;
        virtual void readTexture(TextureHandle handle, void* data) = 0;
        virtual void readTexture(FrameBufferHandle handle, uint8_t attachment, void* data) = 0;
        virtual void destroyTexture(TextureHandle handle) = 0;

        // Frame Buffers
        virtual FrameBufferHandle createFrameBuffer(uint16_t width, uint16_t height, TextureFormat fmt, 
                                                       TextureFlag flags) = 0;
        virtual FrameBufferHandle createFrameBuffer(BackbufferRatio ratio, TextureFormat fmt, 
                                                       TextureFlag flags) = 0;
        virtual FrameBufferHandle createFrameBuffer(uint8_t num, const TextureHandle* handles, 
                                                       bool destroyTextures) = 0;
        virtual FrameBufferHandle createFrameBuffer(void* nwh, uint16_t width, uint16_t height, 
                                                       TextureFormat depthFmt) = 0;
        virtual void destroyFrameBuffer(FrameBufferHandle handle) = 0;

        // Instance Buffer
        virtual bool checkAvailInstanceDataBuffer(uint32_t num, uint16_t stride) = 0;
        virtual const InstanceDataBuffer* allocInstanceDataBuffer(uint32_t num, uint16_t stride) = 0;

        // Indirect Buffer
        virtual IndirectBufferHandle createIndirectBuffer(uint32_t num) = 0;
        virtual void destroyIndirectBuffer(IndirectBufferHandle handle) = 0;
        
        // Occlusion Query
        virtual OcclusionQueryHandle createOccQuery() = 0;
        virtual OcclusionQueryResult getResult(OcclusionQueryHandle handle) = 0;
        virtual void destroyOccQuery(OcclusionQueryHandle handle) = 0;

        // Debug
        virtual void dbgTextClear(uint8_t attr, bool small = false) = 0;
        virtual void dbgTextPrintf(uint16_t x, uint16_t y, uint8_t attr, const char* format, ...) = 0;
        virtual void dbgTextImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const void* data, uint16_t pitch) = 0;
    };
}   // namespace termite

