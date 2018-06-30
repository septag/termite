#pragma once

#include "types.h"

namespace tee
{
    struct TextureT {};
    struct FrameBufferT {};
    struct OcclusionQueryT {};
    struct IndexBufferT {};
    struct DynamicIndexBufferT {};
    struct VertexBufferT {};
    struct DynamicVertexBufferT {};
    struct UniformT {};
    struct ProgramT {};
    struct IndirectBufferT {};
    struct ShaderT {};
    struct VertexDeclT {};

    typedef PhantomType<uint16_t, TextureT, UINT16_MAX> TextureHandle;
    typedef PhantomType<uint16_t, FrameBufferT, UINT16_MAX> FrameBufferHandle;
    typedef PhantomType<uint16_t, OcclusionQueryT, UINT16_MAX> OcclusionQueryHandle;
    typedef PhantomType<uint16_t, IndexBufferT, UINT16_MAX> IndexBufferHandle;
    typedef PhantomType<uint16_t, DynamicIndexBufferT, UINT16_MAX> DynamicIndexBufferHandle;
    typedef PhantomType<uint16_t, VertexBufferT, UINT16_MAX> VertexBufferHandle;
    typedef PhantomType<uint16_t, DynamicVertexBufferT, UINT16_MAX> DynamicVertexBufferHandle;
    typedef PhantomType<uint16_t, UniformT, UINT16_MAX> UniformHandle;
    typedef PhantomType<uint16_t, ProgramT, UINT16_MAX> ProgramHandle;
    typedef PhantomType<uint16_t, IndirectBufferT, UINT16_MAX> IndirectBufferHandle;
    typedef PhantomType<uint16_t, ShaderT, UINT16_MAX> ShaderHandle;
    typedef PhantomType<uint16_t, VertexDeclT, UINT16_MAX> VertexDeclHandle;
    
    struct GfxFatalType
    {
        enum Enum
        {
            DebugCheck,
            MinimumRequiredSpecs,
            InvalidShader,
            UnableToInitialize,
            UnableToCreateTexture,
            DeviceLost,

            Count
        };
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
    struct TextureFormat
    {
        enum Enum
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
            RG11B10F,

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
    };

    struct GfxResetFlag
    {
        enum Enum
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
            DepthClamp = 0x00020000,
            Suspend = 0x00040000
        };

        typedef uint32_t Bits;
    };

    struct GfxDebugFlag
    {
        enum Enum
        {
            None = 0x00000000,
            Wireframe = 0x00000001,     // wireframe for all primitives
            IFH = 0x00000002,           // fast hardware test, no draw calls will be submitted, for profiling
            Stats = 0x00000004,         // Stats display
            Text = 0x00000008           // debug text
        };

        typedef uint32_t Bits;
    };
    
    struct RendererType
    {
        enum Enum
        {
            Noop,
            Direct3D9,
            Direct3D11,
            Direct3D12,
            Gnm,
            Metal,
            OpenGLES,
            OpenGL,
            Vulkan,

            Count
        };
    };

    struct GPUDesc
    {
        uint16_t deviceId;
        uint16_t vendorId;
    };

    struct GpuCapsFlag
    {
        enum Enum
        {
            AlphaToCoverage = 0x0000000000000001ull,
            BlendeIndependent = 0x0000000000000002ull,
            Compute = 0x0000000000000004ull,
            ConservativeRaster = 0x0000000000000008ull,
            DrawIndirect = 0x0000000000000010ull,
            FragmentDepth = 0x0000000000000020ull,
            FragmentOrdering = 0x0000000000000040ull,
            GraphicsDebugger = 0x0000000000000080ull,
            HiDPI = 0x0000000000000100ull,
            HMD = 0x0000000000000200ull,
            Index32 = 0x0000000000000400ull,
            Instancing = 0x0000000000000800ull,
            OcclusionQuery = 0x0000000000001000ull,
            MultiThreaded = 0x0000000000002000ull,
            SwapChain = 0x0000000000004000ull,
            Texture2DArray = 0x0000000000008000ull,
            Texture3D = 0x0000000000010000ull,
            TextureBlit = 0x0000000000020000ull,
            TextureCompareAll = 0x00000000000c0000ull,
            TextureCompareLequal = 0x0000000000080000ull,
            TextureCubeArray = 0x0000000000100000ull,
            TextureDirectAccess = 0x0000000000200000ull,
            TextureReadBack = 0x0000000000400000ull,
            VertexAttribHalf = 0x0000000000800000ull,
            VertexAttribUint10 = 0x0000000000800000ull
        };

        typedef uint64_t Bits;
    };

    struct TextureSupportFlag
    {
        enum Enum
        {
            None = 0,
            Texture2D = 0x0001,
            Texture2D_SRGB = 0x0002,
            Texture2D_Emulated = 0x0004,
            Texture3D = 0x0008,
            Texture3D_SRGB = 0x0010,
            Texture3D_Emulated = 0x0020,
            TextureCube = 0x0040,
            TextureCube_SRGB = 0x0080,
            TextureCube_Emulated = 0x0100,
            TextureVertex = 0x0200,
            TextureImage = 0x0400,
            TextureFramebuffer = 0x0800,
            TextureFramebuffer_MSAA = 0x1000,
            TextureMSAA = 0x2000,
            TextureMSAA_MIP_AutoGen = 0x4000
        };

        typedef uint16_t Bits;
    };

    struct GpuTransform
    {
        float* data;    // Pointer to first matrix
        uint16_t num;   // number of matrices
    };

    struct TransientIndexBuffer
    {
        uint8_t* data;
        uint32_t size;
        uint32_t startIndex;
        IndexBufferHandle handle;
    };

    struct TransientVertexBuffer
    {
        uint8_t* data;             // Pointer to data.
        uint32_t size;             // Data size.
        uint32_t startVertex;      // First vertex.
        uint16_t stride;           // Vertex stride.
        VertexBufferHandle handle; // Vertex buffer handle.
        VertexDeclHandle decl;     // Vertex declaration handle.
    };

    struct InstanceDataBuffer
    {
        uint8_t* data;             // Pointer to data.
        uint32_t size;             // Data size.
        uint32_t offset;           // Offset in vertex buffer.
        uint32_t num;              // Number of instances.
        uint16_t stride;           // Vertex buffer stride.
        VertexBufferHandle handle; // Vertex buffer object handle.
    };

    struct GfxCaps
    {
        RendererType::Enum type;
        GpuCapsFlag::Bits supported;

        uint16_t vendorId;
        uint16_t deviceId;
        bool homogeneousDepth;
        bool originBottomLeft;
        uint8_t numGPUs;

        GPUDesc gpu[4];

        struct Limits
        {
            uint32_t maxDrawCalls;            //!< Maximum draw calls.
            uint32_t maxBlits;                //!< Maximum number of blit calls.
            uint32_t maxTextureSize;          //!< Maximum texture size.
            uint32_t maxViews;                //!< Maximum views.
            uint32_t maxFrameBuffers;         //!< Maximum number of frame buffer handles.
            uint32_t maxFBAttachments;        //!< Maximum frame buffer attachments.
            uint32_t maxPrograms;             //!< Maximum number of program handles.
            uint32_t maxShaders;              //!< Maximum number of shader handles.
            uint32_t maxTextures;             //!< Maximum number of texture handles.
            uint32_t maxTextureSamplers;      //!< Maximum number of texture samplers.
            uint32_t maxVertexDecls;          //!< Maximum number of vertex format declarations.
            uint32_t maxVertexStreams;        //!< Maximum number of vertex streams.
            uint32_t maxIndexBuffers;         //!< Maximum number of index buffer handles.
            uint32_t maxVertexBuffers;        //!< Maximum number of vertex buffer handles.
            uint32_t maxDynamicIndexBuffers;  //!< Maximum number of dynamic index buffer handles.
            uint32_t maxDynamicVertexBuffers; //!< Maximum number of dynamic vertex buffer handles.
            uint32_t maxUniforms;             //!< Maximum number of uniform handles.
            uint32_t maxOcclusionQueries;     //!< Maximum number of occlusion query handles.
        };

        Limits limits;

        TextureSupportFlag::Bits formats[TextureFormat::Count];
    };

    struct ViewStats
    {
        char     name[256];      //!< View name.
        uint8_t  view;           //!< View id.
        uint64_t cpuTimeElapsed; //!< CPU (submit) time elapsed.
        uint64_t gpuTimeElapsed; //!< GPU time elapsed.
    };

    struct GfxStats
    {
        int64_t cpuTimeFrame;    //!< CPU time between two `bgfx::frame` calls.
        int64_t cpuTimeBegin;    //!< Render thread CPU submit begin time.
        int64_t cpuTimeEnd;      //!< Render thread CPU submit end time.
        int64_t cpuTimerFreq;    //!< CPU timer frequency.

        int64_t gpuTimeBegin;    //!< GPU frame begin time.
        int64_t gpuTimeEnd;      //!< GPU frame end time.
        int64_t gpuTimerFreq;    //!< GPU timer frequency.

        int64_t waitRender;       //!< Time spent waiting for render backend thread to finish issuing
                                  //!  draw commands to underlying graphics API.
        int64_t waitSubmit;       //!< Time spent waiting for submit thread to advance to next frame.

        uint32_t numDraw;         //!< Number of draw calls submitted.
        uint32_t numCompute;      //!< Number of compute calls submitted.
        uint32_t maxGpuLatency;   //!< GPU driver latency.

        int64_t gpuMemoryMax;     //!< Maximum available GPU memory.
        int64_t gpuMemoryUsed;    //!< Available GPU memory.

        uint16_t width;           //!< Backbuffer width in pixels.
        uint16_t height;          //!< Backbuffer height in pixels.
        uint16_t textWidth;       //!< Debug text width in characters.
        uint16_t textHeight;      //!< Debug text height in characters.

        uint16_t  numViews;       //!< Number of view stats.
        ViewStats viewStats[256]; //!< View stats.

        // Extra
        uint32_t allocTvbSize;  // per-frame
        uint32_t allocTibSize;
        uint32_t maxTvbSize;
        uint32_t maxTibSize;

    };

    struct HMDDesc
    {
        struct Eye
        {
            float rotation[4];
            float translation[3];
            float fov[4];
            float viewOffset[3];
            float projection[16];
            float pixelsPerTanAngle[2];
        };

        Eye eye[2];
        uint16_t width;
        uint16_t height;
        uint32_t deviceWidth;
        uint32_t deviceHeight;
        uint8_t flags;
    };

    struct RenderFrameType
    {
        enum Enum
        {
            NoContext,
            Render,
            Exiting,

            Count
        };
    };

    struct GfxPlatformData
    {
        void* ndt;  // Native display type
        void* nwh;  // Native window handle
        void* context;  // GL context, d3d device
        void* backBuffer;   // GL backbuffer or d3d render target view
        void* backBufferDS; // Backbuffer depth/stencil
        void* session;      //!< ovrSession, for Oculus SDK

        GfxPlatformData()
        {
            ndt = nullptr;
            nwh = nullptr;
            context = nullptr;
            backBuffer = nullptr;
            backBufferDS = nullptr;
            session = nullptr;
        }
    };

    struct GfxInternalData
    {
        const GfxCaps* caps;
        void* context;
    };

    struct TextureFlag
    {
        enum Enum
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

        typedef uint32_t Bits;
    };

    struct BackbufferRatio
    {
        enum Enum
        {
            Equal,
            Half,
            Quarter,
            Eighth,
            Sixteenth,
            Double,

            Count
        };
    };

    struct GfxViewFlag
    {
        enum Enum
        {
            None = 0x00,
            Stereo = 0x01
        };

        typedef uint8_t Bits;
    };

    struct GfxSubmitFlag
    {
        enum Enum
        {
            Left = 0x01,
            Right = 0x02,
            Both = 0x03
        };

        typedef uint8_t Bits;
    };

    struct GfxState
    {
        enum Enum : uint64_t
        {
            RGBWrite = (0x0000000000000001|0x0000000000000002|0x0000000000000004),
            AlphaWrite = 0x0000000000000008,
            DepthWrite = 0x0000004000000000,
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
            MSAA = 0x0100000000000000,
            None = 0x0000000000000000,
            Mask = 0xffffffffffffffff
        };

        typedef uint64_t Bits;
    };
    
    struct GfxStencilState
    {
        enum Enum
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

        typedef uint32_t Bits;
    };

    struct GfxClearFlag
    {
        enum Enum
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

        typedef uint16_t Bits;
    };

    struct GpuAccessFlag
    {
        enum Enum
        {
            Read,
            Write,
            ReadWrite,

            Count
        };
    };

    struct GfxMemory
    {
        uint8_t* data;
        uint32_t size;
    };

    struct UniformType
    {
        enum Enum
        {
            Int1,
            End,
            Vec4,
            Mat3,
            Mat4,

            Count
        };
    };

    struct GfxBufferFlag
    {
        enum Enum
        {
            None = 0x0000,
            ComputeRead = 0x0100,
            ComputeWrite = 0x0200,
            DrawIndirect = 0x0400,
            Resizable = 0x0800,
            Index32 = 0x1000,
            ComputeReadWrite = (ComputeRead|ComputeWrite)
        };

        typedef uint16_t Bits;
    };

    struct VertexAttrib
    {
        enum Enum
        {
            Position,  //!< a_position
            Normal,    //!< a_normal
            Tangent,   //!< a_tangent
            Bitangent, //!< a_bitangent
            Color0,    //!< a_color0
            Color1,    //!< a_color1
            Color2,    //!< a_color2
            Color3,    //!< a_color3
            Indices,   //!< a_indices
            Weight,    //!< a_weight
            TexCoord0, //!< a_texcoord0
            TexCoord1, //!< a_texcoord1
            TexCoord2, //!< a_texcoord2
            TexCoord3, //!< a_texcoord3
            TexCoord4, //!< a_texcoord4
            TexCoord5, //!< a_texcoord5
            TexCoord6, //!< a_texcoord6
            TexCoord7, //!< a_texcoord7

            Count
        };
    };

    struct VertexAttribType
    {
        enum Enum
        {
            Uint8,  // Uint8
            Uint10, // Uint10, availability depends on: `BGFX_CAPS_VERTEX_ATTRIB_UINT10`.
            Int16,  // Int16
            Half,   // Half, availability depends on: `BGFX_CAPS_VERTEX_ATTRIB_HALF`.
            Float,  // Float
            Count
        };
    };

    struct TextureInfo
    {
        TextureFormat::Enum format;    // Texture format.
        uint32_t storageSize;       // Total amount of bytes required to store texture.
        uint16_t width;             // Texture width.
        uint16_t height;            // Texture height.
        uint16_t depth;             // Texture depth.
        uint8_t numMips;            // Number of MIP maps.
        uint8_t bitsPerPixel;       // Format bits per pixel.
        bool    cubeMap;            // Texture is cubemap.
    };

    struct CubeSide
    {
        enum Enum
        {
            XPos = 0,
            XNeg = 1,
            YPos = 2,
            YNeg = 3,
            ZPos = 4,
            ZNeg = 5
        };
    };

    struct OcclusionQueryResult
    {
        enum Enum
        {
            Invisible,
            Visible,
            NoResult,

            Count
        };
    };

    struct ViewMode
    {
        /// View modes:
        enum Enum
        {
            Default,         //!< Default sort order.
            Sequential,      //!< Sort in the same order in which submit calls were called.
            DepthAscending,  //!< Sort draw call depth in ascending order.
            DepthDescending, //!< Sort draw call depth in descending order.

            Count
        };
    };

    struct VertexDecl
    {
        uint32_t hash;
        uint16_t stride;
        uint16_t offset[VertexAttrib::Count];
        uint16_t attribs[VertexAttrib::Count];
    };

    struct GfxAttachment
    {
        TextureHandle handle; //!< Texture handle.
        uint16_t mip;         //!< Mip level.
        uint16_t layer;       //!< Cubemap side or depth layer/slice.
    };

    namespace gfx {
        inline GfxStencilState::Bits stencilFuncRef(uint8_t _ref)
        {
            return (uint32_t)_ref & 0x000000ff;
        }

        inline GfxStencilState::Bits stencilRMask(uint8_t mask)
        {
            return ((uint32_t)mask << 8) & 0x0000ff00;
        }

        inline GfxState::Bits stateDefault()
        {
            return GfxState::RGBWrite | GfxState::AlphaWrite | GfxState::DepthTestLess | GfxState::DepthWrite |
                GfxState::CullCW | GfxState::MSAA;
        }

        inline GfxState::Bits stateAlphaRef(uint8_t _ref)
        {
            return (((uint64_t)(_ref) << 40) & 0x0000ff0000000000);
        }

        inline GfxState::Bits stateBlendFuncSeparate(GfxState::Enum srcRGB, GfxState::Enum dstRGB, GfxState::Enum srcA, GfxState::Enum dstA)
        {
            return (((uint64_t)srcRGB | ((uint64_t)dstRGB << 4)) | (((uint64_t)srcA | ((uint64_t)dstA << 4)) << 8));
        }

        inline GfxState::Bits stateBlendEqSeparate(GfxState::Enum rgb, GfxState::Enum a)
        {
            return ((uint64_t)rgb | ((uint64_t)a << 3));
        }

        inline GfxState::Bits stateBlendFunc(GfxState::Enum src, GfxState::Enum dst)
        {
            return stateBlendFuncSeparate(src, dst, src, dst);
        }

        inline GfxState::Bits stateBlendEq(GfxState::Enum eq)
        {
            return stateBlendEqSeparate(eq, eq);
        }

        inline GfxState::Bits stateBlendAdd()
        {
            return stateBlendFunc(GfxState::BlendOne, GfxState::BlendOne);
        }

        inline GfxState::Bits stateBlendAlpha()
        {
            return stateBlendFunc(GfxState::BlendSrcAlpha, GfxState::BlendInvSrcAlpha);
        }

        inline GfxState::Bits stateBlendDarken()
        {
            return stateBlendFunc(GfxState::BlendOne, GfxState::BlendOne) | stateBlendEq(GfxState::BlendEqMin);
        }

        inline GfxState::Bits stateBlendLighten()
        {
            return stateBlendFunc(GfxState::BlendOne, GfxState::BlendOne) | stateBlendEq(GfxState::BlendEqMax);
        }

        inline GfxState::Bits stateBlendMultiply()
        {
            return stateBlendFunc(GfxState::BlendDestColor, GfxState::BlendZero);
        }

        inline GfxState::Bits stateBlendNormal()
        {
            return stateBlendFunc(GfxState::BlendOne, GfxState::BlendInvSrcAlpha);
        }

        inline GfxState::Bits stateBlendScreen()
        {
            return stateBlendFunc(GfxState::BlendOne, GfxState::BlendInvSrcColor);
        }

        inline GfxState::Bits stateBlendLinearBurn()
        {
            return stateBlendFunc(GfxState::BlendDestColor, GfxState::BlendInvDestColor) | stateBlendEq(GfxState::BlendEqSub);
        }

        inline const char* rendererTypeToStr(RendererType::Enum type)
        {
            switch (type) {
            case RendererType::Direct3D9:
                return "Direct3D9";
            case RendererType::Direct3D11:
                return "Direct3D11";
            case RendererType::Direct3D12:
                return "Direct3D12";
            case RendererType::Metal:
                return "Metal";
            case RendererType::OpenGLES:
                return "OpenGLES";
            case RendererType::OpenGL:
                return "OpenGL";
            case RendererType::Vulkan:
                return "Vulcan";
            case RendererType::Noop:
                return "Null";
            }
            return "Unknown";
        }
    }
} // namespace tee

