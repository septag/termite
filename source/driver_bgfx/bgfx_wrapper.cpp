#include "termite/core.h"

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
    GfxDriverEventsI* callbacks;

public:
    BgfxCallbacks(GfxDriverEventsI* callbacks)
    {
        assert(callbacks);
        callbacks = callbacks;
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
    void fatal(bgfx::Fatal::Enum _code, const char* _str)
    {
        callbacks->onFatal((GfxFatalType)_code, _str);
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
    void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList)
    {
        callbacks->onTraceVargs(_filePath, _line, _format, _argList);
    }

    /// Return size of for cached item. Return 0 if no cached item was
    /// found.
    ///
    /// @param[in] _id Cache id.
    /// @returns Number of bytes to read.
    uint32_t cacheReadSize(uint64_t _id)
    {
        return callbacks->onCacheReadSize(_id);
    }

    /// Read cached item.
    ///
    /// @param[in] _id Cache id.
    /// @param[in] _data Buffer where to read data.
    /// @param[in] _size Size of data to read.
    bool cacheRead(uint64_t _id, void* _data, uint32_t _size)
    {
        return callbacks->onCacheRead(_id, _data, _size);
    }

    /// Write cached item.
    ///
    /// @param[in] _id Cache id.
    /// @param[in] _data Data to write.
    /// @param[in] _size Size of data to write.
    void cacheWrite(uint64_t _id, const void* _data, uint32_t _size)
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
    void screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data, uint32_t _size, bool _yflip)
    {
        callbacks->onScreenShot(_filePath, _width, _height, _pitch, _data, _size, _yflip);
    }

    /// Called when capture begins.
    void captureBegin(uint32_t _width, uint32_t _height, uint32_t _pitch, bgfx::TextureFormat::Enum _format, bool _yflip)
    {
        callbacks->onCaptureBegin(_width, _height, _pitch, (TextureFormat)_format, _yflip);
    }

    /// Called when capture ends.
    void captureEnd()
    {
        callbacks->onCaptureEnd();
    }

    /// Captured frame.
    ///
    /// @param[in] _data Image data.
    /// @param[in] _size Image size.
    void captureFrame(const void* _data, uint32_t _size)
    {
        callbacks->onCaptureFrame(_data, _size);
    }
};

struct BgfxWrapper
{
    BgfxCallbacks* callbacks;
    bx::AllocatorI* alloc;
    GfxCaps caps;
    GfxStats stats;
    HMDDesc hmd;
    GfxInternalData internal;

    BgfxWrapper()
    {
        callbacks = nullptr;
        alloc = nullptr;
        memset(&caps, 0x00, sizeof(caps));
        memset(&stats, 0x00, sizeof(stats));
        memset(&hmd, 0x00, sizeof(hmd));
        memset(&internal, 0x00, sizeof(internal));
    }
};

static BgfxWrapper g_bgfx;

static result_t initBgfx(uint16_t deviceId, GfxDriverEventsI* callbacks, bx::AllocatorI* alloc)
{
    g_bgfx.alloc = alloc;
    if (callbacks) {
        g_bgfx.callbacks = BX_NEW(alloc, BgfxCallbacks)(callbacks);
        if (!g_bgfx.callbacks)
            return T_ERR_OUTOFMEM;
    }

    return bgfx::init(bgfx::RendererType::Count, 0, deviceId, g_bgfx.callbacks, alloc) ? 0 : T_ERR_FAILED;
}

static void shutdownBgfx()
{
    bgfx::shutdown();
    if (g_bgfx.callbacks) {
        BX_DELETE(g_bgfx.alloc, g_bgfx.callbacks);
    }
}

static void resetBgfx(uint32_t width, uint32_t height, GfxResetFlag flags)
{
    bgfx::reset(width, height, (uint32_t)flags);
}

static uint32_t frame()
{
    return bgfx::frame();
}

static void setDebug(GfxDebugFlag debugFlags)
{
    bgfx::setDebug((uint32_t)debugFlags);
}

static RendererType getRendererType()
{
    return (RendererType)bgfx::getRendererType();
}

static const GfxCaps& getCaps()
{
    const bgfx::Caps* caps = bgfx::getCaps();
    g_bgfx.caps.deviceId = caps->deviceId;
    g_bgfx.caps.supported = (GpuCapsFlag)caps->supported;
    g_bgfx.caps.maxDrawCalls = caps->maxDrawCalls;
    g_bgfx.caps.maxFBAttachments = caps->maxFBAttachments;
    g_bgfx.caps.maxTextureSize = caps->maxTextureSize;
    g_bgfx.caps.maxViews = caps->maxViews;
    g_bgfx.caps.numGPUs = caps->numGPUs;
    g_bgfx.caps.type = (RendererType)caps->rendererType;
    g_bgfx.caps.vendorId = caps->vendorId;
        
    for (int i = 0; i < 4; i++) {
        g_bgfx.caps.gpu[i].deviceId = caps->gpu[i].deviceId;
        g_bgfx.caps.gpu[i].vendorId = caps->gpu[i].vendorId;
    }

    return g_bgfx.caps;
}

static const GfxStats& getStats()
{
    const bgfx::Stats* stats = bgfx::getStats();
    g_bgfx.stats.cpuTimeBegin = stats->cpuTimeBegin;
    g_bgfx.stats.cpuTimeEnd = stats->cpuTimeEnd;
    g_bgfx.stats.cpuTimerFreq = stats->cpuTimerFreq;
    g_bgfx.stats.gpuTimeBegin = stats->gpuTimeBegin;
    g_bgfx.stats.gpuTimeEnd = stats->gpuTimeEnd;
    g_bgfx.stats.gpuTimerFreq = stats->gpuTimerFreq;
    return g_bgfx.stats;
}

static const HMDDesc& getHMD()
{
    const bgfx::HMD* hmd = bgfx::getHMD();
    g_bgfx.hmd.deviceWidth = hmd->deviceWidth;
    g_bgfx.hmd.deviceHeight = hmd->deviceHeight;
    g_bgfx.hmd.width = hmd->width;
    g_bgfx.hmd.height = hmd->height;
    g_bgfx.hmd.flags = hmd->flags;

    for (int i = 0; i < 2; i++) {
        memcpy(g_bgfx.hmd.eye[i].rotation, hmd->eye[i].rotation, sizeof(float) * 4);
        memcpy(g_bgfx.hmd.eye[i].translation, hmd->eye[i].translation, sizeof(float) * 3);
        memcpy(g_bgfx.hmd.eye[i].fov, hmd->eye[i].fov, sizeof(float) * 4);
        memcpy(g_bgfx.hmd.eye[i].viewOffset, hmd->eye[i].viewOffset, sizeof(float) * 3);
    }
    return g_bgfx.hmd;
}

static RenderFrameType renderFrame()
{
    return (RenderFrameType)bgfx::renderFrame();
}

static void setPlatformData(const GfxPlatformData& data)
{
    bgfx::PlatformData p;
    p.backBuffer = data.backBuffer;
    p.backBufferDS = data.backBufferDS;
    p.context = data.context;
    p.ndt = data.ndt;
    p.nwh = data.nwh;
    bgfx::setPlatformData(p);
}

static const GfxInternalData& getInternalData()
{
    const bgfx::InternalData* d = bgfx::getInternalData();
    g_bgfx.internal.caps = &getCaps();
    g_bgfx.internal.context = d->context;
    return g_bgfx.internal;
}

static void overrideInternal(TextureHandle handle, uintptr_t ptr)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::overrideInternal(h, ptr);
}

static void overrideInternal2(TextureHandle handle, uint16_t width, uint16_t height, uint8_t numMips,
    TextureFormat fmt, TextureFlag flags)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::overrideInternal(h, width, height, numMips, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags);
}

static void discard()
{
    bgfx::discard();
}

static uint32_t touch(uint8_t id)
{
    return bgfx::touch(id);
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

static void saveScreenshot(const char* filepath)
{
    bgfx::saveScreenShot(filepath);
}

static void setViewName(uint8_t id, const char* name)
{
    bgfx::setViewName(id, name);
}

static void setViewRect(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    bgfx::setViewRect(id, x, y, width, height);
}

static void setViewRectRatio(uint8_t id, uint16_t x, uint16_t y, BackbufferRatio ratio)
{
    bgfx::setViewRect(id, x, y, (bgfx::BackbufferRatio::Enum)ratio);
}

static void setViewScissor(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    bgfx::setViewScissor(id, x, y, width, height);
}

static void setViewClear(uint8_t id, GfxClearFlag flags, uint32_t rgba, float depth, uint8_t stencil)
{
    bgfx::setViewClear(id, (uint16_t)flags, rgba, depth, stencil);
}
        
static void setViewClearPalette(uint8_t id, GfxClearFlag flags, float depth, uint8_t stencil,
                         uint8_t color0, uint8_t color1, uint8_t color2, uint8_t color3,
                         uint8_t color4, uint8_t color5, uint8_t color6, uint8_t color7)
{
    bgfx::setViewClear(id, (uint16_t)flags, depth, stencil, color0, color1, color2, color3, color4, color5, color6, 
                        color7);
}

static void setViewSeq(uint8_t id, bool enabled)
{
    bgfx::setViewSeq(id, enabled);
}

static void setViewTransform(uint8_t id, const void* view, const void* projLeft, GfxViewFlag flags, const void* projRight)
{
    bgfx::setViewTransform(id, view, projLeft, (uint8_t)flags, projRight);
}

static void setViewRemap(uint8_t id, uint8_t num, const void* remap)
{
    bgfx::setViewRemap(id, num, remap);
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

static void setState(GfxState state, uint32_t rgba)
{
    bgfx::setState((uint64_t)state, rgba);
}

static void setStencil(GfxStencilState frontStencil, GfxStencilState backStencil)
{
    bgfx::setStencil((uint32_t)frontStencil, (uint32_t)backStencil);
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

static void setVertexBuffer(VertexBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
    bgfx::setVertexBuffer(h);
}

static void setVertexBufferI(VertexBufferHandle handle, uint32_t vertexIndex, uint32_t numVertices)
{
    BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
    bgfx::setVertexBuffer(h, vertexIndex, numVertices);
}

static void setDynamicVertexBuffer(DynamicVertexBufferHandle handle, uint32_t startVertex, uint32_t numVertices)
{
    BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
    bgfx::setVertexBuffer(h, startVertex, numVertices);
}

static void setTransientVertexBuffer(const TransientVertexBuffer* tvb)
{
    bgfx::setVertexBuffer((const bgfx::TransientVertexBuffer*)tvb);
}

static void setTransientVertexBufferI(const TransientVertexBuffer* tvb, uint32_t startVertex, uint32_t numVertices)
{
    bgfx::setVertexBuffer((const bgfx::TransientVertexBuffer*)tvb, startVertex, numVertices);
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

static void setTexture(uint8_t stage, UniformHandle sampler, TextureHandle handle, TextureFlag flags)
{
    BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);

    bgfx::setTexture(stage, s, h, (uint32_t)flags);
}

static void setTextureFb(uint8_t stage, UniformHandle sampler, FrameBufferHandle handle, uint8_t attachment,
                         TextureFlag flags)
{
    BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
    BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);

    bgfx::setTexture(stage, s, h, attachment, (uint32_t)flags);
}

static uint32_t submit(uint8_t viewId, ProgramHandle program, int32_t depth, bool preserveState)
{
    BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
    return bgfx::submit(viewId, p, depth, preserveState);
}

static uint32_t submitWithOccQuery(uint8_t viewId, ProgramHandle program, OcclusionQueryHandle occQuery, int32_t depth, bool preserveState)
{
    BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
    BGFX_DECLARE_HANDLE(OcclusionQueryHandle, o, occQuery);
    return bgfx::submit(viewId, p, o, depth, preserveState);
}

static uint32_t submitIndirect(uint8_t viewId, ProgramHandle program, IndirectBufferHandle indirectHandle, uint16_t start,
    uint16_t num, int32_t depth, bool preserveState)
{
    BGFX_DECLARE_HANDLE(ProgramHandle, p, program);
    BGFX_DECLARE_HANDLE(IndirectBufferHandle, i, indirectHandle);
    return bgfx::submit(viewId, p, i, start, num, depth, preserveState);
}

static void setComputeBufferIb(uint8_t stage, IndexBufferHandle handle, GpuAccessFlag access)
{
    BGFX_DECLARE_HANDLE(IndexBufferHandle, h, handle);
    bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
}

static void setComputeBufferVb(uint8_t stage, VertexBufferHandle handle, GpuAccessFlag access)
{
    BGFX_DECLARE_HANDLE(VertexBufferHandle, h, handle);
    bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
}

static void setComputeBufferDynamicIb(uint8_t stage, DynamicIndexBufferHandle handle, GpuAccessFlag access)
{
    BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
    bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
}

static void setComputeBufferDynamicVb(uint8_t stage, DynamicVertexBufferHandle handle, GpuAccessFlag access)
{
    BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
    bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
}

static void setComputeBufferIndirect(uint8_t stage, IndirectBufferHandle handle, GpuAccessFlag access)
{
    BGFX_DECLARE_HANDLE(IndirectBufferHandle, h, handle);
    bgfx::setBuffer(stage, h, (bgfx::Access::Enum)access);
}

static void setComputeImage(uint8_t stage, UniformHandle sampler, TextureHandle handle, uint8_t mip, GpuAccessFlag access,
    TextureFormat fmt)
{
    BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::setImage(stage, s, h, mip, (bgfx::Access::Enum)access, (bgfx::TextureFormat::Enum)fmt);
}

static void setComputeImageFb(uint8_t stage, UniformHandle sampler, FrameBufferHandle handle, uint8_t attachment,
                GpuAccessFlag access, TextureFormat fmt)
{
    BGFX_DECLARE_HANDLE(UniformHandle, s, sampler);
    BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
    bgfx::setImage(stage, s, h, attachment, (bgfx::Access::Enum)access, (bgfx::TextureFormat::Enum)fmt);
}

static uint32_t computeDispatch(uint8_t viewId, ProgramHandle handle, uint16_t numX, uint16_t numY, uint16_t numZ,
                    GfxSubmitFlag flags)
{
    BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
    return bgfx::dispatch(viewId, h, numX, numY, numZ, (uint8_t)flags);
}

static uint32_t computeDispatchIndirect(uint8_t viewId, ProgramHandle handle, IndirectBufferHandle indirectHandle,
                    uint16_t start, uint16_t num, GfxSubmitFlag flags)
{
    BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
    BGFX_DECLARE_HANDLE(IndirectBufferHandle, i, indirectHandle);
    return bgfx::dispatch(viewId, h, i, start, num, (uint8_t)flags);
}

static void blitToDefault(uint8_t viewId, TextureHandle dest, uint16_t destX, uint16_t destY, TextureHandle src,
            uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height)
{
    BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
    BGFX_DECLARE_HANDLE(TextureHandle, s, src);
    bgfx::blit(viewId, d, destX, destY, s, srcX, srcY, width, height);
}

static void blitToTextureFb(uint8_t viewId, TextureHandle dest, uint16_t destX, uint16_t destY, FrameBufferHandle src,
            uint8_t attachment, uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height)
{
    BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
    BGFX_DECLARE_HANDLE(FrameBufferHandle, s, src);
    bgfx::blit(viewId, d, destX, destY, s, attachment, srcX, srcY, width, height);
}

static void blitToTextureT(uint8_t viewId, TextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
            uint16_t destZ, TextureHandle src, uint8_t srcMip, uint16_t srcX, uint16_t srcY,
            uint16_t srcZ, uint16_t width, uint16_t height, uint16_t depth)
{
    BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
    BGFX_DECLARE_HANDLE(TextureHandle, s, src);
    bgfx::blit(viewId, d, destMip, destX, destY, destZ, s, srcMip, srcX, srcY, srcZ, width, height, depth);
}

static void blitToTextureFbMRT(uint8_t viewId, TextureHandle dest, uint8_t destMip, uint16_t destX, uint16_t destY,
            uint16_t destZ, FrameBufferHandle src, uint8_t attachment, uint8_t srcMip, uint16_t srcX,
            uint16_t srcY, uint16_t srcZ, uint16_t width, uint16_t height, uint16_t depth)
{
    BGFX_DECLARE_HANDLE(TextureHandle, d, dest);
    BGFX_DECLARE_HANDLE(FrameBufferHandle, s, src);
    bgfx::blit(viewId, d, destMip, destX, destY, destZ, s, attachment, srcMip, srcX, srcY, srcZ, width, height, depth);
}

static const GfxMemory* allocMem(uint32_t size)
{
    return (const GfxMemory*)bgfx::alloc(size);
}

static const GfxMemory* copy(const void* data, uint32_t size)
{
    return (const GfxMemory*)bgfx::copy(data, size);
}

static const GfxMemory* makeRef(const void* data, uint32_t size, gfxReleaseMemCallback releaseFn, void* userData)
{
    return (const GfxMemory*)bgfx::makeRef(data, size, releaseFn, userData);
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
    bgfx::destroyShader(h);
}

static void destroyUniform(UniformHandle handle)
{
    BGFX_DECLARE_HANDLE(UniformHandle, h, handle);
    bgfx::destroyUniform(h);
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
    BGFX_DECLARE_HANDLE(ProgramHandle, h, handle);
    bgfx::destroyProgram(h);
}

static UniformHandle createUniform(const char* name, UniformType type, uint16_t num)
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

static VertexBufferHandle createVertexBuffer(const GfxMemory* mem, const VertexDecl& decl, GpuBufferFlag flags)
{
    VertexBufferHandle handle;
    handle.value = bgfx::createVertexBuffer((const bgfx::Memory*)mem, (const bgfx::VertexDecl&)decl, (uint16_t)flags).idx;
    return handle;
}

static DynamicVertexBufferHandle createDynamicVertexBuffer(uint32_t numVertices, const VertexDecl& decl, GpuBufferFlag flags)
{
    DynamicVertexBufferHandle handle;
    handle.value = bgfx::createDynamicVertexBuffer(numVertices, (const bgfx::VertexDecl&)decl, (uint16_t)flags).idx;
    return handle;
}

static DynamicVertexBufferHandle createDynamicVertexBufferMem(const GfxMemory* mem, const VertexDecl& decl,
                                                           GpuBufferFlag flags = GpuBufferFlag::None)
{
    DynamicVertexBufferHandle handle;
    handle.value = bgfx::createDynamicVertexBuffer((const bgfx::Memory*)mem, (const bgfx::VertexDecl&)decl, (uint16_t)flags).idx;
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
    bgfx::destroyVertexBuffer(h);
}

static void destroyDynamicVertexBuffer(DynamicVertexBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(DynamicVertexBufferHandle, h, handle);
    bgfx::destroyDynamicVertexBuffer(h);
}

static bool checkAvailTransientVertexBuffer(uint32_t num, const VertexDecl& decl)
{
    return bgfx::checkAvailTransientVertexBuffer(num, (const bgfx::VertexDecl&)decl);
}

static void allocTransientVertexBuffer(TransientVertexBuffer* tvb, uint32_t num, const VertexDecl& decl)
{
    bgfx::allocTransientVertexBuffer((bgfx::TransientVertexBuffer*)tvb, num, (const bgfx::VertexDecl&)decl);
}

static IndexBufferHandle createIndexBuffer(const GfxMemory* mem, GpuBufferFlag flags)
{
    IndexBufferHandle handle;
    handle.value = bgfx::createIndexBuffer((const bgfx::Memory*)mem, (uint16_t)flags).idx;
    return handle;
}

static DynamicIndexBufferHandle createDynamicIndexBuffer(uint32_t num, GpuBufferFlag flags)
{
    DynamicIndexBufferHandle handle;
    handle.value = bgfx::createDynamicIndexBuffer(num, (uint16_t)flags).idx;
    return handle;
}

static DynamicIndexBufferHandle createDynamicIndexBufferMem(const GfxMemory* mem, GpuBufferFlag flags)
{
    DynamicIndexBufferHandle handle;
    handle.value = bgfx::createDynamicIndexBuffer((const bgfx::Memory*)mem, (uint16_t)flags).idx;
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
    bgfx::destroyIndexBuffer(h);
}

static void destroyDynamicIndexBuffer(DynamicIndexBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(DynamicIndexBufferHandle, h, handle);
    bgfx::destroyDynamicIndexBuffer(h);
}

static bool checkAvailTransientIndexBuffer(uint32_t num)
{
    return bgfx::checkAvailTransientIndexBuffer(num);
}

static void allocTransientIndexBuffer(TransientIndexBuffer* tib, uint32_t num)
{
    bgfx::allocTransientIndexBuffer((bgfx::TransientIndexBuffer*)tib, num);
}

static void calcTextureSize(TextureInfo* info, uint16_t width, uint16_t height, uint16_t depth, bool cubemap,
                            uint8_t numMips, TextureFormat fmt)
{
    bgfx::calcTextureSize((bgfx::TextureInfo&)(*info), width, height, depth, cubemap, numMips, 
                            (bgfx::TextureFormat::Enum)fmt);
}

static TextureHandle createTexture(const GfxMemory* mem, TextureFlag flags, uint8_t skipMips, TextureInfo* info)
{
    TextureHandle r;
    r.value = bgfx::createTexture((const bgfx::Memory*)mem, (uint32_t)flags, skipMips, (bgfx::TextureInfo*)info).idx;
    return r;
}

static TextureHandle createTexture2D(uint16_t width, uint16_t height, uint8_t numMips, TextureFormat fmt,
                                     TextureFlag flags, const GfxMemory* mem)
{
    TextureHandle r;
    r.value = bgfx::createTexture2D(width, height, numMips, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags,
                                    (const bgfx::Memory*)mem).idx;
    return r;
}

static TextureHandle createTexture2DRatio(BackbufferRatio ratio, uint8_t numMips, TextureFormat fmt,
                                          TextureFlag flags)
{
    TextureHandle r;
    r.value = bgfx::createTexture2D((bgfx::BackbufferRatio::Enum)ratio, numMips, (bgfx::TextureFormat::Enum)fmt,
                                    (uint32_t)flags).idx;
    return r;
}

static void updateTexture2D(TextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t width,
                            uint16_t height, const GfxMemory* mem, uint16_t pitch)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::updateTexture2D(h, mip, x, y, width, height, (const bgfx::Memory*)mem, pitch);
}

static TextureHandle createTexture3D(uint16_t width, uint16_t height, uint16_t depth, uint8_t numMips,
                                     TextureFormat fmt, TextureFlag flags, const GfxMemory* mem)
{
    TextureHandle r;
    r.value = bgfx::createTexture3D(width, height, depth, numMips, (bgfx::TextureFormat::Enum)fmt,
                                    (uint32_t)flags, (const bgfx::Memory*)mem).idx;
    return r;
}

static void updateTexture3D(TextureHandle handle, uint8_t mip, uint16_t x, uint16_t y, uint16_t z,
                            uint16_t width, uint16_t height, uint16_t depth, const GfxMemory* mem)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::updateTexture3D(h, mip, x, y, z, width, height, depth, (const bgfx::Memory*)mem);
}

static TextureHandle createTextureCube(uint16_t size, uint8_t numMips, TextureFormat fmt, TextureFlag flags,
                                       const GfxMemory* mem)
{
    TextureHandle r;
    r.value = bgfx::createTextureCube(size, numMips, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags, 
                                      (const bgfx::Memory*)mem).idx;
    return r;
}

static void updateTextureCube(TextureHandle handle, CubeSide side, uint8_t mip, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height, const GfxMemory* mem, uint16_t pitch)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::updateTextureCube(h, (uint8_t)side, mip, x, y, width, height, (const bgfx::Memory*)mem, pitch);
}

static void readTexture(TextureHandle handle, void* data)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::readTexture(h, data);
}

static void readFrameBuffer(FrameBufferHandle handle, uint8_t attachment, void* data)
{
    BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
    bgfx::readTexture(h, attachment, data);
}

static void destroyTexture(TextureHandle handle)
{
    BGFX_DECLARE_HANDLE(TextureHandle, h, handle);
    bgfx::destroyTexture(h);
}

static FrameBufferHandle createFrameBuffer(uint16_t width, uint16_t height, TextureFormat fmt, TextureFlag flags)
{
    FrameBufferHandle r;
    r.value = bgfx::createFrameBuffer(width, height, (bgfx::TextureFormat::Enum)fmt, (uint32_t)flags).idx;
    return r;
}

static FrameBufferHandle createFrameBufferRatio(BackbufferRatio ratio, TextureFormat fmt, TextureFlag flags)
{
    FrameBufferHandle r;
    r.value = bgfx::createFrameBuffer((bgfx::BackbufferRatio::Enum)ratio, (bgfx::TextureFormat::Enum)fmt, 
                                    (uint32_t)flags).idx;
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

static FrameBufferHandle createFrameBufferNative(void* nwh, uint16_t width, uint16_t height, TextureFormat depthFmt)
{
    FrameBufferHandle r;
    r.value = bgfx::createFrameBuffer(nwh, width, height, (bgfx::TextureFormat::Enum)depthFmt).idx;
    return r;
}

static void destroyFrameBuffer(FrameBufferHandle handle)
{
    BGFX_DECLARE_HANDLE(FrameBufferHandle, h, handle);
    bgfx::destroyFrameBuffer(h);
}

static bool checkAvailInstanceDataBuffer(uint32_t num, uint16_t stride)
{
    return bgfx::checkAvailInstanceDataBuffer(num, stride);
}

static const InstanceDataBuffer* allocInstanceDataBuffer(uint32_t num, uint16_t stride)
{
    return (const InstanceDataBuffer*)bgfx::allocInstanceDataBuffer(num, stride);
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
    bgfx::destroyIndirectBuffer(h);
}

static OcclusionQueryHandle createOccQuery()
{
    OcclusionQueryHandle r;
    r.value = bgfx::createOcclusionQuery().idx;
    return r;
}

static OcclusionQueryResult getResult(OcclusionQueryHandle handle)
{
    BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
    return (OcclusionQueryResult)bgfx::getResult(h);
}

static void destroyOccQuery(OcclusionQueryHandle handle)
{
    BGFX_DECLARE_HANDLE(OcclusionQueryHandle, h, handle);
    bgfx::destroyOcclusionQuery(h);
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

#ifdef termite_SHARED_LIB

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
    static GfxDriverApi api;
    memset(&api, 0x00, sizeof(api));
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
    api.saveScreenshot = saveScreenshot;
    api.setViewName = setViewName;
    api.setViewRect = setViewRect;
    api.setViewRectRatio = setViewRectRatio;
    api.setViewScissor = setViewScissor;
    api.setViewClear = setViewClear;
    api.setViewClearPalette = setViewClearPalette;
    api.setViewSeq = setViewSeq;
    api.setViewTransform = setViewTransform;
    api.setViewRemap = setViewRemap;
    api.setViewFrameBuffer = setViewFrameBuffer;
    api.setMarker = setMarker;
    api.setState = setState;
    api.setStencil = setStencil;
    api.setScissor = setScissor;
    api.setScissorCache = setScissorCache;
    api.allocTransform = allocTransform;
    api.setTransform = setTransform;
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
    api.setTextureFb = setTextureFb;
    api.submit = submit;
    api.submitWithOccQuery = submitWithOccQuery;
    api.submitIndirect = submitIndirect;
    api.setComputeBufferIb = setComputeBufferIb;
    api.setComputeBufferVb = setComputeBufferVb;
    api.setComputeBufferDynamicVb = setComputeBufferDynamicVb;
    api.setComputeBufferDynamicIb = setComputeBufferDynamicIb;
    api.setComputeBufferIndirect = setComputeBufferIndirect;
    api.setComputeImage = setComputeImage;
    api.setComputeImageFb = setComputeImageFb;
    api.computeDispatch = computeDispatch;
    api.computeDispatchIndirect = computeDispatchIndirect;
    api.blitToDefault = blitToDefault;
    api.blitToTextureFb = blitToTextureFb;
    api.blitToTextureT = blitToTextureT;
    api.blitToTextureFbMRT = blitToTextureFbMRT;
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
    api.checkAvailTransientVertexBuffer = checkAvailTransientVertexBuffer;
    api.checkAvailTransientIndexBuffer = checkAvailTransientIndexBuffer;
    api.allocTransientVertexBuffer = allocTransientVertexBuffer;
    api.allocTransientIndexBuffer = allocTransientIndexBuffer;
    api.createIndexBuffer = createIndexBuffer;
    api.createDynamicIndexBuffer = createDynamicIndexBuffer;
    api.createDynamicVertexBufferMem = createDynamicVertexBufferMem;
    api.updateDynamicIndexBuffer = updateDynamicIndexBuffer;
    api.destroyIndexBuffer = destroyIndexBuffer;
    api.destroyDynamicIndexBuffer = destroyDynamicIndexBuffer;
    api.destroyDynamicVertexBuffer = destroyDynamicVertexBuffer;
    api.calcTextureSize = calcTextureSize;
    api.createTexture = createTexture;
    api.createTexture2D = createTexture2D;
    api.createTexture2DRatio = createTexture2DRatio;
    api.updateTexture2D = updateTexture2D;
    api.createTexture3D = createTexture3D;
    api.updateTexture3D = updateTexture3D;
    api.createTextureCube = createTextureCube;
    api.updateTextureCube = updateTextureCube;
    api.readTexture = readTexture;
    api.readFrameBuffer = readFrameBuffer;
    api.destroyTexture = destroyTexture;
    api.createFrameBuffer = createFrameBuffer;
    api.createFrameBufferRatio = createFrameBufferRatio;
    api.createFrameBufferMRT = createFrameBufferMRT;
    api.createFrameBufferNative = createFrameBufferNative;
    api.createFrameBufferAttachment = createFrameBufferAttachment;
    api.destroyFrameBuffer = destroyFrameBuffer;
    api.checkAvailInstanceDataBuffer = checkAvailInstanceDataBuffer;
    api.allocInstanceDataBuffer = allocInstanceDataBuffer;
    api.createIndirectBuffer = createIndirectBuffer;
    api.destroyIndirectBuffer = destroyIndirectBuffer;
    api.createOccQuery = createOccQuery;
    api.getResult = getResult;
    api.destroyOccQuery = destroyOccQuery;
    api.dbgTextClear = dbgTextClear;
    api.dbgTextPrintf = dbgTextPrintf;
    api.dbgTextImage = dbgTextImage;

    return &api;
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
