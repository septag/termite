#pragma once

#include "bx/bx.h"

#include "gfx_defines.h"
#include "vec_math.h"

namespace termite
{
    struct GfxDriverApi;
    struct IoDriverApi;

    result_t initGfxUtils(GfxDriverApi* driver);
    void shutdownGfxUtils();

    TERMITE_API void calcGaussKernel(vec4_t* kernel, int kernelSize, float stdDevSqr, float intensity, 
                                     int direction /*=0 horizontal, =1 vertical*/, int width, int height);
    TERMITE_API ProgramHandle loadShaderProgram(GfxDriverApi* gfxDriver, IoDriverApi* ioDriver, const char* vsFilepath, 
                                                      const char* fsFilepath);
    TERMITE_API void drawFullscreenQuad(uint8_t viewId, ProgramHandle prog);
} // namespace termite

