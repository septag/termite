#include "pch.h"

#define T_CORE_API
#define T_GFX_API
#include "plugin_api.h"

#include "gfx_utils.h"
#include "gfx_driver.h"

void* termite::getEngineApi(uint16_t apiId, uint32_t version)
{
    ApiId id = ApiId(apiId);
    if (id == ApiId::Core && version == 0) {
        static CoreApi_v0 core0;
        core0.copyMemoryBlock = copyMemoryBlock;
        core0.createMemoryBlock = createMemoryBlock;
        core0.readTextFile = readTextFile;
        core0.refMemoryBlock = refMemoryBlock;
        core0.refMemoryBlockPtr = refMemoryBlockPtr;
        core0.releaseMemoryBlock = releaseMemoryBlock;
        core0.getElapsedTime = getElapsedTime;
        core0.reportError = reportError;
        core0.reportErrorf = reportErrorf;
        core0.logBeginProgress = bx::logBeginProgress;
        core0.logEndProgress = bx::logEndProgress;
        core0.logPrint = bx::logPrint;
        core0.logPrintf = bx::logPrintf;
        core0.getConfig = getConfig;
        core0.getEngineVersion = getEngineVersion;
        core0.getTempAlloc = getTempAlloc;

        return &core0;
    } else if (id == ApiId::Gfx && version == 0) {
        static GfxApi_v0 gfx0;
        gfx0.calcGaussKernel = calcGaussKernel;
        gfx0.drawFullscreenQuad = drawFullscreenQuad;
        gfx0.loadShaderProgram = loadShaderProgram;
        gfx0.vdeclAdd = vdeclAdd;
        gfx0.vdeclBegin = vdeclBegin;
        gfx0.vdeclEnd = vdeclEnd;
        gfx0.vdeclDecode = vdeclDecode;
        gfx0.vdeclGetSize = vdeclGetSize;
        gfx0.vdeclHas = vdeclHas;
        gfx0.vdeclSkip = vdeclSkip;
        return &gfx0;
    }

    return nullptr;
}
