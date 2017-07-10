#pragma once

#include "bx/bx.h"

#include "gfx_defines.h"
#include "vec_math.h"

namespace termite
{
    struct GfxDriverApi;
    struct IoDriverApi;
    struct PostProcessBlur;
    struct PostProcessVignetteSepia;

    struct DisplayPolicy
    {
        enum Enum
        {
            FitToHeight,
            FitToWidth
        };
    };

    result_t initGfxUtils(GfxDriverApi* driver);
    void shutdownGfxUtils();

    TERMITE_API void calcGaussKernel(vec4_t* kernel, int kernelSize, float stdDev, float intensity);
    TERMITE_API ProgramHandle loadShaderProgram(GfxDriverApi* gfxDriver, IoDriverApi* ioDriver, const char* vsFilepath, 
                                                const char* fsFilepath);
    TERMITE_API void drawFullscreenQuad(uint8_t viewId, ProgramHandle prog);
    TERMITE_API void blitToFramebuffer(uint8_t viewId, TextureHandle texture);

    TERMITE_API vec2i_t getRelativeDisplaySize(int refWidth, int refHeight, int targetWidth, int targetHeight, 
                                               DisplayPolicy::Enum policy);

    // Blur PostProcess
    TERMITE_API PostProcessBlur* createBlurPostProcess(bx::AllocatorI* alloc,
                                                       uint16_t width, uint16_t height, 
                                                       float stdDev, 
                                                       TextureFormat::Enum fmt = TextureFormat::RGBA8);
    TERMITE_API TextureHandle drawBlurPostProcess(PostProcessBlur* blur, uint8_t* viewId, TextureHandle sourceTexture, 
                                                  float radius = 1.0f);
    TERMITE_API TextureHandle getBlurPostProcessTexture(PostProcessBlur* blur);
    TERMITE_API void destroyBlurPostProcess(PostProcessBlur* blur);

    // Vignette/Sepia PostProcess
    TERMITE_API PostProcessVignetteSepia* createVignetteSepiaPostProcess(bx::AllocatorI* alloc,
                                                                         uint16_t width, uint16_t height, 
                                                                         float radius, float softness, 
                                                                         float vignetteIntensity,
                                                                         float sepiaIntensity,
                                                                         color_t sepiaColor);
    TERMITE_API TextureHandle drawVignetteSepiaPostProcess(PostProcessVignetteSepia* vignette, uint8_t viewId,
                                                           FrameBufferHandle targetFb, TextureHandle sourceTexture, 
                                                           float intensity = 1.0f);
    TERMITE_API void destroyVignetteSepiaPostProcess(PostProcessVignetteSepia* vignette);

} // namespace termite

