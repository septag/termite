#pragma once

#include <cstdarg>
#include "bx/bx.h"

#include "gfx_defines.h"

namespace termite
{
    class TERMITE_API gfxVertexDecl
    {
    public:
        uint32_t hash;
        uint16_t stride;
        uint16_t offset[int(gfxAttrib::Count)];
        uint16_t attribs[int(gfxAttrib::Count)];

        //
        gfxVertexDecl();
        gfxVertexDecl& begin(gfxRendererType _type = gfxRendererType::Null);
        void end();

        gfxVertexDecl& add(gfxAttrib _attrib, uint8_t _num, gfxAttribType _type, bool _normalized = false, bool _asInt = false);
        gfxVertexDecl& skip(uint8_t _numBytes);
        void decode(gfxAttrib _attrib, uint8_t* _num, gfxAttribType* _type, bool* _normalized, bool* _asInt) const;
        bool has(gfxAttrib _attrib) const
        {
            return attribs[(int)_attrib] != UINT16_MAX;
        }

        uint32_t getSize(uint32_t _num) const
        {
            return _num*stride;
        }
    };

    class BX_NO_VTABLE gfxCallbacksI
    {
    public:
        virtual void onFatal(gfxFatalType type, const char* str) = 0;
        virtual void onTraceVargs(const char* filepath, int line, const char* format, va_list argList) = 0;
        virtual uint32_t onCacheReadSize(uint64_t id) = 0;
        virtual bool onCacheRead(uint64_t id, void* data, uint32_t size) = 0;
        virtual void onCacheWrite(uint64_t id, const void* data, uint32_t size) = 0;
        virtual void onScreenShot(const char *filePath, uint32_t width, uint32_t height, uint32_t pitch, 
                                  const void *data, uint32_t size, bool yflip) = 0;
        virtual void onCaptureBegin(uint32_t width, uint32_t height, uint32_t pitch, gfxTextureFormat fmt, bool yflip) = 0;
        virtual void onCaptureEnd() = 0;
        virtual void onCaptureFrame(const void* data, uint32_t size) = 0;
    };

    typedef void (*gfxCallbackReleaseMem)(void* ptr, void* userData);

    class BX_NO_VTABLE gfxDriverI
    {
    public:
        // Init
        virtual result_t init(uint16_t deviceId, gfxCallbacksI* callbacks, bx::AllocatorI* alloc) = 0;
        virtual void shutdown() = 0;

        virtual void reset(uint32_t width, uint32_t height, gfxResetFlag flags = gfxResetFlag::None) = 0;
        virtual uint32_t frame() = 0;
        virtual void setDebug(gfxDebugFlag debugFlags) = 0;
        virtual gfxRendererType getRendererType() const = 0;
        virtual const gfxCaps& getCaps() = 0;
        virtual const gfxStats& getStats() = 0;
        virtual const gfxHMD& getHMD() = 0;

        // Platform specific
        virtual gfxRenderFrameType renderFrame() = 0;
        virtual void setPlatformData(const gfxPlatformData& data) = 0;
        virtual const gfxInternalData& getInternalData() = 0;
        virtual void overrideInternal(gfxTextureHandle handle, uintptr_t ptr) = 0;
        virtual void overrideInternal(gfxTextureHandle handle, uint16_t width, uint16_t height, uint8_t numMips,
                                      gfxTextureFormat fmt, gfxTextureFlag flags) = 0;

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
        virtual void setViewRect(uint8_t id, uint16_t x, uint16_t y, gfxBackbufferRatio ratio) = 0;
        virtual void setViewScissor(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height) = 0;
        virtual void setViewClear(uint8_t id, gfxClearFlag flags, uint32_t rgba, float depth, uint8_t stencil) = 0;
        virtual void setViewClear(uint8_t id, gfxClearFlag flags, float depth, uint8_t stencil,
                                  uint8_t color0, uint8_t color1, uint8_t color2, uint8_t color3,
                                  uint8_t color4, uint8_t color5, uint8_t color6, uint8_t color7) = 0;
        virtual void setViewSeq(uint8_t id, bool enabled) = 0;
        virtual void setViewTransform(uint8_t id, const void* view, const void* projLeft, 
                                      gfxViewFlag flags = gfxViewFlag::Stereo, const void* projRight = nullptr) = 0;
        virtual void setViewRemap(uint8_t id, uint8_t num, const void* remap) = 0;
        virtual void setViewFrameBuffer(uint8_t id, gfxFrameBufferHandle handle) = 0;

        // Draw
        virtual void setMarker(const char* marker) = 0;
        virtual void setState(gfxState state, uint32_t rgba = 0) = 0;
        virtual void setStencil(gfxStencil frontStencil, gfxStencil backStencil = gfxStencil::None) = 0;
        virtual uint16_t setScissor(uint16_t x, uint16_t y, uint16_t width, uint16_t height) = 0;
        virtual void setScissor(uint16_t cache) = 0;

        // Transform
        virtual uint32_t allocTransform(gfxTransform* transform, uint16_t num) = 0;
        virtual uint32_t setTransform(const void* mtx, uint16_t num) = 0;

        // Conditional Rendering
        virtual void setCondition(gfxOccQueryHandle handle, bool visible) = 0;

        // Buffers
        virtual void setIndexBuffer(gfxIndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices) = 0;
        virtual void setIndexBuffer(gfxDynamicIndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices) = 0;
        virtual void setIndexBuffer(const gfxTransientIndexBuffer* tib, uint32_t firstIndex, uint32_t numIndices) = 0;
        virtual void setIndexBuffer(const gfxTransientIndexBuffer* tib) = 0;
        virtual void setVertexBuffer(gfxVertexBufferHandle handle) = 0;
        virtual void setVertexBuffer(gfxVertexBufferHandle handle, uint32_t vertexIndex, uint32_t numVertices) = 0;
        virtual void setVertexBuffer(gfxDynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t numVertices) = 0;
        virtual void setVertexBuffer(const gfxTransientVertexBuffer* tvb) = 0;
        virtual void setVertexBuffer(const gfxTransientVertexBuffer* tvb, uint32_t startVertex, uint32_t numVertices) = 0;
        virtual void setInstanceDataBuffer(const gfxInstanceDataBuffer* idb, uint32_t num) = 0;
        virtual void setInstanceDataBuffer(gfxVertexBufferHandle handle, uint32_t startVertex, uint32_t num) = 0;
        virtual void setInstanceDataBuffer(gfxDynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t num) = 0;

        // Textures
        virtual void setTexture(uint8_t stage, gfxUniformHandle sampler, gfxTextureHandle handle, 
                                gfxTextureFlag flags = gfxTextureFlag::FromTexture) = 0;
        virtual void setTexture(uint8_t stage, gfxUniformHandle sampler, gfxFrameBufferHandle handle,
                                uint8_t attachment, gfxTextureFlag flags = gfxTextureFlag::FromTexture) = 0;

        // Submit
        virtual uint32_t submit(uint8_t viewId, gfxProgramHandle program, int32_t depth = 0, bool preserveState = false) = 0;
        virtual uint32_t submit(uint8_t viewId, gfxProgramHandle program, gfxOccQueryHandle occQuery, int32_t depth = 0,
                                bool preserveState = false) = 0;
        virtual uint32_t submit(uint8_t viewId, gfxProgramHandle program, gfxIndirectBufferHandle indirectHandle,
                                uint16_t start, uint16_t num, int32_t depth = 0, bool preserveState = false) = 0;

        // Compute
        virtual void setBuffer(uint8_t stage, gfxIndexBufferHandle handle, gfxAccess access) = 0;
        virtual void setBuffer(uint8_t stage, gfxVertexBufferHandle handle, gfxAccess access) = 0;
        virtual void setBuffer(uint8_t stage, gfxDynamicIndexBufferHandle handle, gfxAccess access) = 0;
        virtual void setBuffer(uint8_t stage, gfxDynamicVertexBufferHandle handle, gfxAccess access) = 0;
        virtual void setBuffer(uint8_t stage, gfxIndirectBufferHandle handle, gfxAccess access) = 0;

        // Compute Images
        virtual void setImage(uint8_t stage, gfxUniformHandle sampler, gfxTextureHandle handle, uint8_t mip,
                              gfxAccess access, gfxTextureFormat fmt) = 0;
        virtual void setImage(uint8_t stage, gfxUniformHandle sampler, gfxFrameBufferHandle handle, uint8_t attachment,
                              gfxAccess access, gfxTextureFormat fmt) = 0;

        // Compute Dispatch
        virtual uint32_t dispatch(uint8_t viewId, gfxProgramHandle handle, uint16_t numX, uint16_t numY, uint16_t numZ, 
                                  gfxSubmitFlag flags = gfxSubmitFlag::Left) = 0;
        virtual uint32_t dispatch(uint8_t viewId, gfxProgramHandle handle, gfxIndirectBufferHandle indirectHandle,
                                  uint16_t start, uint16_t num, gfxSubmitFlag flags = gfxSubmitFlag::Left) = 0;

        // Blit
        virtual void blit(uint8_t viewId, gfxTextureHandle dest, uint16_t destX, uint16_t destY, gfxTextureHandle src,
                          uint16_t srcX = 0, uint16_t srcY = 0, uint16_t width = UINT16_MAX, uint16_t height = UINT16_MAX) = 0;
        virtual void blit(uint8_t viewId, gfxTextureHandle dest, uint16_t destX, uint16_t destY, gfxFrameBufferHandle src,
                          uint8_t attachment = 0, uint16_t srcX = 0, uint16_t srcY = 0, uint16_t width = UINT16_MAX, 
                          uint16_t height = UINT16_MAX) = 0;
        virtual void blit(uint8_t viewId, gfxTextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
                          uint16_t destZ, gfxTextureHandle src, uint8_t srcMip = 0, uint16_t srcX = 0, uint16_t srcY = 0,
                          uint16_t srcZ = 0, uint16_t width = UINT16_MAX, uint16_t height = UINT16_MAX, 
                          uint16_t depth = UINT16_MAX) = 0;
        virtual void blit(uint8_t viewId, gfxTextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
                          uint16_t destZ, gfxFrameBufferHandle src, uint8_t attachment = 0, uint8_t srcMip = 0, 
                          uint16_t srcX = 0, uint16_t srcY = 0, uint16_t srcZ = 0, uint16_t width = UINT16_MAX, 
                          uint16_t height = UINT16_MAX, uint16_t depth = UINT16_MAX) = 0;
        
        // Memory
        virtual const gfxMemory* alloc(uint32_t size) = 0;
        virtual const gfxMemory* copy(const void* data, uint32_t size) = 0;
        virtual const gfxMemory* makeRef(const void* data, uint32_t size, gfxCallbackReleaseMem releaseFn = nullptr, 
                                         void* userData = nullptr) = 0;

        // Shaders and Programs
        virtual gfxShaderHandle createShader(const gfxMemory* mem) = 0;
        virtual uint16_t getShaderUniforms(gfxShaderHandle handle, gfxUniformHandle* uniforms = nullptr, uint16_t _max = 0) = 0;
        virtual void destroyShader(gfxShaderHandle handle) = 0;
        virtual gfxProgramHandle createProgram(gfxShaderHandle vsh, gfxShaderHandle fsh, bool destroyShaders) = 0;
        virtual void destroyProgram(gfxProgramHandle handle) = 0;
        virtual void destroyUniform(gfxUniformHandle handle) = 0;

        // Uniforms
        virtual gfxUniformHandle createUniform(const char* name, gfxUniformType type, uint16_t num = 1) = 0;
        virtual void setUniform(gfxUniformHandle handle, const void* value, uint16_t num = 1) = 0;

        // Vertex Buffers
        virtual gfxVertexBufferHandle createVertexBuffer(const gfxMemory* mem, const gfxVertexDecl& decl, 
                                                         gfxBufferFlag flags = gfxBufferFlag::None) = 0;
        virtual gfxDynamicVertexBufferHandle createDynamicVertexBuffer(uint32_t numVertices, const gfxVertexDecl& decl,
                                                                       gfxBufferFlag flags = gfxBufferFlag::None) = 0;
        virtual gfxDynamicVertexBufferHandle createDynamicVertexBuffer(const gfxMemory* mem, const gfxVertexDecl& decl,
                                                                       gfxBufferFlag flags = gfxBufferFlag::None) = 0;
        virtual void updateDynamicVertexBuffer(gfxDynamicVertexBufferHandle handle, uint32_t startVertex, 
                                               const gfxMemory* mem) = 0;
        virtual void destroyVertexBuffer(gfxVertexBufferHandle handle) = 0;
        virtual void destroyDynamicVertexBuffer(gfxDynamicVertexBufferHandle handle) = 0;
        virtual bool checkAvailTransientVertexBuffer(uint32_t num, const gfxVertexDecl& decl) = 0;
        virtual void allocTransientVertexBuffer(gfxTransientVertexBuffer* tvb, uint32_t num, const gfxVertexDecl& decl) = 0;

        // Index buffers
        virtual gfxIndexBufferHandle createIndexBuffer(const gfxMemory* mem, gfxBufferFlag flags = gfxBufferFlag::None) = 0;
        virtual gfxDynamicIndexBufferHandle createDynamicIndexBuffer(uint32_t num, gfxBufferFlag flags = gfxBufferFlag::None) = 0;
        virtual gfxDynamicIndexBufferHandle createDynamicIndexBuffer(const gfxMemory* mem, gfxBufferFlag flags = gfxBufferFlag::None) = 0;
        virtual void updateDynamicIndexBuffer(gfxDynamicIndexBufferHandle handle, uint32_t startIndex, const gfxMemory* mem) = 0;
        virtual void destroyIndexBuffer(gfxIndexBufferHandle handle) = 0;
        virtual void destroyDynamicIndexBuffer(gfxDynamicIndexBufferHandle handle) = 0;
        virtual bool checkAvailTransientIndexBuffer(uint32_t num) = 0;
        virtual void allocTransientIndexBuffer(gfxTransientIndexBuffer* tib, uint32_t num) = 0;

        // Textures
        virtual void calcTextureSize(gfxTextureInfo* info, uint16_t width, uint16_t height, uint16_t depth,
                                     bool cubemap, uint8_t numMips, gfxTextureFormat fmt) = 0;
        virtual gfxTextureHandle createTexture(const gfxMemory* mem, gfxTextureFlag flags, uint8_t skipMips, 
                                               gfxTextureInfo* info) = 0;
        virtual gfxTextureHandle createTexture2D(uint16_t width, uint16_t height, uint8_t numMips, gfxTextureFormat fmt,
                                                 gfxTextureFlag flags, const gfxMemory* mem = nullptr) = 0;
        virtual gfxTextureHandle createTexture2D(gfxBackbufferRatio ratio, uint8_t numMips, gfxTextureFormat fmt,
                                                 gfxTextureFlag flags) = 0;

        virtual void updateTexture2D(gfxTextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t width,
                                     uint16_t height, const gfxMemory* mem, uint16_t pitch) = 0;
        virtual gfxTextureHandle createTexture3D(uint16_t width, uint16_t height, uint16_t depth, uint8_t numMips,
                                                 gfxTextureFormat fmt, gfxTextureFlag flags, 
                                                 const gfxMemory* mem = nullptr) = 0;
        virtual void updateTexture3D(gfxTextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t z,
                                     uint16_t width, uint16_t height, uint16_t depth, const gfxMemory* mem) = 0;
        virtual gfxTextureHandle createTextureCube(uint16_t size, uint8_t numMips, gfxTextureFormat fmt, 
                                                   gfxTextureFlag flags, const gfxMemory* mem = nullptr) = 0;
        virtual void updateTextureCube(gfxTextureHandle handle, gfxCubeSide side, uint8_t mip, uint16_t x, uint16_t y,
                                       uint16_t width, uint16_t height, const gfxMemory* mem, uint16_t pitch) = 0;
        virtual void readTexture(gfxTextureHandle handle, void* data) = 0;
        virtual void readTexture(gfxFrameBufferHandle handle, uint8_t attachment, void* data) = 0;
        virtual void destroyTexture(gfxTextureHandle handle) = 0;

        // Frame Buffers
        virtual gfxFrameBufferHandle createFrameBuffer(uint16_t width, uint16_t height, gfxTextureFormat fmt, 
                                                       gfxTextureFlag flags) = 0;
        virtual gfxFrameBufferHandle createFrameBuffer(gfxBackbufferRatio ratio, gfxTextureFormat fmt, 
                                                       gfxTextureFlag flags) = 0;
        virtual gfxFrameBufferHandle createFrameBuffer(uint8_t num, const gfxTextureHandle* handles, 
                                                       bool destroyTextures) = 0;
        virtual gfxFrameBufferHandle createFrameBuffer(void* nwh, uint16_t width, uint16_t height, 
                                                       gfxTextureFormat depthFmt) = 0;
        virtual void destroyFrameBuffer(gfxFrameBufferHandle handle) = 0;

        // Instance Buffer
        virtual bool checkAvailInstanceDataBuffer(uint32_t num, uint16_t stride) = 0;
        virtual const gfxInstanceDataBuffer* allocInstanceDataBuffer(uint32_t num, uint16_t stride) = 0;

        // Indirect Buffer
        virtual gfxIndirectBufferHandle createIndirectBuffer(uint32_t num) = 0;
        virtual void destroyIndirectBuffer(gfxIndirectBufferHandle handle) = 0;
        
        // Occlusion Query
        virtual gfxOccQueryHandle createOccQuery() = 0;
        virtual gfxOccQueryResult getResult(gfxOccQueryHandle handle) = 0;
        virtual void destroyOccQuery(gfxOccQueryHandle handle) = 0;

        // Debug
        virtual void dbgTextClear(uint8_t attr, bool small = false) = 0;
        virtual void dbgTextPrintf(uint16_t x, uint16_t y, uint8_t attr, const char* format, ...) = 0;
        virtual void dbgTextImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const void* data, uint16_t pitch) = 0;
    };
}   // namespace st

