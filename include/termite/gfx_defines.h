#pragma once

namespace termite
{
    T_HANDLE(gfxTextureHandle);
    T_HANDLE(gfxFrameBufferHandle);
    T_HANDLE(gfxOccQueryHandle);
    T_HANDLE(gfxIndexBufferHandle);
    T_HANDLE(gfxDynamicIndexBufferHandle);
    T_HANDLE(gfxVertexBufferHandle);
    T_HANDLE(gfxDynamicVertexBufferHandle);
    T_HANDLE(gfxVertexDeclHandle);
    T_HANDLE(gfxUniformHandle);
    T_HANDLE(gfxProgramHandle);
    T_HANDLE(gfxIndirectBufferHandle);
    T_HANDLE(gfxShaderHandle);

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

    // Texture formats
    ///
    /// Notation:
    ///
    ///       RGBA16S
    ///       ^   ^ ^
    ///       |   | +-- [ ]Unorm
    ///       |   |     [F]loat
    ///       |   |     [S]norm
    ///       |   |     [I]nt
    ///       |   |     [U]int
    ///       |   +---- Number of bits per component
    ///       +-------- Components
    ///
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
        RGB8,
        RGB8I,
        RGB8U,
        RGB8S,
        RGB9E5F,
        BGRA8,
        RGBA8,
        RGBA8I,
        RGBA8U,
        RGBA8S,
        RGBA16,
        RGBA16I,
        RGBA16U,
        RGBA16F,
        RGBA16S,
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
        D24F,
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
        HiDPi = 0x00010000,
        DepthClamp = 0x00020000
    };

    enum class gfxDebugFlag : uint32_t
    {
        None = 0x00000000,
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
        HiDPi = 0x0000000000008000,
        TextureBlit = 0x0000000000010000,
        TextureReadBack = 0x0000000000020000,
        OcclusionQuery = 0x0000000000040000,

        TextureFormatNone
    };

    struct gfxTransform
    {
        float* data;    // Pointer to first matrix
        uint16_t num;   // number of matrices
    };

    struct gfxTransientIndexBuffer
    {
        uint8_t* data;
        uint32_t size;
        uint32_t startIndex;
        gfxIndexBufferHandle handle;
    };

    struct gfxTransientVertexBuffer
    {
        uint8_t* data;             // Pointer to data.
        uint32_t size;             // Data size.
        uint32_t startVertex;      // First vertex.
        uint16_t stride;           // Vertex stride.
        gfxVertexBufferHandle handle; // Vertex buffer handle.
        gfxVertexDeclHandle decl;     // Vertex declaration handle.
    };

    struct gfxInstanceDataBuffer
    {
        uint8_t* data;             // Pointer to data.
        uint32_t size;             // Data size.
        uint32_t offset;           // Offset in vertex buffer.
        uint32_t num;              // Number of instances.
        uint16_t stride;           // Vertex buffer stride.
        gfxVertexBufferHandle handle; // Vertex buffer object handle.
    };

    struct gfxCaps
    {
        gfxRendererType type;
        gfxCapsFlag supported;
        uint32_t maxDrawCalls;
        uint16_t maxTextureSize;
        uint16_t maxViews;
        uint8_t maxFBAttachments;
        uint8_t numGPUs;
        uint16_t vendorId;
        uint16_t deviceId;
        uint16_t formats[int(gfxTextureFormat::Count)];
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
        gfxEye eye[2];
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
        None = 0x00000000,
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
        FromTexture = 0xffffffff
    };

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

    enum class gfxSubmitFlag : uint8_t
    {
        Left = 0x01,
        Right = 0x02,
        Both = 0x03
    };

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

    enum class gfxStencil : uint32_t
    {
        None = 0x00000000,
        TestLess = 0x00010000,
        TestLequal = 0x00020000,
        TestEqual = 0x00030000,
        TestGequal = 0x00040000,
        TestGreater = 0x00050000,
        TestNotEqual = 0x00060000,
        TestNever = 0x00070000,
        TestAlways = 0x00080000,
        OpStencilFailZero = 0x00000000,
        OpStencilFailKeep = 0x00100000,
        OpStencilFailReplace = 0x00200000,
        OpStencilFailIncr = 0x00300000,
        OpStencilFailIncrSat = 0x00400000,
        OpStencilFailDecr = 0x00500000,
        OpStencilFailDecrSat = 0x00600000,
        OpStencilFailInvert = 0x00700000,
        OpDepthFailZero = 0x00000000,
        OpDepthFailKeep = 0x01000000,
        OpDepthFailReplace = 0x02000000,
        OpDepthFailIncr = 0x03000000,
        OpDepthFailIncrSat = 0x04000000,
        OpDepthFailDecr = 0x05000000,
        OpDepthFailDecrSat = 0x06000000,
        OpDepthFailInvert = 0x07000000,
        OpDepthPassZero = 0x00000000,
        OpDepthPassKeep = 0x10000000,
        OpDepthPassReplace = 0x20000000,
        OpDepthPassIncr = 0x30000000,
        OpDepthPassIncrSat = 0x40000000,
        OpDepthPassDecr = 0x50000000,
        OpDepthPassDecrSat = 0x60000000,
        OpDepthPassInvert = 0x70000000
    };

    enum class gfxClearFlag : uint16_t
    {
        None = 0x0000,
        Color = 0x0001,
        Depth = 0x0002,
        Stencil = 0x0004,
        DiscardColor0 = 0x0008,
        DiscardColor1 = 0x0010,
        DiscardColor2 = 0x0020,
        DiscardColor3 = 0x0040,
        DiscardColor4 = 0x0080,
        DiscardColor5 = 0x0100,
        DiscardColor6 = 0x0200,
        DiscardColor7 = 0x0400,
        DiscardDepth = 0x0800,
        DiscardStencil = 0x1000
    };

    enum class gfxAccess
    {
        Read,
        Write,
        ReadWrite,

        Count
    };

    struct gfxMemory
    {
        uint8_t* data;
        uint32_t size;
    };

    enum class gfxUniformType
    {
        Int1,
        End,
        Vec4,
        Mat3,
        Mat4,

        Count
    };

    enum class gfxBufferFlag : uint16_t
    {
        None = 0x0000,
        ComputeRead = 0x0100,
        ComputeWrite = 0x0200,
        Resizable = 0x0800,
        Index32 = 0x1000
    };

    enum class gfxAttrib : uint16_t
    {
        Position,  // a_position
        Normal,    // a_normal
        Tangent,   // a_tangent
        Bitangent, // a_bitangent
        Color0,    // a_color0
        Color1,    // a_color1
        Indices,   // a_indices
        Weight,    // a_weight
        TexCoord0, // a_texcoord0
        TexCoord1, // a_texcoord1
        TexCoord2, // a_texcoord2
        TexCoord3, // a_texcoord3
        TexCoord4, // a_texcoord4
        TexCoord5, // a_texcoord5
        TexCoord6, // a_texcoord6
        TexCoord7, // a_texcoord7

        Count
    };

    enum class gfxAttribType
    {
        Uint8,  // Uint8
        Uint10, // Uint10, availability depends on: `BGFX_CAPS_VERTEX_ATTRIB_UINT10`.
        Int16,  // Int16
        Half,   // Half, availability depends on: `BGFX_CAPS_VERTEX_ATTRIB_HALF`.
        Float,  // Float
        Count
    };

    struct gfxTextureInfo
    {
        gfxTextureFormat format;    // Texture format.
        uint32_t storageSize;       // Total amount of bytes required to store texture.
        uint16_t width;             // Texture width.
        uint16_t height;            // Texture height.
        uint16_t depth;             // Texture depth.
        uint8_t numMips;            // Number of MIP maps.
        uint8_t bitsPerPixel;       // Format bits per pixel.
        bool    cubeMap;            // Texture is cubemap.
    };

    enum class gfxCubeSide : uint8_t
    {
        XPos = 0,
        XNeg = 1,
        YPos = 2,
        YNeg = 3,
        ZPos = 4,
        ZNeg = 5
    };

    enum class gfxOccQueryResult
    {
        Invisible,
        Visible,
        NoResult,

        Count
    };

    enum class gfxGuiKeyMap : int
    {
        Tab,       // for tabbing through fields
        LeftArrow, // for text edit
        RightArrow,// for text edit
        UpArrow,   // for text edit
        DownArrow, // for text edit
        PageUp,
        PageDown,
        Home,      // for text edit
        End,       // for text edit
        Delete,    // for text edit
        Backspace, // for text edit
        Enter,     // for text edit
        Escape,    // for text edit
        A,         // for text edit CTRL+A: select all
        C,         // for text edit CTRL+C: copy
        V,         // for text edit CTRL+V: paste
        X,         // for text edit CTRL+X: cut
        Y,         // for text edit CTRL+Y: redo
        Z,         // for text edit CTRL+Z: undo

        Count
    };
} // namespace st

C11_DEFINE_FLAG_TYPE(termite::gfxResetFlag);
C11_DEFINE_FLAG_TYPE(termite::gfxDebugFlag);
C11_DEFINE_FLAG_TYPE(termite::gfxCapsFlag);
C11_DEFINE_FLAG_TYPE(termite::gfxTextureFlag);
C11_DEFINE_FLAG_TYPE(termite::gfxViewFlag);
C11_DEFINE_FLAG_TYPE(termite::gfxSubmitFlag);
C11_DEFINE_FLAG_TYPE(termite::gfxState);
C11_DEFINE_FLAG_TYPE(termite::gfxStencil);
C11_DEFINE_FLAG_TYPE(termite::gfxClearFlag);
C11_DEFINE_FLAG_TYPE(termite::gfxBufferFlag);

namespace termite
{
    inline gfxStencil gfxStencilFuncRef(uint8_t _ref)
    {
        return (gfxStencil)((uint32_t)_ref & 0x000000ff);
    }

    inline gfxStencil gfxStencilRMask(uint8_t mask)
    {
        return (gfxStencil)(((uint32_t)mask << 8) & 0x0000ff00);
    }

    inline gfxState gfxStateDefault()
    {
        return gfxState::RGBWrite | gfxState::AlphaWrite | gfxState::DepthTestLess | gfxState::DepthWrite |
               gfxState::CullCW | gfxState::MSAA;
    }

    inline gfxState gfxStateAlphaRef(uint8_t _ref)
    {
        return (gfxState)(((uint64_t)(_ref) << 40) & 0x0000ff0000000000);
    }

    inline gfxState gfxStateBlendFuncSeparate(gfxState srcRGB, gfxState dstRGB, gfxState srcA, gfxState dstA)
    {
        return (gfxState)(((uint64_t)srcRGB | ((uint64_t)dstRGB << 4)) | (((uint64_t)srcA | ((uint64_t)dstA << 4)) << 8));
    }

    inline gfxState gfxStateBlendEqSeparate(gfxState rgb, gfxState a)
    {
        return (gfxState)((uint64_t)rgb | ((uint64_t)a << 3));
    }

    inline gfxState gfxStateBlendFunc(gfxState src, gfxState dst)
    {
        return gfxStateBlendFuncSeparate(src, dst, src, dst);
    }

    inline gfxState gfxStateBlendEq(gfxState eq)
    {
        return gfxStateBlendEqSeparate(eq, eq);
    }

    inline gfxState gfxStateBlendAdd()
    {
        return gfxStateBlendFunc(gfxState::BlendOne, gfxState::BlendOne);
    }

    inline gfxState gfxStateBlendAlpha()
    {
        return gfxStateBlendFunc(gfxState::BlendSrcAlpha, gfxState::BlendInvSrcAlpha);
    }

    inline gfxState gfxStateBlendDarken()
    {
        return gfxStateBlendFunc(gfxState::BlendOne, gfxState::BlendOne) | gfxStateBlendEq(gfxState::BlendEqMin);
    }

    inline gfxState gfxStateBlendLighten()
    {
        return gfxStateBlendFunc(gfxState::BlendOne, gfxState::BlendOne) | gfxStateBlendEq(gfxState::BlendEqMax);
    }

    inline gfxState gfxStateBlendMultiply()
    {
        return gfxStateBlendFunc(gfxState::BlendDestColor, gfxState::BlendZero);
    }

    inline gfxState gfxStateBlendNormal()
    {
        return gfxStateBlendFunc(gfxState::BlendOne, gfxState::BlendInvSrcAlpha);
    }

    inline gfxState gfxStateBlendScreen()
    {
        return gfxStateBlendFunc(gfxState::BlendOne, gfxState::BlendInvSrcColor);
    }

    inline gfxState gfxStateBlendLinearBurn()
    {
        return gfxStateBlendFunc(gfxState::BlendDestColor, gfxState::BlendInvDestColor) | gfxStateBlendEq(gfxState::BlendEqSub);
    }

    inline const char* gfxRendererTypeToStr(gfxRendererType type)
    {
        switch (type) {
        case gfxRendererType::Direct3D9:
            return "Direct3D9";
        case gfxRendererType::Direct3D11:
            return "Direct3D11";
        case gfxRendererType::Direct3D12:
            return "Direct3D12";
        case gfxRendererType::Metal:
            return "Metal";
        case gfxRendererType::OpenGLES:
            return "OpenGLES";
        case gfxRendererType::OpenGL:
            return "OpenGL";
        case gfxRendererType::Vulkan:
            return "Vulcan";
        case gfxRendererType::Null:
            return "Null";
        }
        return "Unknown";
    }
}