#include "pch.h"

#include "gfx_driver.h"
#include "gfx_utils.h"
#include "io_driver.h"

#include "internal.h"

#include TEE_MAKE_SHADER_PATH(shaders_h, blit.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, blit.fso)
#include TEE_MAKE_SHADER_PATH(shaders_h, blur.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, blur.fso)
#include TEE_MAKE_SHADER_PATH(shaders_h, vignette_sepia.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, vignette_sepia.fso)
#include TEE_MAKE_SHADER_PATH(shaders_h, tint.vso)
#include TEE_MAKE_SHADER_PATH(shaders_h, tint.fso)

namespace tee
{
    struct VertexFs
    {
        float x, y;
        float tx, ty;

        static VertexDecl Decl;

        static void init()
        {
            gfx::beginDecl(&Decl);
            gfx::addAttrib(&Decl, VertexAttrib::Position, 2, VertexAttribType::Float);
            gfx::addAttrib(&Decl, VertexAttrib::TexCoord0, 2, VertexAttribType::Float);
            gfx::endDecl(&Decl);
        }
    };
    VertexDecl VertexFs::Decl;

    struct PostProcessBlur
    {
        bx::AllocatorI* alloc;
        BackbufferRatio::Enum ratio;
        uint16_t width;
        uint16_t height;
        vec4_t kernel[BLUR_KERNEL_SIZE];
        FrameBufferHandle fbs[2];
        TextureHandle textures[2];
        ProgramHandle prog;
        UniformHandle uBlurKernel;
        UniformHandle uTexture;
        TextureFormat::Enum fmt;

        PostProcessBlur(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
            width = 0;
            height = 0;
            fmt = TextureFormat::Unknown;
        }
    };

    struct PostProcessVignetteSepia
    {
        bx::AllocatorI* alloc;
        ProgramHandle prog;
        UniformHandle uTexture;
        UniformHandle uVignetteParams;
        UniformHandle uSepiaParams;
        UniformHandle uVignetteColor;

        uint16_t width;
        uint16_t height;
        float start;    // border = 0.5f
        float end;      // border = 0.5f
        float vignetteIntensity;
        float sepiaIntensity;   // 0 ~ 1
        vec4_t sepiaColor; 
        vec4_t vignetteColor;
        
        PostProcessVignetteSepia(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
            width = height = 0;
            start = 0;
            end = 0.45f;   
            vignetteColor = vec4(0, 0, 0, 0);
        }
    };

    struct PostProcessTint
    {
        bx::AllocatorI* alloc;
        ProgramHandle prog;
        UniformHandle uTexture;
        UniformHandle uTintColor;
        uint16_t width;
        uint16_t height;

        PostProcessTint(bx::AllocatorI* _alloc) : alloc(_alloc)
        {
        }
    };

    struct GfxUtils
    {
        VertexBufferHandle fsVb;
        IndexBufferHandle fsIb;
        GfxDriver* driver;

        ProgramHandle blitProg;
        UniformHandle uTexture;

        GfxUtils() :
            driver(nullptr)
        {
        }
    };
    static GfxUtils* gUtils = nullptr;

    bool gfx::initGfxUtils(GfxDriver* driver)
    {
        if (gUtils) {
            BX_ASSERT(0);
            return false;
        }

        gUtils = BX_NEW(getHeapAlloc(), GfxUtils);
        if (!gUtils)
            return false;
        gUtils->driver = driver;

        VertexFs::init();
        static VertexFs fsQuad[] = {
            { -1.0f,  1.0f,  0,    0 },        // top-left
            {  1.0f,  1.0f,  1.0f, 0 },        // top-right
            { -1.0f, -1.0f,  0,    1.0f },     // bottom-left
            {  1.0f, -1.0f,  1.0f, 1.0f }      // bottom-right
        };
        static bool flipped = false;

        if ((driver->getRendererType() == RendererType::OpenGL || driver->getRendererType() == RendererType::OpenGLES) && !flipped) {
            fsQuad[0].ty = 1.0f - fsQuad[0].ty;
            fsQuad[1].ty = 1.0f - fsQuad[1].ty;
            fsQuad[2].ty = 1.0f - fsQuad[2].ty;
            fsQuad[3].ty = 1.0f - fsQuad[3].ty;
            flipped = true;
        }

        static uint16_t indices[] = {
            0, 1, 2,
            2, 1, 3
        };


        if (!gUtils->fsVb.isValid()) {
            gUtils->fsVb = driver->createVertexBuffer(driver->makeRef(fsQuad, sizeof(VertexFs) * 4, nullptr, nullptr),
                                                        VertexFs::Decl, GfxBufferFlag::None);
            if (!gUtils->fsVb.isValid())
                return false;
        }

        if (!gUtils->fsIb.isValid()) {
            gUtils->fsIb = driver->createIndexBuffer(driver->makeRef(indices, sizeof(uint16_t) * 6, nullptr, nullptr),
                                                       GfxBufferFlag::None);
            if (!gUtils->fsIb.isValid())
                return false;
        }

        gUtils->blitProg = driver->createProgram(
            driver->createShader(driver->makeRef(blit_vso, sizeof(blit_vso), nullptr, nullptr)),
            driver->createShader(driver->makeRef(blit_fso, sizeof(blit_fso), nullptr, nullptr)),
            true);
        if (!gUtils->blitProg.isValid())
            return false;

        gUtils->uTexture = driver->createUniform("u_texture", UniformType::Int1, 1);

        return true;
    }

    void gfx::shutdownGfxUtils()
    {
        if (!gUtils)
            return;

        if (gUtils->uTexture.isValid())
            gUtils->driver->destroyUniform(gUtils->uTexture);
        if (gUtils->blitProg.isValid())
            gUtils->driver->destroyProgram(gUtils->blitProg);
        if (gUtils->fsVb.isValid())
            gUtils->driver->destroyVertexBuffer(gUtils->fsVb);
        if (gUtils->fsIb.isValid())
            gUtils->driver->destroyIndexBuffer(gUtils->fsIb);
        gUtils->fsVb.reset();
        gUtils->fsIb.reset();
        BX_DELETE(getHeapAlloc(), gUtils);
        gUtils = nullptr;
    }

    /* references :
    *  http://en.wikipedia.org/wiki/Gaussian_blur
    *  http://en.wikipedia.org/wiki/Normal_distribution
    */
    void gfx::calcGaussKernel(vec4_t* kernel, int kernelSize, float stdDev, float intensity)
    {
        BX_ASSERT(kernelSize % 2 == 1);    // should be Odd number

        int hk = kernelSize / 2;
        float sum = 0.0f;
        float stdDevSqr = stdDev*stdDev;
        float k = bx::sqrt(2.0f*bx::kPi*stdDevSqr);

        for (int i = 0; i < kernelSize; i++) {
            float p = float(i - hk);
            float x = p / float(hk);
            float w = bx::exp2(-(x*x) / (2.0f*stdDevSqr)) / k;
            sum += w;
            kernel[i] = vec4(p, p, w, 0.0f);
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

    ProgramHandle gfx::loadProgram(GfxDriver* gfxDriver, IoDriver* ioDriver, const char* vsFilepath, 
                                    const char* fsFilepath)
    {
        GfxDriver* driver = gfxDriver;
        MemoryBlock* vso = ioDriver->read(vsFilepath, IoPathType::Assets, 0);
        if (!vso) {
            TEE_ERROR("Opening file '%s' failed", vsFilepath);
            return ProgramHandle();
        }

        MemoryBlock* fso = ioDriver->read(fsFilepath, IoPathType::Assets, 0);
        if (!fso) {
            TEE_ERROR("Opening file '%s' failed", fsFilepath);
            return ProgramHandle();
        }

        ShaderHandle vs = gfxDriver->createShader(gfxDriver->makeRef(vso->data, vso->size, releaseMemoryBlockCallback, vso));
        ShaderHandle fs = gfxDriver->createShader(gfxDriver->makeRef(fso->data, fso->size, releaseMemoryBlockCallback, fso));

        if (!vs.isValid() || !fs.isValid())
            return ProgramHandle();

        return driver->createProgram(vs, fs, true);
    }

    void gfx::blitToFramebuffer(uint8_t viewId, TextureHandle texture)
    {
        BX_ASSERT(texture.isValid());

        GfxDriver* driver = gUtils->driver;

        driver->setState(GfxState::RGBWrite | GfxState::AlphaWrite, 0);
        driver->setTexture(0, gUtils->uTexture, texture, TextureFlag::FromTexture);
        gfx::drawFullscreenQuad(viewId, gUtils->blitProg);
    }

    void gfx::drawFullscreenQuad(uint8_t viewId, ProgramHandle prog)
    {
        BX_ASSERT(gUtils->fsIb.isValid());
        BX_ASSERT(gUtils->fsVb.isValid());

        GfxDriver* driver = gUtils->driver;

        driver->setVertexBuffer(0, gUtils->fsVb);
        driver->setIndexBuffer(gUtils->fsIb, 0, 6);
        driver->submit(viewId, prog, 0, false);
    }

    ivec2_t gfx::getRelativeDisplaySize(int refWidth, int refHeight, int targetWidth, int targetHeight, DisplayPolicy::Enum policy)
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

        return ivec2(int(w), int(h));
    }

    PostProcessBlur* gfx::createBlurPostProcess(bx::AllocatorI* alloc, uint16_t width, uint16_t height, float stdDev,
                                                TextureFormat::Enum fmt /*= TextureFormat::RGBA8*/)
    {
        PostProcessBlur* blur = BX_NEW(alloc, PostProcessBlur)(alloc);
        if (!blur)
            return nullptr;

        GfxDriver* driver = gUtils->driver;
        for (int i = 0; i < 2; i++) {
            blur->fbs[i] = driver->createFrameBuffer(width, height, fmt,
                                                     TextureFlag::RT |
                                                     TextureFlag::MagPoint | TextureFlag::MinPoint |
                                                     TextureFlag::U_Clamp | TextureFlag::V_Clamp);
            BX_ASSERT(blur->fbs[i].isValid());
            blur->textures[i] = driver->getFrameBufferTexture(blur->fbs[i], 0);
        }

        calcGaussKernel(blur->kernel, BLUR_KERNEL_SIZE, stdDev, 1.0f);
        blur->width = width;
        blur->height = height;
        blur->fmt = fmt;

        blur->prog = driver->createProgram(
            driver->createShader(driver->makeRef(blur_vso, sizeof(blur_vso), nullptr, nullptr)),
            driver->createShader(driver->makeRef(blur_fso, sizeof(blur_fso), nullptr, nullptr)),
            true);

        blur->uTexture = driver->createUniform("u_texture", UniformType::Int1, 1);
        blur->uBlurKernel = driver->createUniform("u_blurKernel", UniformType::Vec4, BLUR_KERNEL_SIZE);
        return blur;
    }

    TextureHandle gfx::drawBlurPostProcess(PostProcessBlur* blur, uint8_t* viewId, TextureHandle sourceTexture, float radius)
    {
        uint8_t vid = *viewId;
        GfxDriver* driver = gUtils->driver;

        // Downsample to our first blur frameBuffer
        driver->setViewFrameBuffer(vid, blur->fbs[0]);
        driver->setViewRect(vid, 0, 0, blur->width, blur->height);
        gfx::blitToFramebuffer(vid, sourceTexture);
        vid++;

        vec4_t kernel[BLUR_KERNEL_SIZE];
        float hRadius = radius / float(blur->width);
        float vRadius = radius / float(blur->height);

        // Blur horizontally to 2nd framebuffer
        memcpy(kernel, blur->kernel, sizeof(kernel));
        for (int i = 0; i < BLUR_KERNEL_SIZE; i++) {
            kernel[i].x *= hRadius;
            kernel[i].y = 0;
        }

        driver->setViewRect(vid, 0, 0, blur->width, blur->height);
        driver->setViewFrameBuffer(vid, blur->fbs[1]);
        driver->setState(GfxState::RGBWrite, 0);
        driver->setTexture(0, blur->uTexture, blur->textures[0], TextureFlag::FromTexture);
        driver->setUniform(blur->uBlurKernel, kernel, BLUR_KERNEL_SIZE);
        gfx::drawFullscreenQuad(vid, blur->prog);
        vid++;

        // Blur vertically to 1st framebuffer
        memcpy(kernel, blur->kernel, sizeof(kernel));
        for (int i = 0; i < BLUR_KERNEL_SIZE; i++) {
            kernel[i].x = 0;
            kernel[i].y *= vRadius;
        }
        driver->setViewRect(vid, 0, 0, blur->width, blur->height);
        driver->setViewFrameBuffer(vid, blur->fbs[0]);
        driver->setState(GfxState::RGBWrite, 0);
        driver->setTexture(0, blur->uTexture, blur->textures[1], TextureFlag::FromTexture);
        driver->setUniform(blur->uBlurKernel, kernel, BLUR_KERNEL_SIZE);
        gfx::drawFullscreenQuad(vid, blur->prog);
        vid++;

        *viewId = vid;
        return blur->textures[0];
    }

    TextureHandle gfx::getBlurPostProcessTexture(PostProcessBlur* blur)
    {
        return blur->textures[0];
    }

    void gfx::destroyBlurPostProcess(PostProcessBlur* blur)
    {
        BX_ASSERT(blur);

        GfxDriver* driver = gUtils->driver;
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

    void gfx::resizeBlurPostProcessBuffers(PostProcessBlur* blur, uint16_t width, uint16_t height, float stdDev)
    {
        BX_ASSERT(blur);
        GfxDriver* driver = gUtils->driver;
        if (blur->fbs[0].isValid())
            driver->destroyFrameBuffer(blur->fbs[0]);
        if (blur->fbs[1].isValid())
            driver->destroyFrameBuffer(blur->fbs[1]);

        calcGaussKernel(blur->kernel, BLUR_KERNEL_SIZE, stdDev, 1.0f);
        blur->width = width;
        blur->height = height;

        for (int i = 0; i < 2; i++) {
            blur->fbs[i] = driver->createFrameBuffer(width, height, blur->fmt,
                                                     TextureFlag::RT |
                                                     TextureFlag::MagPoint | TextureFlag::MinPoint |
                                                     TextureFlag::U_Clamp | TextureFlag::V_Clamp);
            BX_ASSERT(blur->fbs[i].isValid());
            blur->textures[i] = driver->getFrameBufferTexture(blur->fbs[i], 0);
        }
    }

    PostProcessVignetteSepia* gfx::createVignetteSepiaPostProcess(bx::AllocatorI* alloc, uint16_t width, uint16_t height,
                                                             float start, float end, 
                                                             float vignetteIntensity, float sepiaIntensity, 
                                                             ucolor_t sepiaColor, ucolor_t vignetteColor)
    {
        GfxDriver* driver = gUtils->driver;

        PostProcessVignetteSepia* vignette = BX_NEW(alloc, PostProcessVignetteSepia)(alloc);
        vignette->width = width;
        vignette->height = height;
        vignette->start = start;
        vignette->end = end;
        vignette->sepiaColor = tmath::ucolorToVec4(sepiaColor);
        vignette->vignetteColor = tmath::ucolorToVec4(vignetteColor);
        vignette->vignetteIntensity = vignetteIntensity;
        vignette->sepiaIntensity = sepiaIntensity;

        vignette->prog = driver->createProgram(
            driver->createShader(driver->makeRef(vignette_sepia_vso, sizeof(vignette_sepia_vso), nullptr, nullptr)),
            driver->createShader(driver->makeRef(vignette_sepia_fso, sizeof(vignette_sepia_fso), nullptr, nullptr)),
            true);

        vignette->uTexture = driver->createUniform("u_texture", UniformType::Int1, 1);
        vignette->uVignetteParams = driver->createUniform("u_vignetteParams", UniformType::Vec4, 1);
        vignette->uSepiaParams = driver->createUniform("u_sepiaParams", UniformType::Vec4, 1);
        vignette->uVignetteColor = driver->createUniform("u_vignetteColor", UniformType::Vec4, 1);

        return vignette;
    }

    TextureHandle gfx::drawVignetteSepiaPostProcess(PostProcessVignetteSepia* vignette, uint8_t viewId,
                                               FrameBufferHandle targetFb, TextureHandle sourceTexture,
                                               float intensity)
    {
        GfxDriver* driver = gUtils->driver;

        vec4_t vigParams = vec4(vignette->start, vignette->end, intensity*vignette->vignetteIntensity, 0);
        vec4_t sepiaParams = vec4(vignette->sepiaColor.x, vignette->sepiaColor.y, vignette->sepiaColor.z, 
                                  intensity*vignette->sepiaIntensity);

        driver->setViewRect(viewId, 0, 0, vignette->width, vignette->height);
        driver->setViewFrameBuffer(viewId, targetFb);
        driver->setState(GfxState::RGBWrite, 0);
        driver->setTexture(0, vignette->uTexture, sourceTexture, TextureFlag::FromTexture);
        driver->setUniform(vignette->uVignetteParams, vigParams.f, 1);
        driver->setUniform(vignette->uSepiaParams, sepiaParams.f, 1);
        driver->setUniform(vignette->uVignetteColor, vignette->vignetteColor.f, 1);

        gfx::drawFullscreenQuad(viewId, vignette->prog);
        return driver->getFrameBufferTexture(targetFb, 0);
    }

    TextureHandle gfx::drawVignettePostProcessOverride(PostProcessVignetteSepia* vignette, uint8_t viewId, FrameBufferHandle targetFb,
                                                  TextureHandle sourceTexture, float start, float end, float intensity,
                                                  vec4_t vignetteColor)
    {
        GfxDriver* driver = gUtils->driver;

        vec4_t vigParams = vec4(start, end, intensity, 0);
        vec4_t sepiaParams = vec4(1.0f, 1.0f, 1.0f, 0);

        driver->setViewRect(viewId, 0, 0, vignette->width, vignette->height);
        driver->setViewFrameBuffer(viewId, targetFb);
        driver->setState(GfxState::RGBWrite, 0);
        driver->setTexture(0, vignette->uTexture, sourceTexture, TextureFlag::FromTexture);
        driver->setUniform(vignette->uVignetteParams, vigParams.f, 1);
        driver->setUniform(vignette->uSepiaParams, sepiaParams.f, 1);
        driver->setUniform(vignette->uVignetteColor, vignetteColor.f, 1);
        gfx::drawFullscreenQuad(viewId, vignette->prog);
        return driver->getFrameBufferTexture(targetFb, 0);
    }

    PostProcessTint* gfx::createTintPostPorcess(bx::AllocatorI* alloc, uint16_t width, uint16_t height)
    {
        GfxDriver* driver = gUtils->driver;
        PostProcessTint* tint = BX_NEW(alloc, PostProcessTint)(alloc);

        tint->prog = driver->createProgram(
            driver->createShader(driver->makeRef(tint_vso, sizeof(tint_vso), nullptr, nullptr)),
            driver->createShader(driver->makeRef(tint_fso, sizeof(tint_fso), nullptr, nullptr)),
            true);

        tint->uTexture = driver->createUniform("u_texture", UniformType::Int1, 1);
        tint->uTintColor = driver->createUniform("u_tintColor", UniformType::Vec4, 1);
        tint->width = width;
        tint->height = height;

        return tint;
    }

    void gfx::destroyTintPostProcess(PostProcessTint* tint)
    {
        GfxDriver* driver = gUtils->driver;
        if (tint->uTexture.isValid())
            driver->destroyUniform(tint->uTexture);
        if (tint->uTintColor.isValid())
            driver->destroyUniform(tint->uTintColor);
        if (tint->prog.isValid())
            driver->destroyProgram(tint->prog);
        BX_DELETE(tint->alloc, tint);
    }

    TextureHandle gfx::drawTintPostProcess(PostProcessTint* tint, uint8_t viewId, FrameBufferHandle targetFb, 
                                  TextureHandle sourceTexture, const vec4_t& color, float intensity)
    {
        GfxDriver* driver = gUtils->driver;

        vec4_t tintColor = vec4(color.x, color.y, color.z, intensity);

        driver->setViewRect(viewId, 0, 0, tint->width, tint->height);
        driver->setViewFrameBuffer(viewId, targetFb);
        driver->setState(GfxState::RGBWrite, 0);
        driver->setTexture(0, tint->uTexture, sourceTexture, TextureFlag::FromTexture);
        driver->setUniform(tint->uTintColor, tintColor.f, 1);
        gfx::drawFullscreenQuad(viewId, tint->prog);
        return driver->getFrameBufferTexture(targetFb, 0);
    }

    void gfx::resizeTintPostProcessBuffers(PostProcessTint* tint, uint16_t width, uint16_t height)
    {
        tint->width = width;
        tint->height = height;
    }

    void gfx::destroyVignetteSepiaPostProcess(PostProcessVignetteSepia* vignette)
    {
        BX_ASSERT(vignette);

        GfxDriver* driver = gUtils->driver;
        if (vignette->uTexture.isValid())
            driver->destroyUniform(vignette->uTexture);
        if (vignette->uVignetteParams.isValid())
            driver->destroyUniform(vignette->uVignetteParams);
        if (vignette->uSepiaParams.isValid())
            driver->destroyUniform(vignette->uSepiaParams);
        if (vignette->uVignetteColor.isValid())
            driver->destroyUniform(vignette->uVignetteColor);
        if (vignette->prog.isValid())
            driver->destroyProgram(vignette->prog);
        BX_DELETE(vignette->alloc, vignette);
    }
     
    void gfx::resizeVignetteSepiaPostProcessBuffers(PostProcessVignetteSepia* vignette, uint16_t width, uint16_t height)
    {
        BX_ASSERT(vignette, "");

        vignette->width = width;
        vignette->height = height;
    }

}