#pragma once

#include "bx/bx.h"

#include "gfx_defines.h"
#include "math.h"

namespace tee
{
    struct IoDriver;
    struct PostProcessBlur;
    struct PostProcessVignetteSepia;
    struct PostProcessTint;
    struct GfxDriver;

    struct DisplayPolicy
    {
        enum Enum
        {
            FitToHeight,
            FitToWidth
        };
    };

    namespace gfx {
        TEE_API void calcGaussKernel(vec4_t* kernel, int kernelSize, float stdDev, float intensity);
        TEE_API ProgramHandle loadProgram(GfxDriver* gfxDriver, IoDriver* ioDriver, const char* vsFilepath, const char* fsFilepath);
        TEE_API void drawFullscreenQuad(uint8_t viewId, ProgramHandle prog);
        TEE_API void blitToFramebuffer(uint8_t viewId, TextureHandle texture);
        TEE_API ivec2_t getRelativeDisplaySize(int refWidth, int refHeight, int targetWidth, int targetHeight,
                                               DisplayPolicy::Enum policy);

        // Blur PostProcess
        TEE_API PostProcessBlur* createBlurPostProcess(bx::AllocatorI* alloc,
                                                       uint16_t width, uint16_t height,
                                                       float stdDev,
                                                       TextureFormat::Enum fmt = TextureFormat::RGBA8);
        TEE_API TextureHandle drawBlurPostProcess(PostProcessBlur* blur, uint8_t* viewId, TextureHandle sourceTexture,
                                                  float radius = 1.0f);
        TEE_API TextureHandle getBlurPostProcessTexture(PostProcessBlur* blur);
        TEE_API void destroyBlurPostProcess(PostProcessBlur* blur);
        TEE_API void resizeBlurPostProcessBuffers(PostProcessBlur* blur, uint16_t width, uint16_t height, float stdDev);

        // Vignette/Sepia PostProcess
        TEE_API PostProcessVignetteSepia* createVignetteSepiaPostProcess(bx::AllocatorI* alloc,
                                                                         uint16_t width, uint16_t height,
                                                                         float start, float end,
                                                                         float vignetteIntensity,
                                                                         float sepiaIntensity,
                                                                         ucolor_t sepiaColor,
                                                                         ucolor_t vignetteColor = ucolor(0));
        TEE_API void destroyVignetteSepiaPostProcess(PostProcessVignetteSepia* vignette);
        TEE_API void resizeVignetteSepiaPostProcessBuffers(PostProcessVignetteSepia* vignette, uint16_t width, uint16_t height);
        TEE_API TextureHandle drawVignetteSepiaPostProcess(PostProcessVignetteSepia* vignette, uint8_t viewId,
                                                           FrameBufferHandle targetFb, TextureHandle sourceTexture,
                                                           float intensity = 1.0f);
        TEE_API TextureHandle drawVignettePostProcessOverride(PostProcessVignetteSepia* vignette, uint8_t viewId,
                                                              FrameBufferHandle targetFb, TextureHandle sourceTexture,
                                                              float start, float end, float intensity = 1.0f,
                                                              vec4_t vignetteColor = vec4(0, 0, 0, 0));

        // Tint PostProcess
        TEE_API PostProcessTint* createTintPostPorcess(bx::AllocatorI* alloc, uint16_t width, uint16_t height);
        TEE_API void destroyTintPostProcess(PostProcessTint* tint);
        TEE_API TextureHandle drawTintPostProcess(PostProcessTint* tint, uint8_t viewId, FrameBufferHandle targetFb, 
                                                  TextureHandle sourceTexture, const vec4_t& color, float intensity);
        TEE_API void resizeTintPostProcessBuffers(PostProcessTint* tint, uint16_t width, uint16_t height);

    }

} // namespace tee

