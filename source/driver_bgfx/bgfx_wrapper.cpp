#include "termite/core.h"

#define T_CORE_API
#include "termite/plugin_api.h"

#include "termite/gfx_driver.h"

#include "bgfx/bgfx.h"
#include "bgfx/bgfxplatform.h"

#include <cstdarg>
#include <cstdio>

using namespace termite;

#define BGFX_DECLARE_HANDLE(_Type, _Name, _Handle) bgfx::_Type _Name; _Name.idx = _Handle.value

class BgfxCallbacks : public bgfx::CallbackI
{
private:
    GfxDriverEventsI* m_callbacks;

public:
    BgfxCallbacks(GfxDriverEventsI* callbacks)
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
        m_callbacks->onFatal((GfxFatalType)_code, _str);
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
        return m_callbacks->onCacheReadSize(_id);
    }

    /// Read cached item.
    ///
    /// @param[in] _id Cache id.
    /// @param[in] _data Buffer where to read data.
    /// @param[in] _size Size of data to read.
    bool cacheRead(uint64_t _id, void* _data, uint32_t _size) override
    {
        return m_callbacks->onCacheRead(_id, _data, _size);
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
        m_callbacks->onCaptureBegin(_width, _height, _pitch, (TextureFormat)_format, _yflip);
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

class BgfxWrapper : public GfxDriverI
{
private:
    BgfxCallbacks* m_callbacks;
    bx::AllocatorI* m_alloc;
    GfxCaps m_caps;
    GfxStats m_stats;
    HMDDesc m_hmd;
    GfxInternalData m_internal;

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

    result_t init(uint16_t deviceId, GfxDriverEventsI* callbacks, bx::AllocatorI* alloc) override
    {
        m_alloc = alloc;
        if (callbacks) {
            m_callbacks = BX_NEW(alloc, BgfxCallbacks)(callbacks);
            if (!m_callbacks)
                return T_ERR_OUTOFMEM;
        }

        return bgfx::init(bgfx::RendererType::Count, 0, deviceId, m_callbacks, alloc) ? 0 : T_ERR_FAILED;
    }

    void shutdown() override
    {
        bgfx::shutdown();
        if (m_callbacks) {
            BX_DELETE(m_alloc, m_callbacks);
        }
    }

    void reset(uint32_t width, uint32_t height, GfxResetFlag flags) override
    {
        bgfx::reset(width, height, (uint32_t)flags);
    }

    uint32_t frame() override
    {
        return bgfx::frame();
    }

    void setDebug(GfxDebugFlag debugFlags) override
    {
        bgfx::setDebug((uint32_t)debugFlags);
    }

    RendererType getRendererType() const override
    {
        return (RendererType)bgfx::getRendererType();
    }

    const GfxCaps& getCaps() override
    {
        const bgfx::Caps* caps = bgfx::getCaps();
        m_caps.deviceId = caps->deviceId;
        m_caps.supported = (GpuCapsFlag)caps->supported;
        m_caps.maxDrawCalls = caps->maxDrawCalls;
        m_caps.maxFBAttachments = caps->maxFBAttachments;
        m_caps.maxTextureSize = caps->maxTextureSize;
        m_caps.maxViews = caps->maxViews;
        m_caps.numGPUs = caps->numGPUs;
        m_caps.type = (RendererType)caps->rendererType;
        m_caps.vendorId = caps->vendorId;
        
        for (int i = 0; i < 4; i++) {
            m_caps.gpu[i].deviceId = caps->gpu[i].deviceId;
            m_caps.gpu[i].vendorId = caps->gpu[i].vendorId;
        }

        return m_caps;
    }

    const GfxStats& getStats() override
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

    const HMDDesc& getHMD() override
    {
        const bgfx::HMD* hmd = bgfx::getHMD();
        m_hmd.deviceWidth = hmd->deviceWidth;
        m_hmd.deviceHeight = hmd->deviceHeight;
        m_hmd.width = hmd->width;
        m_hmd.height = hmd->height;
        m_hmd.flags = hmd->flags;

        for (int i = 0; i < 2; i++) {
            memcpy(m_hmd.eye[i].rotation, hmd->eye[i].rotation, sizeof(float) * 4);
            memcpy(m_hmd.eye[i].translation, hmd->eye[i].translation, sizeof(float) * 3);
            memcpy(m_hmd.eye[i].fov, hmd->eye[i].fov, sizeof(float) * 4);
            memcpy(m_hmd.eye[i].viewOffset, hmd->eye[i].viewOffset, sizeof(float) * 3);
        }
        return m_hmd;
    }

    RenderFrameType renderFrame() override
    {
        return (RenderFrameType)bgfx::renderFrame();
    }

    void setPlatformData(const GfxPlatformData& data) override
    {
        bgfx::PlatformData p;
        p.backBuffer = data.backBuffer;
        p.backBufferDS = data.backBufferDS;
        p.context = data.context;
        p.ndt = data.ndt;
        p.nwh = data.nwh;
        bgfx::setPlatformData(p);
    }

    const GfxInternalData& getInternalData() override
    {
        const bgfx::InternalData* d = bgfx::getInternalData();
        m_internal.caps = &getCaps();
        m_internal.context = d->context;
        return m_internal;
    }

    void overrideInternal(TextureHandle handle, uintptr_t ptr) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::overrideInternal(h, ptr);
    }

    void overrideInternal(TextureHandle handle, uint16_t width, uint16_t height, uint8_t numMips,
                          TextureFormat fmt, TextureFlag flags) override
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
        return bgfx::touch(id);
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

    void setViewRect(uint8_t id, uint16_t x, uint16_t y, BackbufferRatio ratio) override
    {
        bgfx::setViewRect(id, x, y, (bgfx::BackbufferRatio::Enum)ratio);
    }

    void setViewScissor(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height) override
    {
        bgfx::setViewScissor(id, x, y, width, height);
    }

    void setViewClear(uint8_t id, GfxClearFlag flags, uint32_t rgba, float depth, uint8_t stencil) override
    {
        bgfx::setViewClear(id, (uint16_t)flags, rgba, depth, stencil);
    }
        
    void setViewClear(uint8_t id, GfxClearFlag flags, float depth, uint8_t stencil,
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

    void setViewTransform(uint8_t id, const void* view, const void* projLeft, GfxViewFlag flags, 
                          const void* projRight) override
    {
        bgfx::setViewTransform(id, view, projLeft, (uint8_t)flags, projRight);
    }

    void setViewRemap(uint8_t id, uint8_t num, const void* remap) override
    {
        bgfx::setViewRemap(id, num, remap);
    }

    void setViewFrameBuffer(uint8_t id, FrameBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
        bgfx::setViewFrameBuffer(id, h);
    }

    void setMarker(const char* marker) override
    {
        bgfx::setMarker(marker);
    }

    void setState(GfxState state, uint32_t rgba) override
    {
        bgfx::setState((uint64_t)state, rgba);
    }

    void setStencil(GfxStencilState frontStencil, GfxStencilState backStencil) override
    {
        bgfx::setStencil((uint32_t)frontStencil, (uint32_t)backStencil);
    }

    uint16_t setScissor(uint16_t x, uint16_t y, uint16_t width, uint16_t height) override
    {
        return bgfx::setScissor(x, y, width, height);
    }

    void setScissor(uint16_t cache) override
    {
        bgfx::setScissor(cache);
    }

    uint32_t allocTransform(GpuTransform* transform, uint16_t num) override
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

    void setCondition(OcclusionQueryHandle handle, bool visible) override
    {
        BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
        bgfx::setCondition(h, visible);
    }

    void setIndexBuffer(IndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices) override
    {
        BGFX_DECLARE_HANDLE(IndexBufferHandle, h, handle);
        bgfx::setIndexBuffer(h, firstIndex, numIndices);
    }

    void setIndexBuffer(DynamicIndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices) override
    {
        BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
        bgfx::setIndexBuffer(h, firstIndex, numIndices);
    }

    void setIndexBuffer(const TransientIndexBuffer* tib, uint32_t firstIndex, uint32_t numIndices) override
    {
        bgfx::setIndexBuffer((const bgfx::TransientIndexBuffer*)tib, firstIndex, numIndices);
    }

    void setIndexBuffer(const TransientIndexBuffer* tib) override
    {
        bgfx::setIndexBuffer((const bgfx::TransientIndexBuffer*)tib);
    }

    void setVertexBuffer(VertexBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
        bgfx::setVertexBuffer(h);
    }

    void setVertexBuffer(VertexBufferHandle handle, uint32_t vertexIndex, uint32_t numVertices) override
    {
        BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
        bgfx::setVertexBuffer(h, vertexIndex, numVertices);
    }

    void setVertexBuffer(DynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t numVertices) override
    {
        BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
        bgfx::setVertexBuffer(h, startVertex, numVertices);
    }

    void setVertexBuffer(const TransientVertexBuffer* tvb) override
    {
        bgfx::setVertexBuffer((const bgfx::TransientVertexBuffer*)tvb);
    }

    void setVertexBuffer(const TransientVertexBuffer* tvb, uint32_t startVertex, uint32_t numVertices) override
    {
        bgfx::setVertexBuffer((const bgfx::TransientVertexBuffer*)tvb, startVertex, numVertices);
    }

    void setInstanceDataBuffer(const InstanceDataBuffer* idb, uint32_t num) override
    {
        bgfx::setInstanceDataBuffer((const bgfx::InstanceDataBuffer*)idb, num);
    }

    void setInstanceDataBuffer(VertexBufferHandle handle, uint32_t startVertex, uint32_t num) override
    {
        BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
        bgfx::setInstanceDataBuffer(h, startVertex, num);
    }

    void setInstanceDataBuffer(DynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t num) override
    {
        BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
        bgfx::setInstanceDataBuffer(h, startVertex, num);
    }

    void setTexture(uint8_t stage, UniformHandle sampler, TextureHandle handle, TextureFlag flags) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);

        bgfx::setTexture(stage, s, h, (uint32_t)flags);
    }

    void setTexture(uint8_t stage, UniformHandle sampler, FrameBufferHandle handle, uint8_t attachment,
                    TextureFlag flags) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
        BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);

        bgfx::setTexture(stage, s, h, attachment, (uint32_t)flags);
    }

    uint32_t submit(uint8_t viewId, ProgramHandle program, int32_t depth, bool preserveState) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
        return bgfx::submit(viewId, p, depth, preserveState);
    }

    uint32_t submit(uint8_t viewId, ProgramHandle program, OcclusionQueryHandle occQuery, int32_t depth, bool preserveState) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
        BGFX_DECLARE_HANDLE(OcclusionQueryHandle, o, occQuery);
        return bgfx::submit(viewId, p, o, depth, preserveState);
    }

    uint32_t submit(uint8_t viewId, ProgramHandle program, IndirectBufferHandle indirectHandle, uint16_t start,
                    uint16_t num, int32_t depth, bool preserveState) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
        BGFX_DECLARE_HANDLE(IndirectBufferHandle, i, indirectHandle);
        return bgfx::submit(viewId, p, i, start, num, depth, preserveState);
    }

    void setBuffer(uint8_t stage, IndexBufferHandle handle, GpuAccessFlag access) override
    {
        BGFX_DECLARE_HANDLE(IndexBufferHandle, h, handle);
        bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
    }

    void setBuffer(uint8_t stage, VertexBufferHandle handle, GpuAccessFlag access) override
    {
        BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
        bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
    }

    void setBuffer(uint8_t stage, DynamicIndexBufferHandle handle, GpuAccessFlag access) override
    {
        BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
        bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
    }

    void setBuffer(uint8_t stage, DynamicVertexBufferHandle handle, GpuAccessFlag access) override
    {
        BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
        bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
    }

    void setBuffer(uint8_t stage, IndirectBufferHandle handle, GpuAccessFlag access) override
    {
        BGFX_DECLARE_HANDLE(IndirectBufferHandle, h, handle);
        bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
    }

    void setImage(uint8_t stage, UniformHandle sampler, TextureHandle handle, uint8_t mip, GpuAccessFlag access,
                  TextureFormat fmt) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::setImage(stage, s, h, mip, (bgfx::Access::Enum)access, (bgfx::TextureFormat::Enum)fmt);
    }

    void setImage(uint8_t stage, UniformHandle sampler, FrameBufferHandle handle, uint8_t attachment,
                  GpuAccessFlag access, TextureFormat fmt) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
        BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
        bgfx::setImage(stage, s, h, attachment, (bgfx::Access::Enum)access, (bgfx::TextureFormat::Enum)fmt);
    }

    uint32_t dispatch(uint8_t viewId, ProgramHandle handle, uint16_t numX, uint16_t numY, uint16_t numZ,
                      GfxSubmitFlag flags) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
        return bgfx::dispatch(viewId, h, numX, numY, numZ, (uint8_t)flags);
    }

    uint32_t dispatch(uint8_t viewId, ProgramHandle handle, IndirectBufferHandle indirectHandle,
                      uint16_t start, uint16_t num, GfxSubmitFlag flags) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
        BGFX_DECLARE_HANDLE(IndirectBufferHandle, i, indirectHandle);
        return bgfx::dispatch(viewId, h, i, start, num, (uint8_t)flags);
    }

    void blit(uint8_t viewId, TextureHandle dest, uint16_t destX, uint16_t destY, TextureHandle src,
              uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
        BGFX_DECLARE_HANDLE(TextureHandle, s, src);
        bgfx::blit(viewId, d, destX, destY, s, srcX, srcY, width, height);
    }

    void blit(uint8_t viewId, TextureHandle dest, uint16_t destX, uint16_t destY, FrameBufferHandle src,
              uint8_t attachment, uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
        BGFX_DECLARE_HANDLE(FrameBufferHandle, s, src);
        bgfx::blit(viewId, d, destX, destY, s, attachment, srcX, srcY, width, height);
    }

    void blit(uint8_t viewId, TextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
              uint16_t destZ, TextureHandle src, uint8_t srcMip, uint16_t srcX, uint16_t srcY,
              uint16_t srcZ, uint16_t width, uint16_t height, uint16_t depth) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
        BGFX_DECLARE_HANDLE(TextureHandle, s, src);
        bgfx::blit(viewId, d, destMip, destX, destY, destZ, s, srcMip, srcX, srcY, srcZ, width, height, depth);
    }

    void blit(uint8_t viewId, TextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
              uint16_t destZ, FrameBufferHandle src, uint8_t attachment, uint8_t srcMip, uint16_t srcX,
              uint16_t srcY, uint16_t srcZ, uint16_t width, uint16_t height, uint16_t depth) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
        BGFX_DECLARE_HANDLE(FrameBufferHandle, s, src);
        bgfx::blit(viewId, d, destMip, destX, destY, destZ, s, attachment, srcMip, srcX, srcY, srcZ, width, height, depth);
    }

    const GfxMemory* alloc(uint32_t size) override
    {
        return (const GfxMemory*)bgfx::alloc(size);
    }

    const GfxMemory* copy(const void* data, uint32_t size) override
    {
        return (const GfxMemory*)bgfx::copy(data, size);
    }

    const GfxMemory* makeRef(const void* data, uint32_t size, gfxReleaseMemCallback releaseFn, void* userData) override
    {
        return (const GfxMemory*)bgfx::makeRef(data, size, releaseFn, userData);
    }

    ShaderHandle createShader(const GfxMemory* mem) override
    {
        ShaderHandle handle;
        handle.value = bgfx::createShader((const bgfx::Memory*)mem).idx;
        return handle;
    }

    uint16_t getShaderUniforms(ShaderHandle handle, UniformHandle* uniforms, uint16_t _max) override
    {
        BGFX_DECLARE_HANDLE(ShaderHandle, h, handle);
        return bgfx::getShaderUniforms(h, (bgfx::UniformHandle*)uniforms, _max);
    }

    void destroyShader(ShaderHandle handle) override
    {
        BGFX_DECLARE_HANDLE(ShaderHandle, h, handle);
        bgfx::destroyShader(h);
    }

    void destroyUniform(UniformHandle handle) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, h, handle);
        bgfx::destroyUniform(h);
    }

    ProgramHandle createProgram(ShaderHandle vsh, ShaderHandle fsh, bool destroyShaders) override
    {
        BGFX_DECLARE_HANDLE(ShaderHandle, v, vsh);
        BGFX_DECLARE_HANDLE(ShaderHandle, f, fsh);
        ProgramHandle h;
        h.value = bgfx::createProgram(v, f, destroyShaders).idx;
        return h;
    }

    void destroyProgram(ProgramHandle handle) override
    {
        BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
        bgfx::destroyProgram(h);
    }

    UniformHandle createUniform(const char* name, UniformType type, uint16_t num) override
    {
        UniformHandle handle;
        handle.value = bgfx::createUniform(name, (bgfx::UniformType::Enum)type, num).idx;
        return handle;
    }

    void setUniform(UniformHandle handle, const void* value, uint16_t num) override
    {
        BGFX_DECLARE_HANDLE(UniformHandle, h, handle);
        bgfx::setUniform(h, value, num);
    }

    VertexBufferHandle createVertexBuffer(const GfxMemory* mem, const VertexDecl& decl, GpuBufferFlag flags) override
    {
        VertexBufferHandle handle;
        handle.value = bgfx::createVertexBuffer((const bgfx::Memory*)mem, (const bgfx::VertexDecl&)decl, (uint16_t)flags).idx;
        return handle;
    }

    DynamicVertexBufferHandle createDynamicVertexBuffer(uint32_t numVertices, const VertexDecl& decl, GpuBufferFlag flags) override
    {
        DynamicVertexBufferHandle handle;
        handle.value = bgfx::createDynamicVertexBuffer(numVertices, (const bgfx::VertexDecl&)decl, (uint16_t)flags).idx;
        return handle;
    }

    DynamicVertexBufferHandle createDynamicVertexBuffer(const GfxMemory* mem, const VertexDecl& decl,
                                                           GpuBufferFlag flags = GpuBufferFlag::None) override
    {
        DynamicVertexBufferHandle handle;
        handle.value = bgfx::createDynamicVertexBuffer((const bgfx::Memory*)mem, (const bgfx::VertexDecl&)decl, (uint16_t)flags).idx;
        return handle;
    }

    void updateDynamicVertexBuffer(DynamicVertexBufferHandle handle, uint32_t startVertex, const GfxMemory* mem) override
    {
        BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
        bgfx::updateDynamicVertexBuffer(h, startVertex, (const bgfx::Memory*)mem);
    }

    void destroyVertexBuffer(VertexBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
        bgfx::destroyVertexBuffer(h);
    }

    void destroyDynamicVertexBuffer(DynamicVertexBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
        bgfx::destroyDynamicVertexBuffer(h);
    }

    bool checkAvailTransientVertexBuffer(uint32_t num, const VertexDecl& decl) override
    {
        return bgfx::checkAvailTransientVertexBuffer(num, (const bgfx::VertexDecl&)decl);
    }

    void allocTransientVertexBuffer(TransientVertexBuffer* tvb, uint32_t num, const VertexDecl& decl) override
    {
        bgfx::allocTransientVertexBuffer((bgfx::TransientVertexBuffer*)tvb, num, (const bgfx::VertexDecl&)decl);
    }

    IndexBufferHandle createIndexBuffer(const GfxMemory* mem, GpuBufferFlag flags) override
    {
        IndexBufferHandle handle;
        handle.value = bgfx::createIndexBuffer((const bgfx::Memory*)mem, (uint16_t)flags).idx;
        return handle;
    }

    DynamicIndexBufferHandle createDynamicIndexBuffer(uint32_t num, GpuBufferFlag flags) override
    {
        DynamicIndexBufferHandle handle;
        handle.value = bgfx::createDynamicIndexBuffer(num, (uint16_t)flags).idx;
        return handle;
    }

    DynamicIndexBufferHandle createDynamicIndexBuffer(const GfxMemory* mem, GpuBufferFlag flags) override
    {
        DynamicIndexBufferHandle handle;
        handle.value = bgfx::createDynamicIndexBuffer((const bgfx::Memory*)mem, (uint16_t)flags).idx;
        return handle;
    }

    void updateDynamicIndexBuffer(DynamicIndexBufferHandle handle, uint32_t startIndex, const GfxMemory* mem) override
    {
        BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
        bgfx::updateDynamicIndexBuffer(h, startIndex, (const bgfx::Memory*)mem);
    }

    void destroyIndexBuffer(IndexBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(IndexBufferHandle, h, handle);
        bgfx::destroyIndexBuffer(h);
    }

    void destroyDynamicIndexBuffer(DynamicIndexBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
        bgfx::destroyDynamicIndexBuffer(h);
    }

    bool checkAvailTransientIndexBuffer(uint32_t num) override
    {
        return bgfx::checkAvailTransientIndexBuffer(num);
    }

    void allocTransientIndexBuffer(TransientIndexBuffer* tib, uint32_t num) override
    {
        bgfx::allocTransientIndexBuffer((bgfx::TransientIndexBuffer*)tib, num);
    }

    void calcTextureSize(TextureInfo* info, uint16_t width, uint16_t height, uint16_t depth, bool cubemap,
                         uint8_t numMips, TextureFormat fmt) override
    {
        bgfx::calcTextureSize((bgfx::TextureInfo&)(*info), width, height, depth, cubemap, numMips, 
                              (bgfx::TextureFormat::Enum)fmt);
    }

    TextureHandle createTexture(const GfxMemory* mem, TextureFlag flags, uint8_t skipMips, TextureInfo* info) override
    {
        TextureHandle r;
        r.value = bgfx::createTexture((const bgfx::Memory*)mem, (uint32_t)flags, skipMips, (bgfx::TextureInfo*)info).idx;
        return r;
    }

    TextureHandle createTexture2D(uint16_t width, uint16_t height, uint8_t numMips, TextureFormat fmt,
                                     TextureFlag flags, const GfxMemory* mem) override
    {
        TextureHandle r;
        r.value = bgfx::createTexture2D(width, height, numMips, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags,
                                      (const bgfx::Memory*)mem).idx;
        return r;
    }

    TextureHandle createTexture2D(BackbufferRatio ratio, uint8_t numMips, TextureFormat fmt,
                                     TextureFlag flags) override
    {
        TextureHandle r;
        r.value = bgfx::createTexture2D((bgfx::BackbufferRatio::Enum)ratio, numMips, (bgfx::TextureFormat::Enum)fmt,
                                      (uint32_t)flags).idx;
        return r;
    }

    void updateTexture2D(TextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t width,
                         uint16_t height, const GfxMemory* mem, uint16_t pitch) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::updateTexture2D(h, mip, x, y, width, height, (const bgfx::Memory*)mem, pitch);
    }

    TextureHandle createTexture3D(uint16_t width, uint16_t height, uint16_t depth, uint8_t numMips,
                                     TextureFormat fmt, TextureFlag flags, const GfxMemory* mem) override
    {
        TextureHandle r;
        r.value = bgfx::createTexture3D(width, height, depth, numMips, (bgfx::TextureFormat::Enum)fmt,
                                      (uint32_t)flags, (const bgfx::Memory*)mem).idx;
        return r;
    }

    void updateTexture3D(TextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t z,
                         uint16_t width, uint16_t height, uint16_t depth, const GfxMemory* mem) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::updateTexture3D(h, mip, x, y, z, width, height, depth, (const bgfx::Memory*)mem);
    }

    TextureHandle createTextureCube(uint16_t size, uint8_t numMips, TextureFormat fmt, TextureFlag flags,
                                       const GfxMemory* mem) override
    {
        TextureHandle r;
        r.value = bgfx::createTextureCube(size, numMips, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags, 
                                        (const bgfx::Memory*)mem).idx;
        return r;
    }

    void updateTextureCube(TextureHandle handle, CubeSide side, uint8_t mip, uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height, const GfxMemory* mem, uint16_t pitch) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::updateTextureCube(h, (uint8_t)side, mip, x, y, width, height, (const bgfx::Memory*)mem, pitch);
    }

    void readTexture(TextureHandle handle, void* data) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::readTexture(h, data);
    }

    void readTexture(FrameBufferHandle handle, uint8_t attachment, void* data) override
    {
        BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
        bgfx::readTexture(h, attachment, data);
    }

    void destroyTexture(TextureHandle handle) override
    {
        BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
        bgfx::destroyTexture(h);
    }

    FrameBufferHandle createFrameBuffer(uint16_t width, uint16_t height, TextureFormat fmt, TextureFlag flags) override
    {
        FrameBufferHandle r;
        r.value = bgfx::createFrameBuffer(width, height, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags).idx;
        return r;
    }

    FrameBufferHandle createFrameBuffer(BackbufferRatio ratio, TextureFormat fmt, TextureFlag flags) override
    {
        FrameBufferHandle r;
        r.value = bgfx::createFrameBuffer((bgfx::BackbufferRatio::Enum)ratio, (bgfx::TextureFormat::Enum)fmt, 
                                        (uint32_t)flags).idx;
        return r;
    }

    FrameBufferHandle createFrameBuffer(uint8_t num, const TextureHandle* handles, bool destroyTextures) override
    {
        FrameBufferHandle r;
        r.value = bgfx::createFrameBuffer(num, (const bgfx::TextureHandle*)handles, destroyTextures).idx;
        return r;
    }

    FrameBufferHandle createFrameBuffer(void* nwh, uint16_t width, uint16_t height, TextureFormat depthFmt) override
    {
        FrameBufferHandle r;
        r.value = bgfx::createFrameBuffer(nwh, width, height, (bgfx::TextureFormat::Enum)depthFmt).idx;
        return r;
    }

    void destroyFrameBuffer(FrameBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
        bgfx::destroyFrameBuffer(h);
    }

    bool checkAvailInstanceDataBuffer(uint32_t num, uint16_t stride) override
    {
        return bgfx::checkAvailInstanceDataBuffer(num, stride);
    }

    const InstanceDataBuffer* allocInstanceDataBuffer(uint32_t num, uint16_t stride) override
    {
        return (const InstanceDataBuffer*)bgfx::allocInstanceDataBuffer(num, stride);
    }

    IndirectBufferHandle createIndirectBuffer(uint32_t num) override
    {
        IndirectBufferHandle r;
        r.value = bgfx::createIndirectBuffer(num).idx;
        return r;
    }

    void destroyIndirectBuffer(IndirectBufferHandle handle) override
    {
        BGFX_DECLARE_HANDLE(IndirectBufferHandle, h, handle);
        bgfx::destroyIndirectBuffer(h);
    }

    OcclusionQueryHandle createOccQuery() override
    {
        OcclusionQueryHandle r;
        r.value = bgfx::createOcclusionQuery().idx;
        return r;
    }

    OcclusionQueryResult getResult(OcclusionQueryHandle handle) override
    {
        BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
        return (OcclusionQueryResult)bgfx::getResult(h);
    }

    void destroyOccQuery(OcclusionQueryHandle handle) override
    {
        BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
        bgfx::destroyOcclusionQuery(h);
    }

    void dbgTextClear(uint8_t attr, bool small) override
    {
        bgfx::dbgTextClear(attr, small);
    }

    void dbgTextPrintf(uint16_t x, uint16_t y, uint8_t attr, const char* format, ...) override
    {
        char text[256];

        va_list args;
        va_start(args, format);
        vsnprintf(text, sizeof(text), format, args);
        va_end(args);

        bgfx::dbgTextPrintf(x, y, attr, text);
    }

    void dbgTextImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const void* data, uint16_t pitch) override
    {
        bgfx::dbgTextImage(x, y, width, height, data, pitch);
    }
};

#ifdef termite_SHARED_LIB
static BgfxWrapper g_bgfx;

static PluginDesc* getBgfxDriverDesc()
{
    static PluginDesc desc;
    strcpy(desc.name, "Bgfx");
    strcpy(desc.description, "Bgfx Driver");
    desc.type = PluginType::GraphicsDriver;
    desc.version = T_MAKE_VERSION(0, 9);
    return &desc;
}

static void* initBgfxDriver(bx::AllocatorI* alloc, GetApiFunc getApi)
{
    return &g_bgfx;
}

static void shutdownBgfxDriver()
{
}

T_PLUGIN_EXPORT void* termiteGetPluginApi(uint16_t apiId, uint32_t version)
{
    static PluginApi_v0 v0;

    if (version == 0) {
        v0.init = initBgfxDriver;
        v0.shutdown = shutdownBgfxDriver;
        v0.getDesc = getBgfxDriverDesc;
        return &v0;
    } else {
        return nullptr;
    }
}


#endif
