#include "pch.h"

#include "gfx_driver.h"
#include "gfx_utils.h"
#include "io_driver.h"

#include T_MAKE_SHADER_PATH(shaders_h, blit.vso)
#include T_MAKE_SHADER_PATH(shaders_h, blit.fso)
#include T_MAKE_SHADER_PATH(shaders_h, blur.vso)
#include T_MAKE_SHADER_PATH(shaders_h, blur.fso)
#include T_MAKE_SHADER_PATH(shaders_h, vignette_sepia.vso)
#include T_MAKE_SHADER_PATH(shaders_h, vignette_sepia.fso)

namespace termite
{

    struct VertexFs
    {
        float x, y;
        float tx, ty;

        static VertexDecl Decl;

        static void init()
        {
            vdeclBegin(&Decl);
            vdeclAdd(&Decl, VertexAttrib::Position, 2, VertexAttribType::Float);
            vdeclAdd(&Decl, VertexAttrib::TexCoord0, 2, VertexAttribType::Float);
            vdeclEnd(&Decl);
        }
    };
    VertexDecl VertexFs::Decl;

    struct PostProcessBlur
    {
        bx::AllocatorI* alloc;
        BackbufferRatio::Enum ratio;
        float width;
        float height;
        vec4_t kernel[BLUR_KERNEL_SIZE];
        FrameBufferHandle fbs[2];
        TextureHandle textures[2];
        ProgramHandle prog;
        UniformHandle uBlurKernel;
        UniformHandle uTexture;

        PostProcessBlur(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
            width = 0;
            height = 0;
        }
    };

    struct PostProcessVignetteSepia
    {
        bx::AllocatorI* alloc;
        ProgramHandle prog;
        UniformHandle uTexture;
        UniformHandle uVignetteParams;
        UniformHandle uSepiaParams;

        float width;
        float height;
        float radius;
        float softness;     // Vignette softness, 0 ~ 0.5
        float vignetteIntensity;
        float sepiaIntensity;   // 0 ~ 1
        vec4_t sepiaColor; 
        
        PostProcessVignetteSepia(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
            width = height = 0;
            radius = 0;
            softness = 0.45f;   
        }
    };

    struct GfxUtils
    {
        VertexBufferHandle fsVb;
        IndexBufferHandle fsIb;
        GfxDriverApi* driver;

        ProgramHandle blitProg;
        UniformHandle uTexture;

        GfxUtils() :
            driver(nullptr)
        {
        }
    };
    static GfxUtils* g_gutils = nullptr;

    result_t initGfxUtils(GfxDriverApi* driver)
    {
        if (g_gutils) {
            assert(0);
            return T_ERR_ALREADY_INITIALIZED;
        }

        g_gutils = BX_NEW(getHeapAlloc(), GfxUtils);
        if (!g_gutils)
            return T_ERR_OUTOFMEM;
        g_gutils->driver = driver;

        VertexFs::init();
        static VertexFs fsQuad[] = {
            { -1.0f,  1.0f,  0,    0 },        // top-left
            {  1.0f,  1.0f,  1.0f, 0 },        // top-right
            { -1.0f, -1.0f,  0,    1.0f },     // bottom-left
            {  1.0f, -1.0f,  1.0f, 1.0f }      // bottom-right
        };

        if (driver->getRendererType() == RendererType::OpenGL || driver->getRendererType() == RendererType::OpenGLES) {
            fsQuad[0].ty = 1.0f - fsQuad[0].ty;
            fsQuad[1].ty = 1.0f - fsQuad[1].ty;
            fsQuad[2].ty = 1.0f - fsQuad[2].ty;
            fsQuad[3].ty = 1.0f - fsQuad[3].ty;
        }

        static uint16_t indices[] = {
            0, 1, 2,
            2, 1, 3
        };


        if (!g_gutils->fsVb.isValid()) {
            g_gutils->fsVb = driver->createVertexBuffer(driver->makeRef(fsQuad, sizeof(VertexFs) * 4, nullptr, nullptr),
                                                        VertexFs::Decl, GpuBufferFlag::None);
            if (!g_gutils->fsVb.isValid())
                return T_ERR_FAILED;
        }

        if (!g_gutils->fsIb.isValid()) {
            g_gutils->fsIb = driver->createIndexBuffer(driver->makeRef(indices, sizeof(uint16_t) * 6, nullptr, nullptr),
                                                       GpuBufferFlag::None);
            if (!g_gutils->fsIb.isValid())
                return T_ERR_FAILED;
        }

        g_gutils->blitProg = driver->createProgram(
            driver->createShader(driver->makeRef(blit_vso, sizeof(blit_vso), nullptr, nullptr)),
            driver->createShader(driver->makeRef(blit_fso, sizeof(blit_fso), nullptr, nullptr)),
            true);
        if (!g_gutils->blitProg.isValid())
            return T_ERR_FAILED;

        g_gutils->uTexture = driver->createUniform("u_texture", UniformType::Int1, 1);

        return 0;
    }

    void shutdownGfxUtils()
    {
        if (!g_gutils)
            return;

        if (g_gutils->uTexture.isValid())
            g_gutils->driver->destroyUniform(g_gutils->uTexture);
        if (g_gutils->blitProg.isValid())
            g_gutils->driver->destroyProgram(g_gutils->blitProg);
        if (g_gutils->fsVb.isValid())
            g_gutils->driver->destroyVertexBuffer(g_gutils->fsVb);
        if (g_gutils->fsIb.isValid())
            g_gutils->driver->destroyIndexBuffer(g_gutils->fsIb);
        g_gutils->fsVb.reset();
        g_gutils->fsIb.reset();
        BX_DELETE(getHeapAlloc(), g_gutils);
        g_gutils = nullptr;
    }

    /* references :
    *  http://en.wikipedia.org/wiki/Gaussian_blur
    *  http://en.wikipedia.org/wiki/Normal_distribution
    */
    void calcGaussKernel(vec4_t* kernel, int kernelSize, float stdDev, float intensity)
    {
        assert(kernelSize % 2 == 1);    // should be Odd number

        int hk = kernelSize / 2;
        float sum = 0.0f;
        float stdDevSqr = stdDev*stdDev;
        float k = sqrtf(2.0f*bx::pi*stdDevSqr);

        for (int i = 0; i < kernelSize; i++) {
            float p = float(i - hk);
            float x = p / float(hk);
            float w = expf(-(x*x) / (2.0f*stdDevSqr)) / k;
            sum += w;
            kernel[i] = vec4f(p, p, w, 0.0f);
        }

        // Normalize
        for (int i = 0; i < kernelSize; i++) {
            kernel[i].z /= sum;
            kernel[i].z *= intensity;
        }
    }

    static void releaseMemoryBlockCallback(void* ptr, void* userData)
    {
        releaseMemoryBlock((MemoryBlock*)userData);
    }

    ProgramHandle loadShaderProgram(GfxDriverApi* gfxDriver, IoDriverApi* ioDriver, const char* vsFilepath, 
                                    const char* fsFilepath)
    {
        GfxDriverApi* driver = gfxDriver;
        MemoryBlock* vso = ioDriver->read(vsFilepath, IoPathType::Assets);
        if (!vso) {
            T_ERROR("Opening file '%s' failed", vsFilepath);
            return ProgramHandle();
        }

        MemoryBlock* fso = ioDriver->read(fsFilepath, IoPathType::Assets);
        if (!fso) {
            T_ERROR("Opening file '%s' failed", fsFilepath);
            return ProgramHandle();
        }

        ShaderHandle vs = gfxDriver->createShader(gfxDriver->makeRef(vso->data, vso->size, releaseMemoryBlockCallback, vso));
        ShaderHandle fs = gfxDriver->createShader(gfxDriver->makeRef(fso->data, fso->size, releaseMemoryBlockCallback, fso));

        if (!vs.isValid() || !fs.isValid())
            return ProgramHandle();

        return driver->createProgram(vs, fs, true);
    }

    void blitToBackbuffer(uint8_t viewId, TextureHandle texture)
    {
        assert(texture.isValid());

        GfxDriverApi* driver = g_gutils->driver;

        driver->setViewRectRatio(viewId, 0, 0, BackbufferRatio::Equal);
        driver->setViewFrameBuffer(viewId, FrameBufferHandle());

        driver->setState(GfxState::RGBWrite | GfxState::AlphaWrite, 0);
        driver->setTexture(0, g_gutils->uTexture, texture, TextureFlag::FromTexture);
        drawFullscreenQuad(viewId, g_gutils->blitProg);
    }

    void blitToFramebuffer(uint8_t viewId, FrameBufferHandle framebuffer, BackbufferRatio::Enum ratio, TextureHandle texture)
    {
        assert(framebuffer.isValid());
        assert(texture.isValid());

        GfxDriverApi* driver = g_gutils->driver;

        driver->setViewRectRatio(viewId, 0, 0, ratio);
        driver->setViewFrameBuffer(viewId, framebuffer);

        driver->setState(GfxState::RGBWrite | GfxState::AlphaWrite, 0);
        driver->setTexture(0, g_gutils->uTexture, texture, TextureFlag::FromTexture);
        drawFullscreenQuad(viewId, g_gutils->blitProg);
    }

    void drawFullscreenQuad(uint8_t viewId, ProgramHandle prog)
    {
        assert(g_gutils->fsIb.isValid());
        assert(g_gutils->fsVb.isValid());

        GfxDriverApi* driver = g_gutils->driver;

        driver->setVertexBuffer(g_gutils->fsVb);
        driver->setIndexBuffer(g_gutils->fsIb, 0, 6);
        driver->submit(viewId, prog, 0, false);
    }

    vec2i_t getRelativeDisplaySize(int refWidth, int refHeight, int targetWidth, int targetHeight, DisplayPolicy::Enum policy)
    {
        float w, h;
        float ratio = float(refWidth) / float(refHeight);
        switch (policy) {
        case DisplayPolicy::FitToHeight:
            h = float(targetHeight);
            w = h*ratio;
            break;

        case DisplayPolicy::FitToWidth:
            w = float(targetWidth);
            h = w / ratio;
            break;
        }

        return vec2i(int(w), int(h));
    }

    PostProcessBlur* createBlurPostProcess(bx::AllocatorI* alloc, BackbufferRatio::Enum ratio, float stdDev,
                                           TextureFormat::Enum fmt /*= TextureFormat::RGBA8*/)
    {
        PostProcessBlur* blur = BX_NEW(alloc, PostProcessBlur)(alloc);
        if (!blur)
            return nullptr;

        GfxDriverApi* driver = g_gutils->driver;
        for (int i = 0; i < 2; i++) {
            blur->fbs[i] = driver->createFrameBufferRatio(ratio, fmt,
                                                          TextureFlag::RT |
                                                          TextureFlag::MagPoint | TextureFlag::MinPoint |
                                                          TextureFlag::U_Clamp | TextureFlag::V_Clamp);
            assert(blur->fbs[i].isValid());
            blur->textures[i] = driver->getFrameBufferTexture(blur->fbs[i], 0);
        }

        const Config& conf = getConfig();
        uint16_t width = conf.gfxWidth;
        uint16_t height = conf.gfxHeight;
        switch (ratio) {
        case BackbufferRatio::Half:
            width >>= 1;
            height >>= 1;
            break;

        case BackbufferRatio::Quarter:
            width >>= 2;
            height >>= 2;
            break;

        case BackbufferRatio::Eighth:
            width >>= 3;
            height >>= 3;
            break;

        case BackbufferRatio::Sixteenth:
            width >>= 4;
            height >>= 4;
            break;

        case BackbufferRatio::Double:
            width <<= 1;
            height <<= 1;
            break;

        default:
            break;
        }

        calcGaussKernel(blur->kernel, BLUR_KERNEL_SIZE, stdDev, 1.0f);
        blur->ratio = ratio;
        blur->width = float(width);
        blur->height = float(height);

        blur->prog = driver->createProgram(
            driver->createShader(driver->makeRef(blur_vso, sizeof(blur_vso), nullptr, nullptr)),
            driver->createShader(driver->makeRef(blur_fso, sizeof(blur_fso), nullptr, nullptr)),
            true);

        blur->uTexture = driver->createUniform("u_texture", UniformType::Int1, 1);
        blur->uBlurKernel = driver->createUniform("u_blurKernel", UniformType::Vec4, BLUR_KERNEL_SIZE);
        return blur;
    }

    TextureHandle drawBlurPostProcess(PostProcessBlur* blur, uint8_t* viewId, TextureHandle sourceTexture, float radius)
    {
        uint8_t vid = *viewId;
        GfxDriverApi* driver = g_gutils->driver;

        // Downsample to our first blur frameBuffer
        blitToFramebuffer(vid, blur->fbs[0], blur->ratio, sourceTexture);
        vid++;

        vec4_t kernel[BLUR_KERNEL_SIZE];
        float hRadius = radius / blur->width;
        float vRadius = radius / blur->height;

        // Blur horizontally to 2nd framebuffer
        memcpy(kernel, blur->kernel, sizeof(kernel));
        for (int i = 0; i < BLUR_KERNEL_SIZE; i++) {
            kernel[i].x *= hRadius;
            kernel[i].y = 0;
        }

        driver->setViewRectRatio(vid, 0, 0, blur->ratio);
        driver->setViewFrameBuffer(vid, blur->fbs[1]);
        driver->setState(GfxState::RGBWrite, 0);
        driver->setTexture(0, blur->uTexture, blur->textures[0], TextureFlag::FromTexture);
        driver->setUniform(blur->uBlurKernel, kernel, BLUR_KERNEL_SIZE);
        drawFullscreenQuad(vid, blur->prog);
        vid++;

        // Blur vertically to 1st framebuffer
        memcpy(kernel, blur->kernel, sizeof(kernel));
        for (int i = 0; i < BLUR_KERNEL_SIZE; i++) {
            kernel[i].x = 0;
            kernel[i].y *= vRadius;
        }
        driver->setViewRectRatio(vid, 0, 0, blur->ratio);
        driver->setViewFrameBuffer(vid, blur->fbs[0]);
        driver->setState(GfxState::RGBWrite, 0);
        driver->setTexture(0, blur->uTexture, blur->textures[1], TextureFlag::FromTexture);
        driver->setUniform(blur->uBlurKernel, kernel, BLUR_KERNEL_SIZE);
        drawFullscreenQuad(vid, blur->prog);
        vid++;

        *viewId = vid;
        return blur->textures[0];
    }

    TextureHandle getBlurPostProcessTexture(PostProcessBlur* blur)
    {
        return blur->textures[0];
    }

    void destroyBlurPostProcess(PostProcessBlur* blur)
    {
        assert(blur);

        GfxDriverApi* driver = g_gutils->driver;
        if (blur->fbs[0].isValid())
            driver->destroyFrameBuffer(blur->fbs[0]);
        if (blur->fbs[1].isValid())
            driver->destroyFrameBuffer(blur->fbs[1]);

        if (blur->uTexture.isValid())
            driver->destroyUniform(blur->uTexture);
        if (blur->uBlurKernel.isValid())
            driver->destroyUniform(blur->uBlurKernel);
        if (blur->prog.isValid())
            driver->destroyProgram(blur->prog);

        BX_DELETE(blur->alloc, blur);
    }

    PostProcessVignetteSepia* createVignetteSepiaPostProcess(bx::AllocatorI* alloc, uint16_t width, uint16_t height, 
                                                             float radius, float softness, 
                                                             float vignetteIntensity, float sepiaIntensity, 
                                                             color_t sepiaColor)
    {
        GfxDriverApi* driver = g_gutils->driver;

        PostProcessVignetteSepia* vignette = BX_NEW(alloc, PostProcessVignetteSepia)(alloc);
        vignette->width = float(width);
        vignette->height = float(height);
        vignette->radius = radius;
        vignette->softness = bx::fclamp(softness, 0, 0.5f);
        vignette->sepiaColor = colorToVec4(sepiaColor);
        vignette->vignetteIntensity = vignetteIntensity;
        vignette->sepiaIntensity = sepiaIntensity;

        vignette->prog = driver->createProgram(
            driver->createShader(driver->makeRef(vignette_sepia_vso, sizeof(vignette_sepia_vso), nullptr, nullptr)),
            driver->createShader(driver->makeRef(vignette_sepia_fso, sizeof(vignette_sepia_fso), nullptr, nullptr)),
            true);

        vignette->uTexture = driver->createUniform("u_texture", UniformType::Int1, 1);
        vignette->uVignetteParams = driver->createUniform("u_vignetteParams", UniformType::Vec4, 1);
        vignette->uSepiaParams = driver->createUniform("u_sepiaParams", UniformType::Vec4, 1);

        return vignette;
    }

    TextureHandle drawVignetteSepiaPostProcess(PostProcessVignetteSepia* vignette, uint8_t viewId,
                                               FrameBufferHandle targetFb, TextureHandle sourceTexture,
                                               float intensity)
    {
        GfxDriverApi* driver = g_gutils->driver;

        float w = vignette->width;
        float h = vignette->height;

        vec4_t vigParams = vec4f(vignette->radius, vignette->softness, intensity*vignette->vignetteIntensity, 0);
        vec4_t sepiaParams = vec4f(vignette->sepiaColor.x, vignette->sepiaColor.y, vignette->sepiaColor.z, 
                                   intensity*vignette->sepiaIntensity);

        driver->setViewRect(viewId, 0, 0, uint16_t(w), uint16_t(h));
        driver->setViewFrameBuffer(viewId, targetFb);
        driver->setState(GfxState::RGBWrite, 0);
        driver->setTexture(0, vignette->uTexture, sourceTexture, TextureFlag::FromTexture);
        driver->setUniform(vignette->uVignetteParams, vigParams.f, 1);
        driver->setUniform(vignette->uSepiaParams, sepiaParams.f, 1);
        drawFullscreenQuad(viewId, vignette->prog);
        return driver->getFrameBufferTexture(targetFb, 0);
    }

    void destroyVignetteSepiaPostProcess(PostProcessVignetteSepia* vignette)
    {
        assert(vignette);

        GfxDriverApi* driver = g_gutils->driver;
        if (vignette->uTexture.isValid())
            driver->destroyUniform(vignette->uTexture);
        if (vignette->uVignetteParams.isValid())
            driver->destroyUniform(vignette->uVignetteParams);
        if (vignette->uSepiaParams.isValid())
            driver->destroyUniform(vignette->uSepiaParams);
        if (vignette->prog.isValid())
            driver->destroyProgram(vignette->prog);
        BX_DELETE(vignette->alloc, vignette);
    }
}