#include "pch.h"

#include "gfx_driver.h"
#include "gfx_utils.h"
#include "io_driver.h"

using namespace termite;

struct VertexFs
{
    float x, y, z;
    float tx, ty;

    static VertexDecl Decl;

    static void init()
    {
        vdeclBegin(&Decl);
        vdeclAdd(&Decl, VertexAttrib::Position, 3, VertexAttribType::Float);
        vdeclAdd(&Decl, VertexAttrib::TexCoord0, 2, VertexAttribType::Float);
        vdeclEnd(&Decl);
    }
};
VertexDecl VertexFs::Decl;

static VertexBufferHandle g_fsVb;
static IndexBufferHandle g_fsIb;
static GfxDriverApi* g_driver = nullptr;

result_t termite::initGfxUtils(GfxDriverApi* driver)
{
    VertexFs::init();
    static VertexFs fsQuad[] = {
        { -1.0f,  1.0f,  0,  0,    0 },        // top-left
        {  1.0f,  1.0f,  0,  1.0f, 0 },        // top-right
        { -1.0f, -1.0f,  0,  0,    1.0f },     // bottom-left
        {  1.0f, -1.0f,  0,  1.0f, 1.0f }      // bottom-right
    };

    static uint16_t indices[] = {
        0, 1, 2,
        2, 1, 3
    };


    if (!g_fsVb.isValid()) {
        g_fsVb = driver->createVertexBuffer(driver->makeRef(fsQuad, sizeof(VertexFs) * 4, nullptr, nullptr), 
            VertexFs::Decl, GpuBufferFlag::None);
        if (!g_fsVb.isValid())
            return T_ERR_FAILED;
    }

    if (!g_fsIb.isValid()) {
        g_fsIb = driver->createIndexBuffer(driver->makeRef(indices, sizeof(uint16_t) * 6, nullptr, nullptr), 
            GpuBufferFlag::None);
        if (!g_fsIb.isValid())
            return T_ERR_FAILED;
    }

    g_driver = driver;
    return 0;
}

void termite::shutdownGfxUtils()
{
    if (!g_driver)
        return;

    if (g_fsVb.isValid())
        g_driver->destroyVertexBuffer(g_fsVb);
    if (g_fsIb.isValid())
        g_driver->destroyIndexBuffer(g_fsIb);
    g_fsVb.reset();
    g_fsIb.reset();
    g_driver = nullptr;
}

/* references :
*  http://en.wikipedia.org/wiki/Gaussian_blur
*  http://en.wikipedia.org/wiki/Normal_distribution
*/
void termite::calcGaussKernel(vec4_t* kernel, int kernelSize, float stdDevSqr, float intensity, int direction, 
                                 int width, int height)
{
    assert(kernelSize % 2 == 1);    // should be odd number

    int hk = kernelSize / 2;
    float sum = 0.0f;
    float w_stride = 1.0f / (float)width;
    float h_stride = 1.0f / (float)height;

    for (int i = 0; i < kernelSize; i++) {
        float p = (float)(i - hk);
        float x = p / (float)hk;
        float w = expf(-(x*x) / (2.0f*stdDevSqr)) / sqrtf(2.0f*bx::pi*stdDevSqr);
        sum += w;
        kernel[i] = vec4f(
            direction == 0 ? (w_stride*p) : 0.0f,
            direction == 1 ? (h_stride*p) : 0.0f,
            w,
            0.0f);
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

ProgramHandle termite::loadShaderProgram(GfxDriverApi* gfxDriver, IoDriverApi* ioDriver, const char* vsFilepath, 
                                         const char* fsFilepath)
{
    GfxDriverApi* driver = gfxDriver;
    MemoryBlock* vso = ioDriver->read(vsFilepath);
    if (!vso) {
        T_ERROR("Opening file '%s' failed", vsFilepath);
        return ProgramHandle();
    }

    MemoryBlock* fso = ioDriver->read(fsFilepath);
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

void termite::drawFullscreenQuad(uint8_t viewId, ProgramHandle prog)
{
    assert(g_fsIb.isValid());
    assert(g_fsVb.isValid());

    GfxDriverApi* driver = g_driver;

    driver->setVertexBuffer(g_fsVb);
    driver->setIndexBuffer(g_fsIb, 0, 6);
    driver->submit(viewId, prog, 0, false);
}
