#pragma once

#include <cstdarg>
#include "bx/bx.h"
#include "core.h"

namespace st
{
    ST_HANDLE(gfxTextureHandle);
    ST_HANDLE(gfxFrameBufferHandle);

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
    ST_DEFINE_FLAG_TYPE(gfxResetFlag);

    enum class gfxDebugFlag : uint32_t
    {
        None = 0x00000000,
        Wireframe = 0x00000001,
        IFH = 0x00000002,
        Stats = 0x00000004,
        Text = 0x00000008
    };
    ST_DEFINE_FLAG_TYPE(gfxDebugFlag);

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
    ST_DEFINE_FLAG_TYPE(gfxCapsFlag);

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
    ST_DEFINE_FLAG_TYPE(gfxTextureFlag);

    enum class gfxBackbufferRatio
    {
        Equal,
        Half,
        Quarter,
        Eighth,
        Sixteenth,
        Double,

        Count
    };

    enum class gfxViewFlag : uint8_t
    {
        None = 0x00,
        Stereo = 0x01
    };
    ST_DEFINE_FLAG_TYPE(gfxViewFlag);

    enum class gfxState : uint64_t
    {
        RGBWrite = 0x0000000000000001,
        AlphaWrite = 0x0000000000000002,
        DepthWrite = 0x0000000000000004,
        DepthTestLess = 0x0000000000000010,
        DepthTestLequal = 0x0000000000000020,
        DepthTestEqual = 0x0000000000000030,
        DepthTestGequal = 0x0000000000000040,
        DepthTestGreater = 0x0000000000000050,
        DepthTestNotEqual = 0x0000000000000060,
        DepthTestNever = 0x0000000000000070,
        DepthTestAlways = 0x0000000000000080,
        BlendZero = 0x0000000000001000,
        BlendOne = 0x0000000000002000,
        BlendSrcColor = 0x0000000000003000,
        BlendInvSrcColor = 0x0000000000004000,
        BlendSrcAlpha = 0x0000000000005000,
        BlendInvSrcAlpha = 0x0000000000006000,
        BlendDestAlpha = 0x0000000000007000,
        BlendInvDestAlpha = 0x0000000000008000,
        BlendDestColor = 0x0000000000009000,
        BlendInvDestColor = 0x000000000000a000,
        BlendSrcAlphaSat = 0x000000000000b000,
        BlendFactor = 0x000000000000c000,
        BlendInvFactor = 0x000000000000d000,
        BlendEqAdd = 0x0000000000000000,
        BlendEqSub = 0x0000000010000000,
        BlendEqRevSub = 0x0000000020000000,
        BlendEqMin = 0x0000000030000000,
        BlendEqMax = 0x0000000040000000,
        BlendIndependent = 0x0000000400000000,
        CullCW = 0x0000001000000000,
        CullCCW = 0x0000002000000000,
        PrimitiveTriStrip = 0x0001000000000000,
        PrimitiveLines = 0x0002000000000000,
        PrimitiveLineStrip = 0x0003000000000000,
        PrimitivePoints = 0x0004000000000000,
        MSAA = 0x1000000000000000,
        None = 0x0000000000000000,
        Mask = 0xffffffffffffffff,
    };
    ST_DEFINE_FLAG_TYPE(gfxState);

    inline uint64_t gfxStateDefault()
    {
        return gfxState::RGBWrite | gfxState::AlphaWrite | gfxState::DepthTestLess | gfxState::DepthWrite |
            gfxState::CullCW | gfxState::MSAA;
    }

    inline uint64_t gfxStateAlphaRef(uint8_t _ref)
    {
        return (((uint64_t)(_ref) << 40) & 0x0000ff0000000000);
    }

    inline uint64_t gfxStateBlendFuncSeparate(uint64_t srcRGB, uint64_t dstRGB, uint64_t srcA, uint64_t dstA)
    {
        return 0ULL | (srcRGB | (dstRGB << 4)) | ((srcA | (dstA << 4)) << 8);
    }

    inline uint64_t gfxStateBlendEqSeparate(uint64_t rgb, uint64_t a)
    {
        return rgb | (a << 3);
    }

    inline uint64_t gfxStateBlendFunc(uint64_t src, uint64_t dst)
    {
        return gfxStateBlendFuncSeparate(src, dst, src, dst);
    }

    inline uint64_t gfxStateBlendEq(uint64_t eq)
    {
        return gfxStateBlendEqSeparate(eq, eq);
    }

    inline uint64_t gfxStateBlendAdd()
    {
        return gfxStateBlendFunc(gfxState::BlendOne, gfxState::BlendOne);
    }

    inline uint64_t gfxStateBlendAlpha()
    {
        return gfxStateBlendFunc(gfxState::BlendSrcAlpha, gfxState::BlendInvSrcAlpha);
    }

    inline uint64_t gfxStateBlendDarken()
    {
        return gfxStateBlendFunc(gfxState::BlendOne, gfxState::BlendOne) | gfxStateBlendEq(gfxState::BlendEqMin);
    }

    inline uint64_t gfxStateBlendLighten()
    {
        return gfxStateBlendFunc(gfxState::BlendOne, gfxState::BlendOne) | gfxStateBlendEq(gfxState::BlendEqMin);
    }

    inline uint64_t gfxStateBlendMultiply()
    {
        return gfxStateBlendFunc(gfxState::BlendDestColor, gfxState::BlendZero);
    }

    inline uint64_t gfxStateBlendNormal()
    {
        return gfxStateBlendFunc(gfxState::BlendOne, gfxState::BlendInvSrcAlpha);
    }

    inline uint64_t gfxStateBlendScreen()
    {
        return gfxStateBlendFunc(gfxState::BlendOne, gfxState::BlendInvSrcColor);
    }

    inline uint64_t gfxStateBlendLinearBurn()
    {
        return gfxStateBlendFunc(gfxState::BlendDestColor, gfxState::BlendInvDestColor) | gfxStateBlendEq(gfxState::BlendEqSub);
    }

    enum class gfxStencilTest : uint32_t
    {
        Less = 0x00010000,
        Lequal = 0x00020000,
        Equal = 0x00030000,
        Gequal = 0x00040000,
        Greater = 0x00050000,
        NotEqual = 0x00060000,
        Never = 0x00070000,
        Always = 0x00080000
    };
    ST_DEFINE_FLAG_TYPE(gfxStencilTest);

    enum class gfxStencilOp : uint32_t
    {
        FailStencilZero = 0x00000000,
        FailStencilKeep = 0x00100000,
        FailStencilReplace = 0x00200000,
        FailStencilIncr = 0x00300000,
        FailStencilIncrSat = 0x00400000,
        FailStencilDecr = 0x00500000,
        FailStencilDecrSat = 0x00600000,
        FailStencilInvert = 0x00700000,
        FailDepthZero = 0x00000000,
        FailDepthKeep = 0x01000000,
        FailDepthReplace = 0x02000000,
        FailDepthIncr = 0x03000000,
        FailDepthIncrSat = 0x04000000,
        FailDepthDecr = 0x05000000,
        FailDepthDecrSat = 0x06000000,
        FailDepthInvert = 0x07000000,
        PassDepthZero = 0x00000000,
        PassDepthKeep = 0x10000000,
        PassDepthReplace = 0x20000000,
        PassDepthIncr = 0x30000000,
        PassDepthIncrSat = 0x40000000,
        PassDepthDecr = 0x50000000,
        PassDepthDecrSat = 0x60000000,
        PassDepthInvert = 0x70000000
    };
    ST_DEFINE_FLAG_TYPE(gfxStencilOp);
    
    inline uint32_t gfxStencilFuncRef(uint8_t _ref)
    {
        return (uint32_t)_ref & 0x000000ff;
    }

    inline uint32_t gfxStencilRMask(uint8_t mask)
    {
        return ((uint32_t)mask << 8) & 0x0000ff00;
    }

    enum class gfxClearFlag : uint16_t
    {
        None = 0x0000,
        Color = 0x0001,
        Depth = 0x0002,
        Stencil = 0x0004,
        Color0 = 0x0008,
        Color1 = 0x0010,
        Color2 = 0x0020,
        Color3 = 0x0040,
        Color4 = 0x0080,
        Color5 = 0x0100,
        Color6 = 0x0200,
        Color7 = 0x0400,
        Depth = 0x0800,
        Stencil = 0x1000
    };
    ST_DEFINE_FLAG_TYPE(gfxClearFlag);
    
    class BX_NO_VTABLE gfxCallbacks
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

    class BX_NO_VTABLE gfxAsyncCallbacks
    {
    public:
        virtual void onInit(bool result) = 0;
        virtual void onRendererType(gfxRendererType type) = 0;
        virtual void onCaps(const gfxCaps& caps) = 0;
        virtual void onStats(const gfxStats& stats) = 0;
        virtual void onHMD(const gfxHMD& hmd) = 0;
        virtual void onRenderFrame(gfxRenderFrameType type) = 0;
        virtual void onInternalData(const gfxInternalData& data) = 0;
        virtual void onTouch(uint8_t id) = 0;
    };

    class BX_NO_VTABLE gfxDriver
    {
    public:
        // Init
        virtual bool init(uint16_t deviceId, gfxCallbacks* callbacks, gfxAsyncCallbacks* asyncCallbacks, 
                          bx::AllocatorI* alloc) = 0;
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

        // Views
        virtual void setViewName(uint8_t id, const char* name) = 0;
        virtual void setViewRect(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height) = 0;
        virtual void setViewRect(uint8_t id, uint16_t x, uint16_t y, gfxBackbufferRatio ratio) = 0;
        virtual void setViewScissor(uint8_t id, uint16_t x, uint16_t y, uint16_t width, uint16_t height) = 0;
        virtual void setViewClear(uint8_t id, uint16_t flags, uint32_t rgba, float depth, uint8_t stencil) = 0;
        virtual void setViewClear(uint8_t id, uint16_t flags, float depth, uint8_t stencil,
                                  uint8_t color0, uint8_t color1, uint8_t color2, uint8_t color3,
                                  uint8_t color4, uint8_t color5, uint8_t color6, uint8_t color7) = 0;
        virtual void setViewSeq(uint8_t id, bool enabled) = 0;
        virtual void setViewTransform(uint8_t id, const void* view, const void* projLeft, uint8_t flags /*gfxViewFlag*/, 
                                      const void* projRight) = 0;
        virtual void setViewRemap(uint8_t id, uint8_t num, const void* remap) = 0;
        virtual void setViewFrameBuffer(uint8_t id, gfxFrameBufferHandle handle) = 0;

        // Draw
        virtual void setMarker(const char* marker) = 0;
        virtual void setState(uint64_t state, uint32_t rgba = 0) = 0;
        virtual void setStencil(uint32_t frontStencil, uint32_t backStencil) = 0;
        virtual void setScissor(uint16_t x, uint16_t y, uint16_t width, uint16_t height) = 0;
        virtual void setScissor(uint16_t cache) = 0;
    };
}   // namespace st

