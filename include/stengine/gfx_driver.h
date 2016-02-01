#pragma once

#include "bx/bx.h"
#include <cstdarg>

namespace st
{
    ST_HANDLE(gfxTextureHandle)

    enum class gfxFatalType
    {
        DebugCheck,
        MinimumRequiredSpecs,
        InvalidShader,
        UnableToInitialize,
        UnableToCreateTexture,
        DeviceLost,

        Count
    };

    enum class gfxTextureFormat
    {
        BC1,    // dxt1
        BC2,    // dxt3
        BC3,    // dxt5
        BC4,    // ATIC1
        BC5,    // ATIC2
        BC6H,
        BC7,
        ETC1,   // ETC1 RGB8
        ETC2,   // ETC2 RGB8
        ETC2A,  // ETC2 RGBA8
        ETC2A1, // ETC2 RGB8A1
        PTC12, // PVRTC1 RGB 2BPP
        PTC14, // PVRTC1 RGB 4BPP
        PTC12A, // PVRTC1 RGBA 2BPP
        PTC14A, // PVRTC1 RGBA 4BPP
        PTC22, // PVRTC2 RGBA 2BPP
        PTC24, // PVRTC2 RGBA 4BPP
        Unknown,
        R1,
        A8,
        R8,
        R8I,
        R8U,
        R8S,
        R16,
        R16I,
        R16U,
        R16F,
        R16S,
        R32I,
        R32U,
        R32F,
        RG8,
        RG8I,
        RG8U,
        RG8S,
        RG16,
        RG16I,
        RG16U,
        RG16F,
        RG16S,
        RG32I,
        RG32U,
        RG32F,
        RGB9E5F,
        BGRA8,
        RGBA8,
        RGBA8I,
        RGBA8U,
        RGBA8S,
        RGBA16,
        RGBA16I,
        RGBA16U,
        RGBA16S,
        RGBA16F,
        RGBA32I,
        RGBA32U,
        RGBA32F,
        R5G6B5,
        RGBA4,
        RGB5A1,
        RGB10A2,
        R11G11B10F,
        UnknownDepth,
        D16,
        D24,
        D24S8,
        D32,
        D16F,
        D32F,
        D0S8,

        Count
    };

    enum class gfxResetFlag : uint32_t
    {
        None = 0,
        Fullscreen = 0x00000001,
        MSAA2X = 0x00000010,
        MSAA4X = 0x00000020,
        MSAA8X = 0x00000030,
        MSAA16X = 0x00000040,
        VSync = 0x00000080,
        MaxAnisotropy = 0x00000100,
        Capture = 0x00000200,
        HMD = 0x00000400,
        HMD_Debug = 0x00000800,
        HMD_Recenter = 0x00001000,
        FlushAfterRender = 0x00002000,
        FlipAfterRender = 0x00004000,
        SRGB_BackBuffer = 0x00008000,
        HiPi = 0x00010000,
        DepthClamp = 0x00020000
    };

    enum class gfxDebugFlag : uint32_t
    {
        None = 0,
        Wireframe = 0x00000001,
        IFH = 0x00000002,
        Stats = 0x00000004,
        Text = 0x00000008
    };

    enum class gfxRendererType : uint32_t
    {
        Null,
        Direct3D9,
        Direct3D11,
        Direct3D12,
        Metal,
        OpenGLES,
        OpenGL,
        Vulkan,

        Count
    };

    struct gfxGPU
    {
        uint16_t deviceId;
        uint16_t vendorId;
    };

    enum class gfxCapsFlag : uint64_t
    {
        TextureCompareLequal = 0x0000000000000001,
        TextureCompareAll = 0x0000000000000003,
        Texture3D = 0x0000000000000004,
        VertexAttribHalf = 0x0000000000000008,
        VertexAttribUint8 = 0x0000000000000010,
        Instancing = 0x0000000000000020,
        Multithreaded = 0x0000000000000040,
        FragmentDepth = 0x0000000000000080,
        BlendIndependent = 0x0000000000000100,
        Compute = 0x0000000000000200,
        FragmentOrdering = 0x0000000000000400,
        SwapChain = 0x0000000000000800,
        HMD = 0x0000000000001000,
        Index32 = 0x0000000000002000,
        DrawIndirect = 0x0000000000004000,
        HiPi = 0x0000000000008000,
        TextureBlit = 0x0000000000010000,
        TextureReadBack = 0x0000000000020000,
        OcclusionQuery = 0x0000000000040000,

        TextureFormatNone
    };

    struct gfxCaps
    {
        gfxRendererType type;
        uint64_t supported; // gfxCapsFlag
        uint32_t maxDrawCalls;
        uint16_t maxTextureSize;
        uint16_t maxViews;
        uint8_t maxFBAttachments;
        uint8_t numGPUs;
        uint16_t vendorId;
        uint16_t deviceId;
        uint16_t formats[gfxTextureFormat::Count];
        gfxGPU gpu[4];
    };

    struct gfxStats
    {
        uint64_t cpuTimeBegin;
        uint64_t cpuTimeEnd;
        uint64_t cpuTimerFreq;
        uint64_t gpuTimeBegin;
        uint64_t gpuTimeEnd;
        uint64_t gpuTimerFreq;
    };

    struct gfxEye
    {
        float rotation[4];
        float translation[3];
        float fov[4];
        float viewOffset[3];
    };

    struct gfxHMD
    {
        uint16_t width;
        uint16_t height;
        uint32_t deviceWidth;
        uint32_t deviceHeight;
        uint8_t flags;
        gfxEye eye;
    };

    enum class gfxRenderFrameType
    {
        NoContext,
        Render,
        Exiting,

        Count
    };

    struct gfxPlatformData
    {
        void* ndt;  // Native display type
        void* nwh;  // Native window handle
        void* context;  // GL context, d3d device
        void* backBuffer;   // GL backbuffer or d3d render target view
        void* backBufferDS; // Backbuffer depth/stencil
    };

    struct gfxInternalData
    {
        const gfxCaps* caps;
        void* context;
    };

    enum class gfxTextureFlag : uint32_t
    {
        None = 0,
        U_Mirror = 0x00000001,
        U_Clamp = 0x00000002,
        U_Border = 0x00000003,
        V_Mirror = 0x00000004,
        V_Clamp = 0x00000008,
        V_Border = 0x0000000c,
        W_Mirror = 0x00000010,
        W_Clamp = 0x00000020,
        W_Border = 0x00000030,
        MinPoint = 0x00000040,
        MinAnisotropic = 0x00000080,
        MagPoint = 0x00000100,
        MagAnisotropic = 0x00000200,
        MipPoint = 0x00000400,
        RT = 0x00001000,
        RT_MSAA2X = 0x00002000,
        RT_MSAA4X = 0x00003000,
        RT_MSAA8X = 0x00004000,
        RT_MSAA16X = 0x00005000,
        RT_WriteOnly = 0x00008000,
        CompareLess = 0x00010000,
        CompareLequal = 0x00020000,
        CompareEqual = 0x00030000,
        CompareGequal = 0x00040000,
        CompareGreater = 0x00050000,
        CompareNotEqual = 0x00060000,
        CompareNever = 0x00070000,
        CompareAlways = 0x00080000,
        ComputeWrite = 0x00100000,
        SRGB = 0x00200000,
        BlitDst = 0x00400000,
        ReadBack = 0x00800000,
    };

    class BX_NO_VTABLE gfxCallbacks
    {
    public:
        virtual void fatal(gfxFatalType type, const char* str) = 0;
        virtual void traceVargs(const char* filepath, int line, const char* format, va_list argList) = 0;
        virtual uint32_t cacheReadSize(uint64_t id) = 0;
        virtual bool cacheRead(uint64_t id, void* data, uint32_t size) = 0;
        virtual void cacheWrite(uint64_t id, const void* data, uint32_t size) = 0;
        virtual void screenShot(const char *filePath, uint32_t width, uint32_t height, uint32_t pitch, 
                                const void *data, uint32_t size, bool yflip) = 0;
        virtual void captureBegin(uint32_t width, uint32_t height, uint32_t pitch, gfxTextureFormat fmt, bool yflip) = 0;
        virtual void captureEnd() = 0;
        virtual void captureFrame(const void* data, uint32_t size) = 0;
    };

    class BX_NO_VTABLE gfxDriver
    {
    public:
        // Init
        virtual bool init(uint16_t deviceId, gfxCallbacks* callbacks, bx::AllocatorI* alloc) = 0;
        virtual void shutdown() = 0;

        virtual void reset(uint32_t width, uint32_t height, uint32_t flags) = 0;
        virtual void frame() = 0;
        virtual void setDebug(uint32_t debugFlags) = 0;
        virtual gfxRendererType getRendererType() const = 0;
        virtual const gfxCaps* getCaps() const = 0;
        virtual const gfxStats* getStats() const = 0;
        virtual const gfxHMD* getHMD() const = 0;

        // Platform specific
        virtual gfxRenderFrameType renderFrame() = 0;
        virtual void setPlatformData(const gfxPlatformData& data) = 0;
        virtual const gfxInternalData* getInternalData() const = 0;
        virtual void overrideInternal(gfxTextureHandle handle, uintptr_t ptr) = 0;
        virtual void overrideInternal(gfxTextureHandle handle, uint16_t width, uint16_t height, uint16_t numMips,
                                      gfxTextureFormat fmt, uint32_t flags /*gfxTextureFlag*/) = 0;

        // Misc
        virtual void discard() = 0;
        virtual uint32_t touch(uint8_t id) = 0;
        virtual void setPaletteColor(uint8_t index, uint32_t rgba) = 0;
        virtual void setPaletteColor(uint8_t index, float rgba) = 0;
        virtual void setPaletteColor(uint8_t index, float r, float g, float b, float a) = 0;
        virtual void saveScreenshot(const char* filepath) = 0;
    };
}

