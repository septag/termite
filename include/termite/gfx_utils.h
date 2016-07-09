#pragma once

#include "bx/bx.h"

#include "gfx_defines.h"
#include "vec_math.h"

namespace termite
{
    struct GfxApi;
    class IoDriverI;

    result_t initGfxUtils(GfxApi* driver);
    void shutdownGfxUtils();

    TERMITE_API void calcGaussKernel(vec4_t* kernel, int kernelSize, float stdDevSqr, float intensity, 
                                     int direction /*=0 horizontal, =1 vertical*/, int width, int height);
    TERMITE_API ProgramHandle loadShaderProgram(GfxApi* gfxDriver, IoDriverI* ioDriver, const char* vsFilepath, 
                                                      const char* fsFilepath);
    TERMITE_API void drawFullscreenQuad(uint8_t viewId, ProgramHandle prog);
} // namespace termite

