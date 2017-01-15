#include "pch.h"

#include "gfx_driver.h"
#include "gfx_utils.h"
#include "io_driver.h"

#include T_MAKE_SHADER_PATH(shaders_h, blit.vso)
#include T_MAKE_SHADER_PATH(shaders_h, blit.fso)

using namespace termite;

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

struct GfxUtils
{
    VertexBufferHandle fsVb;
    IndexBufferHandle fsIb;
    GfxDriverApi* driver;
    ProgramHandle blitProg;
    UniformHandle utexture;

    GfxUtils() :
        driver(nullptr)
    {
    }
};
static GfxUtils* g_gutils = nullptr;

result_t termite::initGfxUtils(GfxDriverApi* driver)
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
    g_gutils->utexture = driver->createUniform("u_texture", UniformType::Int1, 1);
        
    return 0;
}

void termite::shutdownGfxUtils()
{
    if (!g_gutils)
        return;

    if (g_gutils->utexture.isValid())
        g_gutils->driver->destroyUniform(g_gutils->utexture);
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

void termite::blitFullscreen(uint8_t viewId, TextureHandle texture)
{
    assert(texture.isValid());

    GfxDriverApi* driver = g_gutils->driver;

    driver->setViewRectRatio(viewId, 0, 0, BackbufferRatio::Equal);
    driver->setViewFrameBuffer(viewId, FrameBufferHandle());
                             
    driver->setState(GfxState::RGBWrite | GfxState::AlphaWrite, 0);
    driver->setTexture(0, g_gutils->utexture, texture, TextureFlag::FromTexture);
    drawFullscreenQuad(viewId, g_gutils->blitProg);
}

void termite::drawFullscreenQuad(uint8_t viewId, ProgramHandle prog)
{
    assert(g_gutils->fsIb.isValid());
    assert(g_gutils->fsVb.isValid());

    GfxDriverApi* driver = g_gutils->driver;

    driver->setVertexBuffer(g_gutils->fsVb);
    driver->setIndexBuffer(g_gutils->fsIb, 0, 6);
    driver->submit(viewId, prog, 0, false);
}

vec2i_t termite::getRelativeDisplaySize(int refWidth, int refHeight, int targetWidth, int targetHeight, 
                                          DisplayPolicy::Enum policy)
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
