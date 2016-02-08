#include "stengine/plugins.h"
#include "stengine/gfx_driver.h"
#include "bgfx/bgfx.h"
#include "bgfx/bgfxplatform.h"

using namespace st;

#define BGFX_DECLARE_HANDLE(_Type, _Name, _Handle) bgfx::_Type _Name; _Name.idx = _Handle.idx

class BgfxCallbacks : public bgfx::CallbackI
{
private:
    gfxCallbacks* m_callbacks;

public:
    BgfxCallbacks(gfxCallbacks* callbacks)
    {
        assert(callbacks);
        m_callbacks = callbacks;
    }

    virtual ~BgfxCallbacks()
    {
    }

    /// If fatal code code is not Fatal::DebugCheck this callback is
    /// called on unrecoverable error. It's not safe to continue, inform
    /// user and terminate application from this call.
    ///
    /// @param[in] _code Fatal error code.
    /// @param[in] _str More information about error.
    ///
    /// @remarks
    ///   Not thread safe and it can be called from any thread.
    void fatal(bgfx::Fatal::Enum _code, const char* _str) override
    {
        m_callbacks->onFatal((gfxFatalType)_code, _str);
    }

    /// Print debug message.
    ///
    /// @param[in] _filePath File path where debug message was generated.
    /// @param[in] _line Line where debug message was generated.
    /// @param[in] _format `printf` style format.
    /// @param[in] _argList Variable arguments list initialized with
    ///   `va_start`.
    ///
    /// @remarks
    ///   Not thread safe and it can be called from any thread.
    void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override
    {
        m_callbacks->onTraceVargs(_filePath, _line, _format, _argList);
    }

    /// Return size of for cached item. Return 0 if no cached item was
    /// found.
    ///
    /// @param[in] _id Cache id.
    /// @returns Number of bytes to read.
    uint32_t cacheReadSize(uint64_t _id) override
    {
        m_callbacks->onCacheReadSize(_id);
    }

    /// Read cached item.
    ///
    /// @param[in] _id Cache id.
    /// @param[in] _data Buffer where to read data.
    /// @param[in] _size Size of data to read.
    bool cacheRead(uint64_t _id, void* _data, uint32_t _size) override
    {
        m_callbacks->onCacheRead(_id, _data, _size);
    }

    /// Write cached item.
    ///
    /// @param[in] _id Cache id.
    /// @param[in] _data Data to write.
    /// @param[in] _size Size of data to write.
    void cacheWrite(uint64_t _id, const void* _data, uint32_t _size) override
    {
        m_callbacks->onCacheWrite(_id, _data, _size);
    }

    /// Screenshot captured. Screenshot format is always 4-byte BGRA.
    ///
    /// @param[in] _filePath File path.
    /// @param[in] _width Image width.
    /// @param[in] _height Image height.
    /// @param[in] _pitch Number of bytes to skip to next line.
    /// @param[in] _data Image data.
    /// @param[in] _size Image size.
    /// @param[in] _yflip If true image origin is bottom left.
    void screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data, uint32_t _size, bool _yflip) override
    {
        m_callbacks->onScreenShot(_filePath, _width, _height, _pitch, _data, _size, _yflip);
    }

    /// Called when capture begins.
    void captureBegin(uint32_t _width, uint32_t _height, uint32_t _pitch, bgfx::TextureFormat::Enum _format, bool _yflip) override
    {
        m_callbacks->onCaptureBegin(_width, _height, _pitch, (gfxTextureFormat)_format, _yflip);
    }

    /// Called when capture ends.
    void captureEnd() override
    {
        m_callbacks->onCaptureEnd();
    }

    /// Captured frame.
    ///
    /// @param[in] _data Image data.
    /// @param[in] _size Image size.
    void captureFrame(const void* _data, uint32_t _size) override
    {
        m_callbacks->onCaptureFrame(_data, _size);
    }
};

class BgfxWrapper : public gfxDriver
{
private:
    BgfxCallbacks* m_callbacks;
    bx::AllocatorI* m_alloc;
    gfxCaps m_caps;
    gfxStats m_stats;
    gfxHMD m_hmd;
    gfxInternalData m_internal;

public:
    BgfxWrapper()
    {
        m_callbacks = nullptr;
        m_alloc = nullptr;
        memset(&m_caps, 0x00, sizeof(m_caps));
        memset(&m_stats, 0x00, sizeof(m_stats));
        memset(&m_hmd, 0x00, sizeof(m_hmd));
        memset(&m_internal, 0x00, sizeof(m_internal));
    }

    virtual ~BgfxWrapper()
    {
    }

    bool init(uint16_t deviceId, gfxCallbacks* callbacks, bx::AllocatorI* alloc) override
    {
        m_alloc = alloc;
        if (callbacks) {
            m_callbacks = BX_NEW(alloc, BgfxCallbacks)(callbacks);
            if (!m_callbacks)
                return false;
        }

        return bgfx::init(bgfx::RendererType::Count, 0, deviceId, m_callbacks, alloc);
    }

    void shutdown() override
    {
        bgfx::shutdown();
        if (m_callbacks) {
            BX_DELETE(m_alloc, m_callbacks);
        }
    }

    void reset(uint32_t width, uint32_t height, gfxClearFlag flags) override
    {
        bgfx::reset(width, height, (uint32_t)flags);
    }

    uint32_t frame() override
    {
        return bgfx::frame();
    }

    void setDebug(gfxDebugFlag debugFlags) override
    {
        bgfx::setDebug((uint32_t)debugFlags);
    }

    gfxRendererType getRendererType() const override
    {
        return (gfxRendererType)bgfx::getRendererType();
    }

    const gfxCaps& getCaps() override
    {
        const bgfx::Caps* caps = bgfx::getCaps();
        m_caps.deviceId = caps->deviceId;
        m_caps.supported = (gfxCapsFlag)caps->supported;
        m_caps.maxDrawCalls = caps->maxDrawCalls;
        m_caps.maxFBAttachments = caps->maxFBAttachments;
        m_caps.maxTextureSize = caps->maxTextureSize;
        m_caps.maxViews = caps->maxViews;
        m_caps.numGPUs = caps->numGPUs;
        m_caps.type = (gfxRendererType)caps->rendererType;
        m_caps.vendorId = caps->vendorId;
        
        for (int i = 0; i < 4; i++) {
            m_caps.gpu[i].deviceId = caps->gpu[i].deviceId;
            m_caps.gpu[i].vendorId = caps->gpu[i].vendorId;
        }

        return m_caps;
    }

    const gfxStats& getStats() override
    {
        const bgfx::Stats* stats = bgfx::getStats();
        m_stats.cpuTimeBegin = stats->cpuTimeBegin;
        m_stats.cpuTimeEnd = stats->cpuTimeEnd;
        m_stats.cpuTimerFreq = stats->cpuTimerFreq;
        m_stats.gpuTimeBegin = stats->gpuTimeBegin;
        m_stats.gpuTimeEnd = stats->gpuTimeEnd;
        m_stats.gpuTimerFreq = stats->gpuTimerFreq;
        return m_stats;
    }

    const gfxHMD& getHMD() override
    {
        const bgfx::HMD* hmd = bgfx::getHMD();
        m_hmd.deviceWidth = hmd->deviceWidth;
        m_hmd.deviceHeight = hmd->deviceHeight;
        m_hmd.width = hmd->width;
        m_hmd.height = hmd->height;
        memcpy(m_hmd.eye.rotation, hmd->eye->rotation, sizeof(float) * 4);
        memcpy(m_hmd.eye.translation, hmd->eye->translation, sizeof(float) * 3);
        memcpy(m_hmd.eye.fov, hmd->eye->fov, sizeof(float) * 4);
        memcpy(m_hmd.eye.viewOffset, hmd->eye->viewOffset, sizeof(float) * 3);
        m_hmd.flags = hmd->flags;
        return m_hmd;
    }

    gfxRenderFrameType renderFrame() override
    {
        return (gfxRenderFrameType)bgfx::renderFrame();
    }

    void setPlatformData(const gfxPlatformData& data) override
    {
        bgfx::PlatformData p;
        p.backBuffer = data.backBuffer;
        p.backBufferDS = data.backBufferDS;
        p.context = data.context;
        p.ndt = data.ndt;
        p.nwh = data.nwh;
        bgfx::setPlatformData(p);
    }

    const gfxInternalData& getInternalData() override
    {
        const bgfx::InternalData* d = bgfx::getInternalData();
        m_internal.caps = &getCaps();
        m_internal.context = d->context;
        return m_internal;
    }

    void overrideInternal(gfxTextureHandle handle, uintptr_t ptr) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::overrideInternal(h, ptr);
    }

    void overrideInternal(gfxTextureHandle handle, uint16_t width, uint16_t height, uint8_t numMips,
                          gfxTextureFormat fmt, gfxTextureFlag flags) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::overrideInternal(h, width, height, numMips, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags);
    }

    void discard() override
    {
        bgfx::discard();
    }

    uint32_t touch(uint8_t id) override
    {
        bgfx::touch(id);
    }

    void setPaletteColor(uint8_t index, uint32_t rgba) override
    {
        bgfx::setPaletteColor(index, rgba);
    }

    void setPaletteColor(uint8_t index, float rgba[4]) override
    {
        bgfx::setPaletteColor(index, rgba);
    }

    void setPaletteColor(uint8_t index, float r, float g, float b, float a) override
    {
        bgfx::setPaletteColor(index, r, g, b, a);
    }

    void saveScreenshot(const char* filepath) override
    {
        bgfx::saveScreenShot(filepath);
    }

    void setViewName(uint8_t id, const char* name) override
    {
        bgfx::setViewName(id, name);
    }

    void setViewRect(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height) override
    {
        bgfx::setViewRect(id, x, y, width, height);
    }

    void setViewRect(uint8_t id, uint16_t x, uint16_t y, gfxBackbufferRatio ratio) override
    {
        bgfx::setViewRect(id, x, y, (bgfx::BackbufferRatio::Enum)ratio);
    }

    void setViewScissor(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height) override
    {
        bgfx::setViewScissor(id, x, y, width, height);
    }

    void setViewClear(uint8_t id, gfxClearFlag flags, uint32_t rgba, float depth, uint8_t stencil) override
    {
        bgfx::setViewClear(id, (uint16_t)flags, rgba, depth, stencil);
    }

    void setViewClear(uint8_t id, gfxClearFlag flags, float depth, uint8_t stencil,
                      uint8_t color0, uint8_t color1, uint8_t color2, uint8_t color3,
                      uint8_t color4, uint8_t color5, uint8_t color6, uint8_t color7) override
    {
        bgfx::setViewClear(id, (uint16_t)flags, depth, stencil, color0, color1, color2, color3, color4, color5, color6, 
                           color7);
    }

    void setViewSeq(uint8_t id, bool enabled) override
    {
        bgfx::setViewSeq(id, enabled);
    }

    void setViewTransform(uint8_t id, const void* view, const void* projLeft, gfxViewFlag flags, 
                          const void* projRight) override
    {
        bgfx::setViewTransform(id, view, projLeft, (uint8_t)flags, projRight);
    }

    void setViewRemap(uint8_t id, uint8_t num, const void* remap) override
    {
        bgfx::setViewRemap(id, num, remap);
    }

    void setViewFrameBuffer(uint8_t id, gfxFrameBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
        bgfx::setViewFrameBuffer(id, h);
    }

    void setMarker(const char* marker) override
    {
        bgfx::setMarker(marker);
    }

    void setState(gfxState state, uint32_t rgba) override
    {
        bgfx::setState((uint64_t)state, rgba);
    }

    void setStencil(gfxStencil frontStencil, gfxStencil backStencil) override
    {
        bgfx::setStencil((uint32_t)frontStencil, (uint32_t)backStencil);
    }

    void setScissor(uint16_t x, uint16_t y, uint16_t width, uint16_t height) override
    {
        bgfx::setScissor(x, y, width, height);
    }

    void setScissor(uint16_t cache) override
    {
        bgfx::setScissor(cache);
    }

    uint32_t allocTransform(gfxTransform* transform, uint16_t num) override
    {
        bgfx::Transform t;
        uint32_t r = bgfx::allocTransform(&t, num);
        transform->data = t.data;
        transform->num = t.num;
        return r;
    }

    uint32_t setTransform(const void* mtx, uint16_t num) override
    {
        return bgfx::setTransform(mtx, num);
    }

    void setCondition(gfxOccQueryHandle handle, bool visible) override
    {
        BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
        bgfx::setCondition(h, visible);
    }

    void setIndexBuffer(gfxIndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices) override
    {
        BGFX_DECLARE_HANDLE(IndexBufferHandle, h, handle);
        bgfx::setIndexBuffer(h, firstIndex, numIndices);
    }

    void setIndexBuffer(gfxDynamicIndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices) override
    {
        BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
        bgfx::setIndexBuffer(h, firstIndex, numIndices);
    }

    void setIndexBuffer(const gfxTransientIndexBuffer* tib, uint32_t firstIndex, uint32_t numIndices) override
    {
        bgfx::setIndexBuffer((const bgfx::TransientIndexBuffer*)tib, firstIndex, numIndices);
    }

    void setIndexBuffer(const gfxTransientIndexBuffer* tib) override
    {
        bgfx::setIndexBuffer((const bgfx::TransientIndexBuffer*)tib);
    }

    void setVertexBuffer(gfxVertexBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
        bgfx::setVertexBuffer(h);
    }

    void setVertexBuffer(gfxVertexBufferHandle handle, uint32_t vertexIndex, uint32_t numVertices) override
    {
        BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
        bgfx::setVertexBuffer(h);
    }

    void setVertexBuffer(gfxDynamicVertexBufferHandle handle, uint32_t numVertices) override
    {
        BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
        bgfx::setVertexBuffer(h, numVertices);
    }

    void setVertexBuffer(const gfxTransientVertexBuffer* tvb) override
    {
        bgfx::setVertexBuffer((const bgfx::TransientVertexBuffer*)tvb);
    }

    void setVertexBuffer(const gfxTransientVertexBuffer* tvb, uint32_t startVertex, uint32_t numVertices) override
    {
        bgfx::setVertexBuffer((const bgfx::TransientVertexBuffer*)tvb, startVertex, numVertices);
    }

    void setInstanceDataBuffer(const gfxInstanceDataBuffer* idb, uint32_t num) override
    {
        bgfx::setInstanceDataBuffer((const bgfx::InstanceDataBuffer*)idb, num);
    }

    void setInstanceDataBuffer(gfxVertexBufferHandle handle, uint32_t startVertex, uint32_t num) override
    {
        BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
        bgfx::setInstanceDataBuffer(h, startVertex, num);
    }

    void setInstanceDataBuffer(gfxDynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t num) override
    {
        BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
        bgfx::setInstanceDataBuffer(h, startVertex, num);
    }

    void setTexture(uint8_t stage, gfxUniformHandle sampler, gfxTextureHandle handle, gfxTextureFlag flags) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);

        bgfx::setTexture(stage, s, h, (uint32_t)flags);
    }

    void setTexture(uint8_t stage, gfxUniformHandle sampler, gfxFrameBufferHandle handle, uint8_t attachment,
                    gfxTextureFlag flags) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
        BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);

        bgfx::setTexture(stage, s, h, attachment, (uint32_t)flags);
    }

    uint32_t submit(uint8_t viewId, gfxProgramHandle program, int32_t depth) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
        bgfx::submit(viewId, p, depth);
    }

    uint32_t submit(uint8_t viewId, gfxProgramHandle program, gfxOccQueryHandle occQuery, int32_t depth) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
        BGFX_DECLARE_HANDLE(OcclusionQueryHandle, o, occQuery);
        bgfx::submit(viewId, p, o, depth);
    }

    uint32_t submit(uint8_t viewId, gfxProgramHandle program, gfxIndirectBufferHandle indirectHandle, uint16_t start,
                    uint16_t num, int32_t depth) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
        BGFX_DECLARE_HANDLE(IndirectBufferHandle, i, indirectHandle);
        bgfx::submit(viewId, p, i, start, num, depth);
    }

    void setBuffer(uint8_t stage, gfxIndexBufferHandle handle, gfxAccess access) override
    {
        BGFX_DECLARE_HANDLE(IndexBufferHandle, h, handle);
        bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
    }

    void setBuffer(uint8_t stage, gfxVertexBufferHandle handle, gfxAccess access) override
    {
        BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
        bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
    }

    void setBuffer(uint8_t stage, gfxDynamicIndexBufferHandle handle, gfxAccess access) override
    {
        BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
        bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
    }

    void setBuffer(uint8_t stage, gfxDynamicVertexBufferHandle handle, gfxAccess access) override
    {
        BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
        bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
    }

    void setBuffer(uint8_t stage, gfxIndirectBufferHandle handle, gfxAccess access) override
    {
        BGFX_DECLARE_HANDLE(IndirectBufferHandle, h, handle);
        bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
    }

    void setImage(uint8_t stage, gfxUniformHandle sampler, gfxTextureHandle handle, uint8_t mip, gfxAccess access,
                  gfxTextureFormat fmt) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::setImage(stage, s, h, mip, (bgfx::Access::Enum)access, (bgfx::TextureFormat::Enum)fmt);
    }

    void setImage(uint8_t stage, gfxUniformHandle sampler, gfxFrameBufferHandle handle, uint8_t attachment,
                  gfxAccess access, gfxTextureFormat fmt) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
        BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
        bgfx::setImage(stage, s, h, attachment, (bgfx::Access::Enum)access, (bgfx::TextureFormat::Enum)fmt);
    }

    uint32_t dispatch(uint8_t viewId, gfxProgramHandle handle, uint16_t numX, uint16_t numY, uint16_t numZ,
                      gfxSubmitFlag flags) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
        bgfx::dispatch(viewId, h, numX, numY, numZ, (uint8_t)flags);
    }

    uint32_t dispatch(uint8_t viewId, gfxProgramHandle handle, gfxIndirectBufferHandle indirectHandle,
                      uint16_t start, uint16_t num, gfxSubmitFlag flags) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
        BGFX_DECLARE_HANDLE(IndirectBufferHandle, i, indirectHandle);
        bgfx::dispatch(viewId, h, i, start, num, (uint8_t)flags);
    }

    void blit(uint8_t viewId, gfxTextureHandle dest, uint16_t destX, uint16_t destY, gfxTextureHandle src,
              uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
        BGFX_DECLARE_HANDLE(TextureHandle, s, src);
        bgfx::blit(viewId, d, destX, destY, s, srcX, srcY, width, height);
    }

    void blit(uint8_t viewId, gfxTextureHandle dest, uint16_t destX, uint16_t destY, gfxFrameBufferHandle src,
              uint8_t attachment, uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
        BGFX_DECLARE_HANDLE(FrameBufferHandle, s, src);
        bgfx::blit(viewId, d, destX, destY, s, attachment, srcX, srcY, width, height);
    }

    void blit(uint8_t viewId, gfxTextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
              uint16_t destZ, gfxTextureHandle src, uint8_t srcMip, uint16_t srcX, uint16_t srcY,
              uint16_t srcZ, uint16_t width, uint16_t height, uint16_t depth) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
        BGFX_DECLARE_HANDLE(TextureHandle, s, src);
        bgfx::blit(viewId, d, destMip, destX, destY, destZ, s, srcMip, srcX, srcY, srcZ, width, height, depth);
    }

    void blit(uint8_t viewId, gfxTextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
              uint16_t destZ, gfxFrameBufferHandle src, uint8_t attachment, uint8_t srcMip, uint16_t srcX,
              uint16_t srcY, uint16_t srcZ, uint16_t width, uint16_t height, uint16_t depth) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
        BGFX_DECLARE_HANDLE(FrameBufferHandle, s, src);
        bgfx::blit(viewId, d, destMip, destX, destY, destZ, s, attachment, srcMip, srcX, srcY, srcZ, width, height, depth);
    }

    const gfxMemory* alloc(uint32_t size) override
    {
        return (const gfxMemory*)bgfx::alloc(size);
    }

    const gfxMemory* copy(const void* data, uint32_t size) override
    {
        return (const gfxMemory*)bgfx::copy(data, size);
    }

    const gfxMemory* makeRef(const void* data, uint32_t size, gfxCallbackReleaseMem releaseFn, void* userData) override
    {
        return (const gfxMemory*)bgfx::makeRef(data, size, releaseFn, userData);
    }

    gfxShaderHandle createShader(const gfxMemory* mem) override
    {
        gfxShaderHandle handle;
        handle.idx = bgfx::createShader((const bgfx::Memory*)mem).idx;
        return handle;
    }

    uint16_t getShaderUniforms(gfxShaderHandle handle, gfxUniformHandle* uniforms, uint16_t _max) override
    {
        BGFX_DECLARE_HANDLE(ShaderHandle, h, handle);
        return bgfx::getShaderUniforms(h, (bgfx::UniformHandle*)uniforms, _max);
    }

    void destroyShader(gfxShaderHandle handle) override
    {
        BGFX_DECLARE_HANDLE(ShaderHandle, h, handle);
        bgfx::destroyShader(h);
    }

    gfxProgramHandle createProgram(gfxShaderHandle vsh, gfxShaderHandle fsh, bool destroyShaders) override
    {
        BGFX_DECLARE_HANDLE(ShaderHandle, v, vsh);
        BGFX_DECLARE_HANDLE(ShaderHandle, f, fsh);
        bgfx::createProgram(v, f, destroyShaders);
    }

    gfxUniformHandle createUniform(const char* name, gfxUniformType type, uint16_t num) override
    {
        gfxUniformHandle handle;
        handle.idx = bgfx::createUniform(name, (bgfx::UniformType::Enum)type, num).idx;
        return handle;
    }

    void setUniform(gfxUniformHandle handle, const void* value, uint16_t num) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, h, handle);
        bgfx::setUniform(h, value, num);
    }

    gfxVertexBufferHandle createVertexBuffer(const gfxMemory* mem, const gfxVertexDecl& decl, gfxBufferFlag flags) override
    {
        gfxVertexBufferHandle handle;
        handle.idx = bgfx::createVertexBuffer((const bgfx::Memory*)mem, (const bgfx::VertexDecl&)decl, (uint16_t)flags).idx;
        return handle;
    }

    gfxDynamicVertexBufferHandle createDynamicVertexBuffer(uint32_t numVertices, const gfxVertexDecl& decl,
                                                           gfxBufferFlag flags) override
    {
        gfxDynamicVertexBufferHandle handle;
        handle.idx = bgfx::createDynamicVertexBuffer(numVertices, (const bgfx::VertexDecl&)decl, (uint16_t)flags).idx;
        return handle;
    }

    gfxDynamicVertexBufferHandle createDynamicVertexBuffer(const gfxMemory* mem, const gfxVertexDecl& decl,
                                                           gfxBufferFlag flags) override
    {
        gfxDynamicVertexBufferHandle handle;
        handle.idx = bgfx::createDynamicVertexBuffer((const bgfx::Memory*)mem, (const bgfx::VertexDecl&)decl, (uint16_t)flags).idx;
        return handle;
    }

    void updateDynamicVertexBuffer(gfxDynamicVertexBufferHandle handle, uint32_t startVertex, const gfxMemory* mem) override
    {
        BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
        bgfx::updateDynamicVertexBuffer(h, startVertex, (const bgfx::Memory*)mem);
    }

    void destroyVertexBuffer(gfxVertexBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
        bgfx::destroyVertexBuffer(h);
    }

    void destroyDynamicVertexBuffer(gfxDynamicVertexBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
        bgfx::destroyDynamicVertexBuffer(h);
    }

    bool checkAvailTransientVertexBuffer(uint32_t num, const gfxVertexDecl& decl) override
    {
        return bgfx::checkAvailTransientVertexBuffer(num, (const bgfx::VertexDecl&)decl);
    }

    void allocTransientVertexBuffer(gfxTransientVertexBuffer* tvb, uint32_t num, const gfxVertexDecl& decl) override
    {
        bgfx::allocTransientVertexBuffer((bgfx::TransientVertexBuffer*)tvb, num, (const bgfx::VertexDecl&)decl);
    }

    gfxIndexBufferHandle createIndexBuffer(const gfxMemory* mem, gfxBufferFlag flags) override
    {
        gfxIndexBufferHandle handle;
        handle.idx = bgfx::createIndexBuffer((const bgfx::Memory*)mem, (uint16_t)flags).idx;
        return handle;
    }

    gfxDynamicIndexBufferHandle createDynamicIndexBuffer(uint32_t num, gfxBufferFlag flags) override
    {
        gfxDynamicIndexBufferHandle handle;
        handle.idx = bgfx::createDynamicIndexBuffer(num, (uint16_t)flags).idx;
        return handle;
    }

    gfxDynamicIndexBufferHandle createDynamicIndexBuffer(const gfxMemory* mem, gfxBufferFlag flags) override
    {
        gfxDynamicIndexBufferHandle handle;
        handle.idx = bgfx::createDynamicIndexBuffer((const bgfx::Memory*)mem, (uint16_t)flags).idx;
        return handle;
    }

    void updateDynamicIndexBuffer(gfxDynamicIndexBufferHandle handle, uint32_t startIndex, const gfxMemory* mem) override
    {
        BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
        bgfx::updateDynamicIndexBuffer(h, startIndex, (const bgfx::Memory*)mem);
    }

    void destroyIndexBuffer(gfxIndexBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(IndexBufferHandle, h, handle);
        bgfx::destroyIndexBuffer(h);
    }

    void destroyDynamicIndexBuffer(gfxDynamicIndexBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
        bgfx::destroyDynamicIndexBuffer(h);
    }

    bool checkAvailTransientIndexBuffer(uint32_t num) override
    {
        return bgfx::checkAvailTransientIndexBuffer(num);
    }

    void allocTransientIndexBuffer(gfxTransientIndexBuffer* tib, uint32_t num) override
    {
        bgfx::allocTransientIndexBuffer((bgfx::TransientIndexBuffer*)tib, num);
    }

    void calcTextureSize(gfxTextureInfo* info, uint16_t width, uint16_t height, uint16_t depth, bool cubemap,
                         uint8_t numMips, gfxTextureFormat fmt) override
    {
        bgfx::calcTextureSize((bgfx::TextureInfo&)(*info), width, height, depth, cubemap, numMips, 
                              (bgfx::TextureFormat::Enum)fmt);
    }

    gfxTextureHandle createTexture(const gfxMemory* mem, gfxTextureFlag flags, uint8_t skipMips, gfxTextureInfo* info) override
    {
        gfxTextureHandle r;
        r.idx = bgfx::createTexture((const bgfx::Memory*)mem, (uint32_t)flags, skipMips, (bgfx::TextureInfo*)info).idx;
        return r;
    }

    gfxTextureHandle createTexture2D(uint16_t width, uint16_t height, uint8_t numMips, gfxTextureFormat fmt,
                                     gfxTextureFlag flags, const gfxMemory* mem) override
    {
        gfxTextureHandle r;
        r.idx = bgfx::createTexture2D(width, height, numMips, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags,
                                      (const bgfx::Memory*)mem).idx;
        return r;
    }

    gfxTextureHandle createTexture2D(gfxBackbufferRatio ratio, uint8_t numMips, gfxTextureFormat fmt,
                                     gfxTextureFlag flags) override
    {
        gfxTextureHandle r;
        r.idx = bgfx::createTexture2D((bgfx::BackbufferRatio::Enum)ratio, numMips, (bgfx::TextureFormat::Enum)fmt,
                                      (uint32_t)flags).idx;
        return r;
    }

    void updateTexture2D(gfxTextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t width,
                         uint16_t height, const gfxMemory* mem, uint16_t pitch) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::updateTexture2D(h, mip, x, y, width, height, (const bgfx::Memory*)mem, pitch);
    }

    gfxTextureHandle createTexture3D(uint16_t width, uint16_t height, uint16_t depth, uint8_t numMips,
                                     gfxTextureFormat fmt, gfxTextureFlag flags, const gfxMemory* mem) override
    {
        gfxTextureHandle r;
        r.idx = bgfx::createTexture3D(width, height, depth, numMips, (bgfx::TextureFormat::Enum)fmt,
                                      (uint32_t)flags, (const bgfx::Memory*)mem).idx;
        return r;
    }

    void updateTexture3D(gfxTextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t z,
                         uint16_t width, uint16_t height, uint16_t depth, const gfxMemory* mem) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::updateTexture3D(h, mip, x, y, z, width, height, depth, (const bgfx::Memory*)mem);
    }

    gfxTextureHandle createTextureCube(uint16_t size, uint8_t numMips, gfxTextureFormat fmt, gfxTextureFlag flags,
                                       const gfxMemory* mem) override
    {
        gfxTextureHandle r;
        r.idx = bgfx::createTextureCube(size, numMips, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags, 
                                        (const bgfx::Memory*)mem).idx;
        return r;
    }

    void updateTextureCube(gfxTextureHandle handle, gfxCubeSide side, uint8_t mip, uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height, const gfxMemory* mem, uint16_t pitch) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::updateTextureCube(h, (uint8_t)side, mip, x, y, width, height, (const bgfx::Memory*)mem, pitch);
    }

    void readTexture(gfxTextureHandle handle, void* data) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::readTexture(h, data);
    }

    void readTexture(gfxFrameBufferHandle handle, uint8_t attachment, void* data) override
    {
        BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
        bgfx::readTexture(h, attachment, data);
    }

    void destroyTexture(gfxTextureHandle handle) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::destroyTexture(h);
    }

    gfxFrameBufferHandle createFrameBuffer(uint16_t width, uint16_t height, gfxTextureFormat fmt, gfxTextureFlag flags) override
    {
        gfxFrameBufferHandle r;
        r.idx = bgfx::createFrameBuffer(width, height, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags).idx;
        return r;
    }

    gfxFrameBufferHandle createFrameBuffer(gfxBackbufferRatio ratio, gfxTextureFormat fmt, gfxTextureFlag flags) override
    {
        gfxFrameBufferHandle r;
        r.idx = bgfx::createFrameBuffer((bgfx::BackbufferRatio::Enum)ratio, (bgfx::TextureFormat::Enum)fmt, 
                                        (uint32_t)flags).idx;
        return r;
    }

    gfxFrameBufferHandle createFrameBuffer(uint8_t num, const gfxTextureHandle* handles, bool destroyTextures) override
    {
        gfxFrameBufferHandle r;
        r.idx = bgfx::createFrameBuffer(num, (const bgfx::TextureHandle*)handles, destroyTextures).idx;
        return r;
    }

    gfxFrameBufferHandle createFrameBuffer(void* nwh, uint16_t width, uint16_t height, gfxTextureFormat depthFmt) override
    {
        gfxFrameBufferHandle r;
        r.idx = bgfx::createFrameBuffer(nwh, width, height, (bgfx::TextureFormat::Enum)depthFmt).idx;
        return r;
    }

    void destroyFrameBuffer(gfxFrameBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
        bgfx::destroyFrameBuffer(h);
    }

    bool checkAvailInstanceDataBuffer(uint32_t num, uint16_t stride) override
    {
        return bgfx::checkAvailInstanceDataBuffer(num, stride);
    }

    const gfxInstanceDataBuffer* allocInstanceDataBuffer(uint32_t num, uint16_t stride) override
    {
        return (const gfxInstanceDataBuffer*)bgfx::allocInstanceDataBuffer(num, stride);
    }

    gfxIndirectBufferHandle createIndirectBuffer(uint32_t num) override
    {
        gfxIndirectBufferHandle r;
        r.idx = bgfx::createIndirectBuffer(num).idx;
        return r;
    }

    void destroyIndirectBuffer(gfxIndirectBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(IndirectBufferHandle, h, handle);
        bgfx::destroyIndirectBuffer(h);
    }

    gfxOccQueryHandle createOccQuery() override
    {
        gfxOccQueryHandle r;
        r.idx = bgfx::createOcclusionQuery().idx;
        return r;
    }

    gfxOccQueryResult getResult(gfxOccQueryHandle handle) override
    {
        BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
        return (gfxOccQueryResult)bgfx::getResult(h);
    }

    void destroyOccQuery(gfxOccQueryHandle handle) override
    {
        BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
        bgfx::destroyOcclusionQuery(h);
    }
};

#ifdef STENGINE_SHARED_LIB
#define MY_NAME "Bgfx"
static bx::AllocatorI* gAlloc = nullptr;
static st::srvHandle gMyHandle = ST_INVALID_HANDLE;

STPLUGIN_API pluginDesc* stPluginGetDesc()
{
    static pluginDesc desc;
    desc.name = MY_NAME;
    desc.description = "Bgfx wrapper driver";
    desc.engineVersion = ST_MAKE_VERSION(0, 1);
    desc.type = srvDriverType::Graphics;
    desc.version = ST_MAKE_VERSION(1, 0);
    return &desc;
}

STPLUGIN_API st::pluginHandle stPluginInit(bx::AllocatorI* alloc)
{
    assert(alloc);

    gAlloc = alloc;
    BgfxWrapper* driver = BX_NEW(alloc, BgfxWrapper)();
    if (driver) {
        gMyHandle = srvRegisterGraphicsDriver(driver, MY_NAME);
        if (gMyHandle == ST_INVALID_HANDLE) {
            BX_DELETE(alloc, driver);
            gAlloc = nullptr;
            return nullptr;
        }
    }

    return driver;
}

STPLUGIN_API void stPluginShutdown(pluginHandle handle)
{
    assert(handle);
    assert(gAlloc);

    if (gMyHandle != ST_INVALID_HANDLE)
        srvUnregisterGraphicsDriver(gMyHandle);
    BX_DELETE(gAlloc, handle);
    gAlloc = nullptr;
    gMyHandle = ST_INVALID_HANDLE;
}
#endif