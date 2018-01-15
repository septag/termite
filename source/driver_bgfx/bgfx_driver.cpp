#include "termite/tee.h"

#define TEE_CORE_API
#include "termite/plugin_api.h"
#include "termite/gfx_driver.h"

#include "bxx/pool.h"
#include "bx/mutex.h"
#include "bx/uint32_t.h"

#include "bgfx/bgfx.h"
#include "bgfx/platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

//
using namespace tee;

static CoreApi* gTee = nullptr;

#define BGFX_DECLARE_HANDLE(_Type, _Name, _Handle) bgfx::_Type _Name; _Name.idx = _Handle.value

class BgfxCallbacks : public bgfx::CallbackI
{
private:
    GfxDriverEventsI* callbacks;

public:
    BgfxCallbacks(GfxDriverEventsI* _callbacks)
    {
        assert(_callbacks);
        callbacks = _callbacks;
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
        callbacks->onFatal((GfxFatalType::Enum)_code, _str);
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
        callbacks->onTraceVargs(_filePath, _line, _format, _argList);
    }

    /// Return size of for cached item. Return 0 if no cached item was
    /// found.
    ///
    /// @param[in] _id Cache id.
    /// @returns Number of bytes to read.
    uint32_t cacheReadSize(uint64_t _id) override
    {
        return callbacks->onCacheReadSize(_id);
    }

    /// Read cached item.
    ///
    /// @param[in] _id Cache id.
    /// @param[in] _data Buffer where to read data.
    /// @param[in] _size Size of data to read.
    bool cacheRead(uint64_t _id, void* _data, uint32_t _size) override
    {
        return callbacks->onCacheRead(_id, _data, _size);
    }

    /// Write cached item.
    ///
    /// @param[in] _id Cache id.
    /// @param[in] _data Data to write.
    /// @param[in] _size Size of data to write.
    void cacheWrite(uint64_t _id, const void* _data, uint32_t _size) override
    {
        callbacks->onCacheWrite(_id, _data, _size);
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
        callbacks->onScreenShot(_filePath, _width, _height, _pitch, _data, _size, _yflip);
    }

    /// Called when capture begins.
    void captureBegin(uint32_t _width, uint32_t _height, uint32_t _pitch, bgfx::TextureFormat::Enum _format, bool _yflip) override
    {
        callbacks->onCaptureBegin(_width, _height, _pitch, (TextureFormat::Enum)_format, _yflip);
    }

    /// Called when capture ends.
    void captureEnd() override
    {
        callbacks->onCaptureEnd();
    }

    /// Captured frame.
    ///
    /// @param[in] _data Image data.
    /// @param[in] _size Image size.
    void captureFrame(const void* _data, uint32_t _size) override
    {
        callbacks->onCaptureFrame(_data, _size);
    }

    /// Profiler region begin.
    ///
    /// @param[in] _name Region name, contains dynamic string.
    /// @param[in] _abgr Color of profiler region.
    /// @param[in] _filePath File path where profilerBegin was called.
    /// @param[in] _line Line where profilerBegin was called.
    ///
    /// @remarks
    ///   Not thread safe and it can be called from any thread.
    ///
    /// @attention C99 equivalent is `bgfx_callback_vtbl.profiler_begin`.
    ///
    void profilerBegin(const char* _name, uint32_t _abgr, const char* _filePath, uint16_t _line) override
    {
        TEE_PROFILE_BEGIN_STR(gTee, _name, 0);
    }

    /// Profiler region begin with string literal name.
    ///
    /// @param[in] _name Region name, contains string literal.
    /// @param[in] _abgr Color of profiler region.
    /// @param[in] _filePath File path where profilerBeginLiteral was called.
    /// @param[in] _line Line where profilerBeginLiteral was called.
    ///
    /// @remarks
    ///   Not thread safe and it can be called from any thread.
    ///
    /// @attention C99 equivalent is `bgfx_callback_vtbl.profiler_begin_literal`.
    ///
    void profilerBeginLiteral(const char* _name, uint32_t _abgr, const char* _filePath, uint16_t _line) override
    {
        //T_PROFILE_BEGIN_STR(g_core, _name, 0);
    }

    /// Profiler region end.
    ///
    /// @remarks
    ///   Not thread safe and it can be called from any thread.
    ///
    /// @attention C99 equivalent is `bgfx_callback_vtbl.profiler_end`.
    ///
    virtual void profilerEnd() override
    {
        TEE_PROFILE_END(gTee);
    }
};

struct BgfxSmallMemBlock
{
    uint8_t buff[32];
};

struct BgfxWrapper
{
    BgfxCallbacks* callbacks;
    bx::AllocatorI* alloc;
    GfxCaps caps;
    GfxStats stats;
    HMDDesc hmd;
    GfxInternalData internal;
    bx::Pool<BgfxSmallMemBlock> smallPool;

    BgfxWrapper()
    {
        callbacks = nullptr;
        alloc = nullptr;
        bx::memSet(&caps, 0x00, sizeof(caps));
        bx::memSet(&stats, 0x00, sizeof(stats));
        bx::memSet(&hmd, 0x00, sizeof(hmd));
        bx::memSet(&internal, 0x00, sizeof(internal));
    }
};

static BgfxWrapper gBgfx;

static bool initBgfx(uint16_t deviceId, GfxDriverEventsI* callbacks, bx::AllocatorI* alloc)
{
    gBgfx.alloc = alloc;
    if (callbacks) {
        gBgfx.callbacks = BX_NEW(alloc, BgfxCallbacks)(callbacks);
        if (!gBgfx.callbacks)
            return false;
    }

    gBgfx.smallPool.create(512, alloc);

    return bgfx::init(bgfx::RendererType::Count, 0, deviceId, gBgfx.callbacks, alloc);
}

static void shutdownBgfx()
{
    bgfx::frame();
    bgfx::shutdown();

    gBgfx.smallPool.destroy();

    if (gBgfx.callbacks) {
        BX_DELETE(gBgfx.alloc, gBgfx.callbacks);
    }
}

static void resetBgfx(uint32_t width, uint32_t height, GfxResetFlag::Bits flags)
{
    bgfx::reset(width, height, flags);
}

static void resetView(uint8_t viewId)
{
    bgfx::resetView(viewId);
}

static uint32_t frame()
{
    gBgfx.stats.allocTvbSize = 0;
    gBgfx.stats.allocTibSize = 0;
    return bgfx::frame();
}

static void setDebug(GfxDebugFlag::Bits debugFlags)
{
    bgfx::setDebug(debugFlags);
}

static RendererType::Enum getRendererType()
{
    return (RendererType::Enum)bgfx::getRendererType();
}

static const GfxCaps& getCaps()
{
    const bgfx::Caps* caps = bgfx::getCaps();
    gBgfx.caps.type = (RendererType::Enum)caps->rendererType;
    gBgfx.caps.deviceId = caps->deviceId;
    gBgfx.caps.supported = caps->supported;
    gBgfx.caps.vendorId = caps->vendorId;
    gBgfx.caps.homogeneousDepth = caps->homogeneousDepth;
    gBgfx.caps.originBottomLeft = caps->originBottomLeft;
    gBgfx.caps.numGPUs = caps->numGPUs;
        
    for (int i = 0; i < 4; i++) {
        gBgfx.caps.gpu[i].deviceId = caps->gpu[i].deviceId;
        gBgfx.caps.gpu[i].vendorId = caps->gpu[i].vendorId;
    }

    static_assert(TextureFormat::Count == bgfx::TextureFormat::Count, "TextureFormat is not synced with Bgfx");
    memcpy(gBgfx.caps.formats, caps->formats, sizeof(TextureFormat::Enum)*TextureFormat::Count);

    return gBgfx.caps;
}

static const GfxStats& getStats()
{
    const bgfx::Stats* stats = bgfx::getStats();
    gBgfx.stats.cpuTimeFrame = stats->cpuTimeFrame;
    gBgfx.stats.cpuTimeBegin = stats->cpuTimeBegin;
    gBgfx.stats.cpuTimeEnd = stats->cpuTimeEnd;
    gBgfx.stats.cpuTimerFreq = stats->cpuTimerFreq;

    gBgfx.stats.gpuTimeBegin = stats->gpuTimeBegin;
    gBgfx.stats.gpuTimeEnd = stats->gpuTimeEnd;
    gBgfx.stats.gpuTimerFreq = stats->gpuTimerFreq;

    gBgfx.stats.waitRender = stats->waitRender;
    gBgfx.stats.waitSubmit = stats->waitSubmit;

    gBgfx.stats.numDraw = stats->numDraw;
    gBgfx.stats.numCompute = stats->numCompute;
    gBgfx.stats.maxGpuLatency = stats->maxGpuLatency;

    gBgfx.stats.width = stats->width;
    gBgfx.stats.height = stats->height;
    gBgfx.stats.textWidth = stats->textWidth;
    gBgfx.stats.textHeight = stats->textHeight;

    gBgfx.stats.numViews = stats->numViews;
    memcpy(gBgfx.stats.viewStats, stats->viewStats, sizeof(ViewStats)*stats->numViews);

    return gBgfx.stats;
}

static const HMDDesc& getHMD()
{
    const bgfx::HMD* hmd = bgfx::getHMD();
    gBgfx.hmd.deviceWidth = hmd->deviceWidth;
    gBgfx.hmd.deviceHeight = hmd->deviceHeight;
    gBgfx.hmd.width = hmd->width;
    gBgfx.hmd.height = hmd->height;
    gBgfx.hmd.flags = hmd->flags;

    for (int i = 0; i < 2; i++) {
        memcpy(gBgfx.hmd.eye[i].rotation, hmd->eye[i].rotation, sizeof(float) * 4);
        memcpy(gBgfx.hmd.eye[i].translation, hmd->eye[i].translation, sizeof(float) * 3);
        memcpy(gBgfx.hmd.eye[i].fov, hmd->eye[i].fov, sizeof(float) * 4);
        memcpy(gBgfx.hmd.eye[i].viewOffset, hmd->eye[i].viewOffset, sizeof(float) * 3);
    }
    return gBgfx.hmd;
}

static RenderFrameType::Enum renderFrame()
{
    return (RenderFrameType::Enum)bgfx::renderFrame();
}

static void setPlatformData(const GfxPlatformData& data)
{
    bgfx::PlatformData p;
    p.ndt = data.ndt;
    p.nwh = data.nwh;
    p.context = data.context;
    p.backBuffer = data.backBuffer;
    p.backBufferDS = data.backBufferDS;
    bgfx::setPlatformData(p);
}

static const GfxInternalData& getInternalData()
{
    const bgfx::InternalData* d = bgfx::getInternalData();
    gBgfx.internal.caps = &getCaps();
    gBgfx.internal.context = d->context;
    return gBgfx.internal;
}

static void overrideInternal(TextureHandle handle, uintptr_t ptr)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::overrideInternal(h, ptr);
}

static void overrideInternal2(TextureHandle handle, uint16_t width, uint16_t height, uint8_t numMips,       
                              TextureFormat::Enum fmt, TextureFlag::Bits flags)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::overrideInternal(h, width, height, numMips, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags);
}

static void discard()
{
    bgfx::discard();
}

static void touch(uint8_t id)
{
    bgfx::touch(id);
}

static void setPaletteColor(uint8_t index, uint32_t rgba)
{
    bgfx::setPaletteColor(index, rgba);
}

static void setPaletteColorRgba(uint8_t index, float rgba[4])
{
    bgfx::setPaletteColor(index, rgba);
}

static void setPaletteColorRgbaf(uint8_t index, float r, float g, float b, float a)
{
    bgfx::setPaletteColor(index, r, g, b, a);
}

static void setViewName(uint8_t id, const char* name)
{
    bgfx::setViewName(id, name);
}

static void setViewRect(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    bgfx::setViewRect(id, x, y, width, height);
}

static void setViewRectRatio(uint8_t id, uint16_t x, uint16_t y, BackbufferRatio::Enum ratio)
{
    bgfx::setViewRect(id, x, y, (bgfx::BackbufferRatio::Enum)ratio);
}

static void setViewScissor(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    bgfx::setViewScissor(id, x, y, width, height);
}

static void setViewClear(uint8_t id, GfxClearFlag::Bits flags, uint32_t rgba, float depth, uint8_t stencil)
{
    bgfx::setViewClear(id, flags, rgba, depth, stencil);
}
        
static void setViewClearPalette(uint8_t id, GfxClearFlag::Bits flags, float depth, uint8_t stencil,
                                uint8_t color0, uint8_t color1, uint8_t color2, uint8_t color3,
                                uint8_t color4, uint8_t color5, uint8_t color6, uint8_t color7)
{
    bgfx::setViewClear(id, flags, depth, stencil, color0, color1, color2, color3, color4, color5, color6, color7);
}

static void setViewMode(uint8_t id, ViewMode::Enum mode)
{
    bgfx::setViewMode(id, (bgfx::ViewMode::Enum)mode);
}

static void setViewTransform(uint8_t id, const void* view, const void* projLeft, GfxViewFlag::Bits flags, 
                             const void* projRight)
{
    bgfx::setViewTransform(id, view, projLeft, flags, projRight);
}

static void setViewFrameBuffer(uint8_t id, FrameBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
    bgfx::setViewFrameBuffer(id, h);
}

static void setMarker(const char* marker)
{
    bgfx::setMarker(marker);
}

static void setState(GfxState::Bits state, uint32_t rgba)
{
    bgfx::setState(state, rgba);
}

static void setStencil(GfxStencilState::Bits frontStencil, GfxStencilState::Bits backStencil)
{
    bgfx::setStencil(frontStencil, backStencil);
}

static uint16_t setScissor(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    return bgfx::setScissor(x, y, width, height);
}

static void setScissorCache(uint16_t cache)
{
    bgfx::setScissor(cache);
}

static uint32_t allocTransform(GpuTransform* transform, uint16_t num)
{
    bgfx::Transform t;
    uint32_t r = bgfx::allocTransform(&t, num);
    transform->data = t.data;
    transform->num = t.num;
    return r;
}

static uint32_t setTransform(const void* mtx, uint16_t num)
{
    return bgfx::setTransform(mtx, num);
}

static void setTransformCached(uint32_t cache, uint16_t num)
{
    bgfx::setTransform(cache, num);
}

static void setCondition(OcclusionQueryHandle handle, bool visible)
{
    BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
    bgfx::setCondition(h, visible);
}

static void setIndexBuffer(IndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices)
{
    BGFX_DECLARE_HANDLE(IndexBufferHandle, h, handle);
    bgfx::setIndexBuffer(h, firstIndex, numIndices);
}

static void setDynamicIndexBuffer(DynamicIndexBufferHandle handle, uint32_t firstIndex, uint32_t numIndices)
{
    BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
    bgfx::setIndexBuffer(h, firstIndex, numIndices);
}

static void setTransientIndexBufferI(const TransientIndexBuffer* tib, uint32_t firstIndex, uint32_t numIndices)
{
    bgfx::setIndexBuffer((const bgfx::TransientIndexBuffer*)tib, firstIndex, numIndices);
}

static void setTransientIndexBuffer(const TransientIndexBuffer* tib)
{
    bgfx::setIndexBuffer((const bgfx::TransientIndexBuffer*)tib);
}

static void setVertexBuffer(uint8_t stream, VertexBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
    bgfx::setVertexBuffer(stream, h);
}

static void setVertexBufferI(uint8_t stream, VertexBufferHandle handle, uint32_t vertexIndex, uint32_t numVertices)
{
    BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
    bgfx::setVertexBuffer(stream, h, vertexIndex, numVertices);
}

static void setDynamicVertexBuffer(uint8_t stream, DynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t numVertices)
{
    BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
    bgfx::setVertexBuffer(stream, h, startVertex, numVertices);
}

static void setTransientVertexBuffer(uint8_t stream, const TransientVertexBuffer* tvb)
{
    bgfx::setVertexBuffer(stream, (const bgfx::TransientVertexBuffer*)tvb);
}

static void setTransientVertexBufferI(uint8_t stream, const TransientVertexBuffer* tvb, uint32_t startVertex, uint32_t numVertices)
{
    bgfx::setVertexBuffer(stream, (const bgfx::TransientVertexBuffer*)tvb, startVertex, numVertices);
}

static void setInstanceDataBuffer(const InstanceDataBuffer* idb, uint32_t num)
{
    bgfx::setInstanceDataBuffer((const bgfx::InstanceDataBuffer*)idb, num);
}

static void setInstanceDataBufferVb(VertexBufferHandle handle, uint32_t startVertex, uint32_t num)
{
    BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
    bgfx::setInstanceDataBuffer(h, startVertex, num);
}

static void setInstanceDataBufferDynamicVb(DynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t num)
{
    BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
    bgfx::setInstanceDataBuffer(h, startVertex, num);
}

static void setTexture(uint8_t stage, UniformHandle sampler, TextureHandle handle, TextureFlag::Bits flags)
{
    BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);

    bgfx::setTexture(stage, s, h, flags);
}

static void submit(uint8_t viewId, ProgramHandle program, int32_t depth, bool preserveState)
{
    BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
    bgfx::submit(viewId, p, depth, preserveState);
}

static void submitWithOccQuery(uint8_t viewId, ProgramHandle program, OcclusionQueryHandle occQuery, int32_t depth, bool preserveState)
{
    BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
    BGFX_DECLARE_HANDLE(OcclusionQueryHandle, o, occQuery);
    bgfx::submit(viewId, p, o, depth, preserveState);
}

static void submitIndirect(uint8_t viewId, ProgramHandle program, IndirectBufferHandle indirectHandle, uint16_t start,
    uint16_t num, int32_t depth, bool preserveState)
{
    BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
    BGFX_DECLARE_HANDLE(IndirectBufferHandle, i, indirectHandle);
    bgfx::submit(viewId, p, i, start, num, depth, preserveState);
}

static void setComputeBufferIb(uint8_t stage, IndexBufferHandle handle, GpuAccessFlag::Enum access)
{
    BGFX_DECLARE_HANDLE(IndexBufferHandle, h, handle);
    bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
}

static void setComputeBufferVb(uint8_t stage, VertexBufferHandle handle, GpuAccessFlag::Enum access)
{
    BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
    bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
}

static void setComputeBufferDynamicIb(uint8_t stage, DynamicIndexBufferHandle handle, GpuAccessFlag::Enum access)
{
    BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
    bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
}

static void setComputeBufferDynamicVb(uint8_t stage, DynamicVertexBufferHandle handle, GpuAccessFlag::Enum access)
{
    BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
    bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
}

static void setComputeBufferIndirect(uint8_t stage, IndirectBufferHandle handle, GpuAccessFlag::Enum access)
{
    BGFX_DECLARE_HANDLE(IndirectBufferHandle, h, handle);
    bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
}

static void setComputeImage(uint8_t stage, UniformHandle sampler, TextureHandle handle, uint8_t mip, 
                            GpuAccessFlag::Enum access, TextureFormat::Enum fmt)
{
    BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::setImage(stage, s, h, mip, (bgfx::Access::Enum)access, (bgfx::TextureFormat::Enum)fmt);
}

static void computeDispatch(uint8_t viewId, ProgramHandle handle, uint32_t numX, uint32_t numY, uint32_t numZ,
                            GfxSubmitFlag::Bits flags)
{
    BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
    bgfx::dispatch(viewId, h, numX, numY, numZ, flags);
}

static void computeDispatchIndirect(uint8_t viewId, ProgramHandle handle, IndirectBufferHandle indirectHandle,
                                    uint16_t start, uint16_t num, GfxSubmitFlag::Bits flags)
{
    BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
    BGFX_DECLARE_HANDLE(IndirectBufferHandle, i, indirectHandle);
    bgfx::dispatch(viewId, h, i, start, num, flags);
}

static void blit(uint8_t viewId, TextureHandle dest, uint16_t destX, uint16_t destY, TextureHandle src,
            uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height)
{
    BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
    BGFX_DECLARE_HANDLE(TextureHandle, s, src);
    bgfx::blit(viewId, d, destX, destY, s, srcX, srcY, width, height);
}

static void blitMip(uint8_t viewId, TextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
            uint16_t destZ, TextureHandle src, uint8_t srcMip, uint16_t srcX, uint16_t srcY,
            uint16_t srcZ, uint16_t width, uint16_t height, uint16_t depth)
{
    BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
    BGFX_DECLARE_HANDLE(TextureHandle, s, src);
    bgfx::blit(viewId, d, destMip, destX, destY, destZ, s, srcMip, srcX, srcY, srcZ, width, height, depth);
}

static const GfxMemory* allocMem(uint32_t size)
{
    return (const GfxMemory*)bgfx::alloc(size);
}

static const GfxMemory* copy(const void* data, uint32_t size)
{
    return (const GfxMemory*)bgfx::copy(data, size);
}

static const GfxMemory* makeRef(const void* data, uint32_t size, GfxReleaseMemCallback releaseFn, void* userData)
{
    return (const GfxMemory*)bgfx::makeRef(data, size, releaseFn, userData);
}

static bool isTextureValid(uint16_t depth, bool cube, uint16_t numLayers, TextureFormat::Enum fmt, uint32_t flags)
{
    return bgfx::isTextureValid(depth, cube, numLayers, (bgfx::TextureFormat::Enum)fmt, flags);
}

static ShaderHandle createShader(const GfxMemory* mem)
{
    ShaderHandle handle;
    handle.value = bgfx::createShader((const bgfx::Memory*)mem).idx;
    return handle;
}

static uint16_t getShaderUniforms(ShaderHandle handle, UniformHandle* uniforms, uint16_t _max)
{
    BGFX_DECLARE_HANDLE(ShaderHandle, h, handle);
    return bgfx::getShaderUniforms(h, (bgfx::UniformHandle*)uniforms, _max);
}

static void destroyShader(ShaderHandle handle)
{
    BGFX_DECLARE_HANDLE(ShaderHandle, h, handle);
    bgfx::destroy(h);
}

static void destroyUniform(UniformHandle handle)
{
    BGFX_DECLARE_HANDLE(UniformHandle, h, handle);
    bgfx::destroy(h);
}

static ProgramHandle createProgram(ShaderHandle vsh, ShaderHandle fsh, bool destroyShaders)
{
    BGFX_DECLARE_HANDLE(ShaderHandle, v, vsh);
    BGFX_DECLARE_HANDLE(ShaderHandle, f, fsh);
    ProgramHandle h;
    h.value = bgfx::createProgram(v, f, destroyShaders).idx;
    return h;
}

static void destroyProgram(ProgramHandle handle)
{
    assert(handle.isValid());
    BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
    bgfx::destroy(h);
}

static UniformHandle createUniform(const char* name, UniformType::Enum type, uint16_t num)
{
    UniformHandle handle;
    handle.value = bgfx::createUniform(name, (bgfx::UniformType::Enum)type, num).idx;
    return handle;
}

static void setUniform(UniformHandle handle, const void* value, uint16_t num)
{
    BGFX_DECLARE_HANDLE(UniformHandle, h, handle);
    bgfx::setUniform(h, value, num);
}

static VertexBufferHandle createVertexBuffer(const GfxMemory* mem, const VertexDecl& decl, GfxBufferFlag::Bits flags)
{
    VertexBufferHandle handle;
    handle.value = bgfx::createVertexBuffer((const bgfx::Memory*)mem, (const bgfx::VertexDecl&)decl, flags).idx;
    return handle;
}

static DynamicVertexBufferHandle createDynamicVertexBuffer(uint32_t numVertices, const VertexDecl& decl, 
                                                           GfxBufferFlag::Bits flags)
{
    DynamicVertexBufferHandle handle;
    handle.value = bgfx::createDynamicVertexBuffer(numVertices, (const bgfx::VertexDecl&)decl, flags).idx;
    return handle;
}

static DynamicVertexBufferHandle createDynamicVertexBufferMem(const GfxMemory* mem, const VertexDecl& decl,
                                                              GfxBufferFlag::Bits flags = GfxBufferFlag::None)
{
    DynamicVertexBufferHandle handle;
    handle.value = bgfx::createDynamicVertexBuffer((const bgfx::Memory*)mem, (const bgfx::VertexDecl&)decl, flags).idx;
    return handle;
}

static void updateDynamicVertexBuffer(DynamicVertexBufferHandle handle, uint32_t startVertex, const GfxMemory* mem)
{
    BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
    bgfx::updateDynamicVertexBuffer(h, startVertex, (const bgfx::Memory*)mem);
}

static void destroyVertexBuffer(VertexBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
    bgfx::destroy(h);
}

static void destroyDynamicVertexBuffer(DynamicVertexBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
    bgfx::destroy(h);
}

static uint32_t getAvailTransientVertexBuffer(uint32_t num, const VertexDecl& decl)
{
    return bgfx::getAvailTransientVertexBuffer(num, (const bgfx::VertexDecl&)decl);
}

static void allocTransientVertexBuffer(TransientVertexBuffer* tvb, uint32_t num, const VertexDecl& decl)
{
    bgfx::allocTransientVertexBuffer((bgfx::TransientVertexBuffer*)tvb, num, (const bgfx::VertexDecl&)decl);
    gBgfx.stats.allocTvbSize += tvb->size;
    gBgfx.stats.maxTvbSize = bx::uint32_max(gBgfx.stats.allocTvbSize, gBgfx.stats.maxTvbSize);
}

static bool allocTransientBuffers(TransientVertexBuffer* tvb, const VertexDecl& decl, uint32_t numVerts,
                                  TransientIndexBuffer* tib, uint16_t numIndices)
{
    bool r = bgfx::allocTransientBuffers((bgfx::TransientVertexBuffer*)tvb, (const bgfx::VertexDecl&)decl, numVerts,
                                         (bgfx::TransientIndexBuffer*)tib, numIndices);
    gBgfx.stats.allocTvbSize += tvb->size;
    gBgfx.stats.maxTvbSize = bx::uint32_max(gBgfx.stats.allocTvbSize, gBgfx.stats.maxTvbSize);
    gBgfx.stats.allocTibSize += tib->size;
    gBgfx.stats.maxTibSize = bx::uint32_max(gBgfx.stats.allocTibSize, gBgfx.stats.maxTibSize);
    return r;
}

static IndexBufferHandle createIndexBuffer(const GfxMemory* mem, GfxBufferFlag::Bits flags)
{
    IndexBufferHandle handle;
    handle.value = bgfx::createIndexBuffer((const bgfx::Memory*)mem, flags).idx;
    return handle;
}

static DynamicIndexBufferHandle createDynamicIndexBuffer(uint32_t num, GfxBufferFlag::Bits flags)
{
    DynamicIndexBufferHandle handle;
    handle.value = bgfx::createDynamicIndexBuffer(num, flags).idx;
    return handle;
}

static DynamicIndexBufferHandle createDynamicIndexBufferMem(const GfxMemory* mem, GfxBufferFlag::Bits flags)
{
    DynamicIndexBufferHandle handle;
    handle.value = bgfx::createDynamicIndexBuffer((const bgfx::Memory*)mem, flags).idx;
    return handle;
}

static void updateDynamicIndexBuffer(DynamicIndexBufferHandle handle, uint32_t startIndex, const GfxMemory* mem)
{
    BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
    bgfx::updateDynamicIndexBuffer(h, startIndex, (const bgfx::Memory*)mem);
}

static void destroyIndexBuffer(IndexBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(IndexBufferHandle, h, handle);
    bgfx::destroy(h);
}

static void destroyDynamicIndexBuffer(DynamicIndexBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
    bgfx::destroy(h);
}

static uint32_t getAvailTransientIndexBuffer(uint32_t num)
{
    return bgfx::getAvailTransientIndexBuffer(num);
}

static void allocTransientIndexBuffer(TransientIndexBuffer* tib, uint32_t num)
{
    bgfx::allocTransientIndexBuffer((bgfx::TransientIndexBuffer*)tib, num);
    gBgfx.stats.allocTibSize += tib->size;
    gBgfx.stats.maxTibSize = bx::uint32_max(gBgfx.stats.allocTibSize, gBgfx.stats.maxTibSize);
}

static void calcTextureSize(TextureInfo* info, uint16_t width, uint16_t height, uint16_t depth, bool cubemap,
                            bool hasMips, uint16_t numLayers, TextureFormat::Enum fmt)
{
    bgfx::calcTextureSize((bgfx::TextureInfo&)(*info), width, height, depth, cubemap, hasMips, numLayers, 
                          (bgfx::TextureFormat::Enum)fmt);
}

static TextureHandle createTexture2D(uint16_t width, uint16_t height, bool hasMips, uint16_t numLayers,
                                     TextureFormat::Enum fmt, TextureFlag::Bits flags, const GfxMemory* mem)
{
    TextureHandle r;
    r.value = bgfx::createTexture2D(width, height, hasMips, numLayers, (bgfx::TextureFormat::Enum)fmt, flags,
                                    (const bgfx::Memory*)mem).idx;
    return r;
}

static TextureHandle createTexture2DRatio(BackbufferRatio::Enum ratio, bool hasMips, uint16_t numLayers, 
                                          TextureFormat::Enum fmt, TextureFlag::Bits flags)
{
    TextureHandle r;
    r.value = bgfx::createTexture2D((bgfx::BackbufferRatio::Enum)ratio, hasMips, numLayers, 
                                    (bgfx::TextureFormat::Enum)fmt, flags).idx;
    return r;
}

static void updateTexture2D(TextureHandle handle, uint16_t layer, uint8_t mip, uint16_t x, uint16_t y, uint16_t width,
                            uint16_t height, const GfxMemory* mem, uint16_t pitch)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::updateTexture2D(h, layer, mip, x, y, width, height, (const bgfx::Memory*)mem, pitch);
}

static TextureHandle createTexture3D(uint16_t width, uint16_t height, uint16_t depth, bool hasMips,
                                     TextureFormat::Enum fmt, TextureFlag::Bits flags, const GfxMemory* mem)
{
    TextureHandle r;
    r.value = bgfx::createTexture3D(width, height, depth, hasMips, (bgfx::TextureFormat::Enum)fmt, flags, 
                                    (const bgfx::Memory*)mem).idx;
    return r;
}

static void updateTexture3D(TextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t z,
                            uint16_t width, uint16_t height, uint16_t depth, const GfxMemory* mem)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::updateTexture3D(h, mip, x, y, z, width, height, depth, (const bgfx::Memory*)mem);
}

static TextureHandle createTextureCube(uint16_t size, bool hasMips, uint16_t numLayers, TextureFormat::Enum fmt, 
                                       TextureFlag::Bits flags, const GfxMemory* mem)
{
    TextureHandle r;
    r.value = bgfx::createTextureCube(size, hasMips, numLayers, (bgfx::TextureFormat::Enum)fmt, flags, 
                                      (const bgfx::Memory*)mem).idx;
    return r;
}

static void updateTextureCube(TextureHandle handle, uint16_t layer, CubeSide::Enum side, uint8_t mip, uint16_t x, uint16_t y,
                              uint16_t width, uint16_t height, const GfxMemory* mem, uint16_t pitch)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::updateTextureCube(h, layer, (uint8_t)side, mip, x, y, width, height, (const bgfx::Memory*)mem, pitch);
}

static void readTexture(TextureHandle handle, void* data, uint8_t mip)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::readTexture(h, data, mip);
}

static void destroyTexture(TextureHandle handle)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::destroy(h);
}

static FrameBufferHandle createFrameBuffer(uint16_t width, uint16_t height, TextureFormat::Enum fmt, TextureFlag::Bits flags)
{
    FrameBufferHandle r;
    r.value = bgfx::createFrameBuffer(width, height, (bgfx::TextureFormat::Enum)fmt, flags).idx;
    return r;
}

static FrameBufferHandle createFrameBufferRatio(BackbufferRatio::Enum ratio, TextureFormat::Enum fmt, TextureFlag::Bits flags)
{
    FrameBufferHandle r;
    r.value = bgfx::createFrameBuffer((bgfx::BackbufferRatio::Enum)ratio, (bgfx::TextureFormat::Enum)fmt, flags).idx;
    return r;
}

static FrameBufferHandle createFrameBufferMRT(uint8_t num, const TextureHandle* handles, bool destroyTextures)
{
    FrameBufferHandle r;
    r.value = bgfx::createFrameBuffer(num, (const bgfx::TextureHandle*)handles, destroyTextures).idx;
    return r;
}

static FrameBufferHandle createFrameBufferAttachment(uint8_t num, const GfxAttachment* attachment, bool destroyTextures)
{
    FrameBufferHandle r;
    r.value = bgfx::createFrameBuffer(num, (const bgfx::Attachment*)attachment, destroyTextures).idx;
    return r;
}

static FrameBufferHandle createFrameBufferNative(void* nwh, uint16_t width, uint16_t height, TextureFormat::Enum depthFmt)
{
    FrameBufferHandle r;
    r.value = bgfx::createFrameBuffer(nwh, width, height, (bgfx::TextureFormat::Enum)depthFmt).idx;
    return r;
}

static void destroyFrameBuffer(FrameBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
    bgfx::destroy(h);
}

static TextureHandle getFrameBufferTexture(FrameBufferHandle handle, uint8_t attachment)
{
    BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
    TextureHandle r;
    r.value = bgfx::getTexture(h, attachment).idx;
    return r;
}

static uint32_t getAvailInstanceDataBuffer(uint32_t num, uint16_t stride)
{
    return bgfx::getAvailInstanceDataBuffer(num, stride);
}

static void allocInstanceDataBuffer(InstanceDataBuffer* ibuff, uint32_t num, uint16_t stride)
{
    bgfx::allocInstanceDataBuffer((bgfx::InstanceDataBuffer*)ibuff, num, stride);
}

static IndirectBufferHandle createIndirectBuffer(uint32_t num)
{
    IndirectBufferHandle r;
    r.value = bgfx::createIndirectBuffer(num).idx;
    return r;
}

static void destroyIndirectBuffer(IndirectBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(IndirectBufferHandle, h, handle);
    bgfx::destroy(h);
}

static OcclusionQueryHandle createOccQuery()
{
    OcclusionQueryHandle r;
    r.value = bgfx::createOcclusionQuery().idx;
    return r;
}

static OcclusionQueryResult::Enum getResult(OcclusionQueryHandle handle)
{
    BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
    return (OcclusionQueryResult::Enum)bgfx::getResult(h);
}

static void destroyOccQuery(OcclusionQueryHandle handle)
{
    BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
    bgfx::destroy(h);
}

static void dbgTextClear(uint8_t attr, bool small)
{
    bgfx::dbgTextClear(attr, small);
}

static void dbgTextPrintf(uint16_t x, uint16_t y, uint8_t attr, const char* format, ...)
{
    char text[256];

    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    bgfx::dbgTextPrintf(x, y, attr, text);
}

static void dbgTextImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const void* data, uint16_t pitch)
{
    bgfx::dbgTextImage(x, y, width, height, data, pitch);
}

PluginDesc* getBgfxDriverDesc()
{
    static PluginDesc desc;
    strcpy(desc.name, "Bgfx");
    strcpy(desc.description, "Bgfx Driver");
    desc.type = PluginType::GraphicsDriver;
    desc.version = TEE_MAKE_VERSION(1, 0);
    return &desc;
}

void* initBgfxDriver(bx::AllocatorI* alloc, GetApiFunc getApi)
{
    static GfxDriver api;
    bx::memSet(&api, 0x00, sizeof(api));
    api.init = initBgfx;
    api.shutdown = shutdownBgfx;
    api.reset = resetBgfx;
    api.frame = frame;
    api.setDebug = setDebug;
    api.getRendererType = getRendererType;
    api.getCaps = getCaps;
    api.getStats = getStats;
    api.getHMD = getHMD;
    api.renderFrame = renderFrame;
    api.setPlatformData = setPlatformData;
    api.getInternalData = getInternalData;
    api.overrideInternal = overrideInternal;
    api.overrideInternal2 = overrideInternal2;
    api.discard = discard;
    api.touch = touch;
    api.setPaletteColor = setPaletteColor;
    api.setPaletteColorRgba = setPaletteColorRgba;
    api.setPaletteColorRgbaf = setPaletteColorRgbaf;
    api.setViewName = setViewName;
    api.setViewRect = setViewRect;
    api.setViewRectRatio = setViewRectRatio;
    api.setViewScissor = setViewScissor;
    api.setViewClear = setViewClear;
    api.setViewClearPalette = setViewClearPalette;
    api.setViewMode = setViewMode;
    api.setViewTransform = setViewTransform;
    api.setViewFrameBuffer = setViewFrameBuffer;
    api.resetView = resetView;
    api.setMarker = setMarker;
    api.setState = setState;
    api.setStencil = setStencil;
    api.setScissor = setScissor;
    api.setScissorCache = setScissorCache;
    api.allocTransform = allocTransform;
    api.setTransform = setTransform;
    api.setTransformCached = setTransformCached;
    api.setCondition = setCondition;
    api.setIndexBuffer = setIndexBuffer;
    api.setDynamicIndexBuffer = setDynamicIndexBuffer;
    api.setTransientIndexBuffer = setTransientIndexBuffer;
    api.setTransientIndexBufferI = setTransientIndexBufferI;
    api.setVertexBuffer = setVertexBuffer;
    api.setVertexBufferI = setVertexBufferI;
    api.setDynamicVertexBuffer = setDynamicVertexBuffer;
    api.setTransientVertexBuffer = setTransientVertexBuffer;
    api.setTransientVertexBufferI = setTransientVertexBufferI;
    api.setInstanceDataBuffer = setInstanceDataBuffer;
    api.setInstanceDataBufferVb = setInstanceDataBufferVb;
    api.setInstanceDataBufferDynamicVb = setInstanceDataBufferDynamicVb;
    api.setTexture = setTexture;
    api.submit = submit;
    api.submitWithOccQuery = submitWithOccQuery;
    api.submitIndirect = submitIndirect;
    api.setComputeBufferIb = setComputeBufferIb;
    api.setComputeBufferVb = setComputeBufferVb;
    api.setComputeBufferDynamicVb = setComputeBufferDynamicVb;
    api.setComputeBufferDynamicIb = setComputeBufferDynamicIb;
    api.setComputeBufferIndirect = setComputeBufferIndirect;
    api.setComputeImage = setComputeImage;
    api.computeDispatch = computeDispatch;
    api.computeDispatchIndirect = computeDispatchIndirect;
    api.blit = blit;
    api.blitMip = blitMip;
    api.alloc = allocMem;
    api.copy = copy;
    api.makeRef = makeRef;
    api.createShader = createShader;
    api.getShaderUniforms = getShaderUniforms;
    api.destroyShader = destroyShader;
    api.createProgram = createProgram;
    api.destroyProgram = destroyProgram;
    api.destroyUniform = destroyUniform;
    api.createUniform = createUniform;
    api.setUniform = setUniform;
    api.createVertexBuffer = createVertexBuffer;
    api.createDynamicVertexBuffer = createDynamicVertexBuffer;
    api.createDynamicVertexBufferMem = createDynamicVertexBufferMem;
    api.updateDynamicVertexBuffer = updateDynamicVertexBuffer;
    api.destroyVertexBuffer = destroyVertexBuffer;
    api.destroyDynamicVertexBuffer = destroyDynamicVertexBuffer;
    api.getAvailTransientVertexBuffer = getAvailTransientVertexBuffer;
    api.getAvailTransientIndexBuffer = getAvailTransientIndexBuffer;
    api.allocTransientVertexBuffer = allocTransientVertexBuffer;
    api.allocTransientIndexBuffer = allocTransientIndexBuffer;
    api.allocTransientBuffers = allocTransientBuffers;
    api.createIndexBuffer = createIndexBuffer;
    api.createDynamicIndexBuffer = createDynamicIndexBuffer;
    api.createDynamicVertexBufferMem = createDynamicVertexBufferMem;
    api.updateDynamicIndexBuffer = updateDynamicIndexBuffer;
    api.createDynamicIndexBufferMem = createDynamicIndexBufferMem;
    api.destroyIndexBuffer = destroyIndexBuffer;
    api.destroyDynamicIndexBuffer = destroyDynamicIndexBuffer;
    api.destroyDynamicVertexBuffer = destroyDynamicVertexBuffer;
    api.calcTextureSize = calcTextureSize;
    api.createTexture2D = createTexture2D;
    api.createTexture2DRatio = createTexture2DRatio;
    api.updateTexture2D = updateTexture2D;
    api.createTexture3D = createTexture3D;
    api.updateTexture3D = updateTexture3D;
    api.createTextureCube = createTextureCube;
    api.updateTextureCube = updateTextureCube;
    api.readTexture = readTexture;
    api.isTextureValid = isTextureValid;
    api.destroyTexture = destroyTexture;
    api.createFrameBuffer = createFrameBuffer;
    api.createFrameBufferRatio = createFrameBufferRatio;
    api.createFrameBufferMRT = createFrameBufferMRT;
    api.createFrameBufferNative = createFrameBufferNative;
    api.createFrameBufferAttachment = createFrameBufferAttachment;
    api.destroyFrameBuffer = destroyFrameBuffer;
    api.getFrameBufferTexture = getFrameBufferTexture;
    api.getAvailInstanceDataBuffer = getAvailInstanceDataBuffer;
    api.allocInstanceDataBuffer = allocInstanceDataBuffer;
    api.createIndirectBuffer = createIndirectBuffer;
    api.destroyIndirectBuffer = destroyIndirectBuffer;
    api.createOccQuery = createOccQuery;
    api.getResult = getResult;
    api.destroyOccQuery = destroyOccQuery;
    api.dbgTextClear = dbgTextClear;
    api.dbgTextPrintf = dbgTextPrintf;
    api.dbgTextImage = dbgTextImage;

    // Some assertions for enum matching between ours and bgfx
    static_assert(RendererType::Count == bgfx::RendererType::Count, "RendererType mismatch");
    static_assert(RendererType::Vulkan == bgfx::RendererType::Vulkan, "RendererType mismatch");
    static_assert(GpuAccessFlag::Count == bgfx::Access::Count, "AccessFlag mismatch");
    static_assert(VertexAttrib::TexCoord7  == bgfx::Attrib::TexCoord7, "VertexAttrib Mismatch");
    static_assert(VertexAttribType::Float == bgfx::AttribType::Float, "VertexAttribType mismatch");
    static_assert(TextureFormat::Unknown == bgfx::TextureFormat::Unknown, "TextureFormat mismatch");
    static_assert(TextureFormat::RG11B10F == bgfx::TextureFormat::RG11B10F, "TextureFormat mismatch");
    static_assert(TextureFormat::Count == bgfx::TextureFormat::Count, "TextureFormat mismatch");
    static_assert(UniformType::Count == bgfx::UniformType::Count, "UniformType mismatch");
    static_assert(BackbufferRatio::Count == bgfx::BackbufferRatio::Count, "BackbufferRatio mismatch");
    static_assert(OcclusionQueryResult::Count == bgfx::OcclusionQueryResult::Count, "OcclusionQueryResult mismatch");
    static_assert(sizeof(TransientIndexBuffer) == sizeof(bgfx::TransientIndexBuffer), "TransientIndexBuffer mismatch");
    static_assert(sizeof(TransientVertexBuffer) == sizeof(bgfx::TransientVertexBuffer), "TransientVertexBuffer mismatch");
    static_assert(sizeof(InstanceDataBuffer) == sizeof(bgfx::InstanceDataBuffer), "InstanceDataBuffer mismatch");
    static_assert(sizeof(TextureInfo) == sizeof(bgfx::TextureInfo), "TextureInfo mismatch");
    static_assert(sizeof(GfxAttachment) == sizeof(bgfx::Attachment), "GfxAttachment mismatch");
    static_assert(sizeof(GpuTransform) == sizeof(bgfx::Transform), "GpuTransform mismatch");
    static_assert(sizeof(HMDDesc) == sizeof(bgfx::HMD), "HMD mismatch");
    static_assert(sizeof(VertexDecl) == sizeof(bgfx::VertexDecl), "VertexDecl mismatch");
    static_assert(sizeof(GfxMemory) == sizeof(bgfx::Memory), "Memory mismatch");

    gTee = (CoreApi*)getApi(ApiId::Core, 0);

    return &api;
}

void shutdownBgfxDriver()
{
}

#ifdef termite_SHARED_LIB
TEE_PLUGIN_EXPORT void* termiteGetPluginApi(uint16_t apiId, uint32_t version)
{
    static PluginApi v0;

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
