#include "pch.h"

#include "bxx/pool.h"
#include "bxx/array.h"
#include "bxx/logger.h"
#include "bx/fpumath.h"
#include "bx/uint32_t.h"

#include "gfx_driver.h"
#include "gfx_texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize.h"

#include "bxx/linear_allocator.h"
#include "bxx/leakcheck_allocator.h"
#include "bxx/proxy_allocator.h"
#include "bxx/path.h"

using namespace termite;

class TextureLoaderAll : public ResourceCallbacksI
{
public:
    TextureLoaderAll() {}

    bool loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
    void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
    void onReload(ResourceHandle handle, bx::AllocatorI* alloc) override;
};

struct TextureLoader
{
    bx::Pool<Texture> texturePool;
    TextureLoaderAll loader;
    bx::AllocatorI* alloc;
    Texture* whiteTexture;
    Texture* blackTexture;
    Texture* asyncBlankTexture;
    Texture* failTexture;
    GfxDriverApi* driver;

    TextureLoader(bx::AllocatorI* _alloc)
    {
        alloc = _alloc;
        whiteTexture = nullptr;
        asyncBlankTexture = nullptr;
        failTexture = nullptr;
        driver = nullptr;
    }
};

static TextureLoader* g_texLoader = nullptr;

result_t termite::initTextureLoader(GfxDriverApi* driver, bx::AllocatorI* alloc, int texturePoolSize)
{
    assert(driver);
    if (g_texLoader) {
        assert(false);
        return T_ERR_ALREADY_INITIALIZED;
    }

    TextureLoader* loader = BX_NEW(alloc, TextureLoader)(alloc);
    g_texLoader = loader;
    loader->driver = driver;

    if (!loader->texturePool.create(texturePoolSize, alloc)) {
        return T_ERR_OUTOFMEM;
    }

    // Create a default async object (1x1 RGBA white)
    static uint32_t whitePixel = 0xffffffff;
    g_texLoader->whiteTexture = loader->texturePool.newInstance();
    g_texLoader->whiteTexture->handle = driver->createTexture2D(1, 1, false, 1, TextureFormat::RGBA8,
        TextureFlag::U_Clamp|TextureFlag::V_Clamp|TextureFlag::MinPoint|TextureFlag::MagPoint,
        driver->makeRef(&whitePixel, sizeof(uint32_t), nullptr, nullptr));
    g_texLoader->whiteTexture->info.width = 1;
    g_texLoader->whiteTexture->info.height = 1;
    g_texLoader->whiteTexture->info.format = TextureFormat::RGBA8;
    if (!g_texLoader->whiteTexture->handle.isValid()) {
        T_ERROR("Creating Blank 1x1 texture failed");
        return T_ERR_FAILED;
    }
    g_texLoader->asyncBlankTexture = g_texLoader->whiteTexture;

    static uint32_t blackPixel = 0xff000000;
    g_texLoader->blackTexture = loader->texturePool.newInstance();
    g_texLoader->blackTexture->handle = driver->createTexture2D(1, 1, false, 1, TextureFormat::RGBA8,
                                                                TextureFlag::U_Clamp|TextureFlag::V_Clamp|TextureFlag::MinPoint|TextureFlag::MagPoint,
                                                                driver->makeRef(&blackPixel, sizeof(uint32_t), nullptr, nullptr));
    g_texLoader->blackTexture->info.width = 1;
    g_texLoader->blackTexture->info.height = 1;
    g_texLoader->blackTexture->info.format = TextureFormat::RGBA8;
    if (!g_texLoader->blackTexture->handle.isValid()) {
        T_ERROR("Creating Blank 1x1 texture failed");
        return T_ERR_FAILED;
    }

    // Fail texture (Checkers 2x2)
    static uint32_t checkerPixels[] = {
        0xff0000ff,
        0xffffffff,
        0xff0000ff,
        0xffffffff
    };
    g_texLoader->failTexture = loader->texturePool.newInstance();
    if (!g_texLoader->failTexture)
        return T_ERR_OUTOFMEM;
    g_texLoader->failTexture->handle = driver->createTexture2D(2, 2, false, 1, TextureFormat::RGBA8,
                                                               TextureFlag::MinPoint | TextureFlag::MagPoint,
                                                               driver->makeRef(checkerPixels, sizeof(checkerPixels), nullptr, nullptr));
    g_texLoader->failTexture->info.width = 2;
    g_texLoader->failTexture->info.height = 2;
    g_texLoader->failTexture->info.format = TextureFormat::RGBA8;
    if (!g_texLoader->failTexture->handle.isValid()) {
        T_ERROR("Creating Fail 2x2 texture failed");
        return T_ERR_FAILED;
    }

    return 0;
}

void termite::registerTextureToResourceLib()
{
    ResourceTypeHandle handle;
    handle = registerResourceType("texture", &g_texLoader->loader, sizeof(LoadTextureParams),
                                  uintptr_t(g_texLoader->failTexture), uintptr_t(g_texLoader->asyncBlankTexture));
    assert(handle.isValid());
}

void termite::shutdownTextureLoader()
{
    if (!g_texLoader)
        return;
    
    if (g_texLoader->whiteTexture) {
        if (g_texLoader->whiteTexture->handle.isValid())
            g_texLoader->driver->destroyTexture(g_texLoader->whiteTexture->handle);
    }

    if (g_texLoader->blackTexture) {
        if (g_texLoader->blackTexture->handle.isValid())
            g_texLoader->driver->destroyTexture(g_texLoader->blackTexture->handle);
    }

    if (g_texLoader->failTexture) {
        if (g_texLoader->failTexture->handle.isValid())
            g_texLoader->driver->destroyTexture(g_texLoader->failTexture->handle);
    }

    g_texLoader->texturePool.destroy();

    BX_DELETE(g_texLoader->alloc, g_texLoader);
    g_texLoader = nullptr;
}

TextureHandle termite::getWhiteTexture1x1()
{
    return g_texLoader->whiteTexture->handle;
}

termite::TextureHandle termite::getBlackTexture1x1()
{
    return g_texLoader->blackTexture->handle;
}

bool termite::blitRawPixels(uint8_t* dest, int destX, int destY, int destWidth, int destHeight, const uint8_t* src, 
                            int srcX, int srcY, int srcWidth, int srcHeight, int pixelSize)
{
    if ((destWidth - destX) < (srcWidth - srcX))
        return false;
    if ((destHeight - destY) < (srcHeight - srcY))
        return false;

    int destOffset = 0;
    for (int y = srcY; y < srcHeight; y++) {
        memcpy(dest + (destX + (destY + destOffset)*destWidth)*pixelSize, src + (srcX + y*srcWidth)*pixelSize, 
               srcWidth*pixelSize);
        destOffset++;
    }

    return true;
}


static void stbCallbackFreeImage(void* ptr, void* userData)
{
    stbi_image_free(ptr);
}

static bool loadUncompressed(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
{
    assert(g_texLoader);
    GfxDriverApi* driver = g_texLoader->driver;
    const LoadTextureParams* texParams = (const LoadTextureParams*)params.userParams;

    int numComp;
    TextureFormat::Enum fmt = texParams->fmt;
    switch (fmt) {
        case TextureFormat::Unknown:
            numComp = 0;
            break;

        case TextureFormat::RGBA8:
        case TextureFormat::RGBA8S:
        case TextureFormat::RGBA8I:
        case TextureFormat::RGBA8U:
            numComp = 4;
            break;

        case TextureFormat::RGB8:
        case TextureFormat::RGB8I:
        case TextureFormat::RGB8U:
        case TextureFormat::RGB8S:
            numComp = 3;
            break;

        case TextureFormat::R8:
        case TextureFormat::R8I:
        case TextureFormat::R8U:
        case TextureFormat::R8S:
            numComp = 1;
            break;

        case TextureFormat::RG8:
        case TextureFormat::RG8I:
        case TextureFormat::RG8U:
        case TextureFormat::RG8S:
            numComp = 2;
            break;

        default:
            assert(0);  // Unsupported format
            return false;
    }

    int width, height, comp;
    uint8_t* pixels = stbi_load_from_memory(mem->data, mem->size, &width, &height, &comp, numComp);
    if (!pixels)
        return false;

    Texture* texture;
    if (alloc)
        texture = BX_NEW(alloc, Texture)();
    else
        texture = g_texLoader->texturePool.newInstance();
    if (!texture) {
        stbi_image_free(pixels);
        return false;
    }

    // If texture format is Unknown, fix the format by guessing it
    if (fmt == TextureFormat::Unknown) {
        switch (comp) {
        case 4:
            fmt = TextureFormat::RGBA8;
            break;
        case 3:
            fmt = TextureFormat::RGB8;
            break;
        case 2:
            fmt = TextureFormat::RG8;
            break;
        case 1:
            fmt = TextureFormat::R8;
            break;
        }
        numComp = comp;
    }

    // Generate mips
    int numMips = 1;    // default
    const GfxMemory* gmem = nullptr;
    if (texParams->generateMips) {
        numMips = 1 + (int)bx::ffloor(bx::flog2((float)bx::uint32_max(width, height)));
        int skipMips = bx::uint32_min(numMips - 1, texParams->skipMips);
        int origWidth = width, origHeight = height;

        // We have the first mip, calculate image size
        uint32_t sizeBytes = 0;
        int mipWidth = width, mipHeight = height;
        for (int i = 0; i < numMips; i++) {
            if (i >= skipMips) {
                sizeBytes += mipWidth*mipHeight*numComp;
                if (i == skipMips) {
                    width = mipWidth;
                    height = mipHeight;
                }
            }
            mipWidth = bx::uint32_max(1, mipWidth >> 1);
            mipHeight = bx::uint32_max(1, mipHeight >> 1);
        }
        numMips -= skipMips;
        
        // Allocate the buffer and generate mips
        gmem = driver->alloc(sizeBytes);
        if (!gmem) {
            stbi_image_free(pixels);
            if (alloc)
                BX_DELETE(alloc, texture);
            else
                g_texLoader->texturePool.deleteInstance(texture);
            return false;
        }

        uint8_t* srcPixels = gmem->data;
        if (skipMips > 0) {
            stbir_resize_uint8(pixels, origWidth, origHeight, 0, srcPixels, width, height, 0, numComp);
        } else {
            memcpy(srcPixels, pixels, width*height*numComp);
        }
        stbi_image_free(pixels);

        int srcWidth = width, srcHeight = height;
        mipWidth = width >> 1;   
        mipHeight = height >> 1;
        for (int i = 1; i < numMips; i++) {
            uint8_t* destPixels = srcPixels + srcWidth*srcHeight*numComp;

            stbir_resize_uint8(srcPixels, srcWidth, srcHeight, 0, destPixels, mipWidth, mipHeight, 0, numComp);

            srcWidth = mipWidth;
            srcHeight = mipHeight;
            srcPixels = destPixels;

            mipWidth = bx::uint32_max(1, mipWidth >> 1);
            mipHeight = bx::uint32_max(1, mipHeight >> 1);
        }        
    } else {
        gmem = driver->makeRef(pixels, width*height*numComp, stbCallbackFreeImage, nullptr);
    }

    texture->handle = driver->createTexture2D(width, height, texParams->generateMips, 1, fmt, 
                                              texParams->flags, gmem);
    if (!texture->handle.isValid())
        return false;

    TextureInfo* info = &texture->info;
    info->width = width;
    info->height = height;
    info->format = fmt;
    info->numMips = 1;
    info->storageSize = width * height * numComp;
    info->bitsPerPixel = numComp * 8;
    texture->ratio = float(info->width) / float(info->height);

    *obj = uintptr_t(texture);

    return true;
}

static bool loadCompressed(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
{
    const LoadTextureParams* texParams = (const LoadTextureParams*)params.userParams;
    
    Texture* texture;
    if (alloc)
        texture = BX_NEW(alloc, Texture)();
    else
        texture = g_texLoader->texturePool.newInstance();
    if (!texture)
        return false;

    GfxDriverApi* driver = g_texLoader->driver;
    texture->handle = driver->createTexture(driver->copy(mem->data, mem->size), texParams->flags, texParams->skipMips,
                                            &texture->info);
    if (!texture->handle.isValid())
        return false;
    texture->ratio = float(texture->info.width) / float(texture->info.height);

    *obj = uintptr_t(texture);
    return true;
}


bool TextureLoaderAll::loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj,
                               bx::AllocatorI* alloc)
{
    bx::Path path(params.uri);
    bx::Path ext = path.getFileExt();
    ext.toLower();
    if (ext.isEqual("ktx") || ext.isEqual("dds")) {
        return loadCompressed(mem, params, obj, alloc);
    } else if (ext.isEqual("png") || ext.isEqual("tga") || ext.isEqual("jpg") || ext.isEqual("bmp") ||
               ext.isEqual("jpeg") || ext.isEqual("psd") || ext.isEqual("hdr") || ext.isEqual("gif"))
    {
        return loadUncompressed(mem, params, obj, alloc);
    } else {
        return false;
    }
}

void TextureLoaderAll::unloadObj(uintptr_t obj, bx::AllocatorI* alloc)
{
    assert(g_texLoader);
    assert(obj);

    Texture* texture = (Texture*)obj;
    if (texture->handle.isValid())
        g_texLoader->driver->destroyTexture(texture->handle);

    if (alloc)
        BX_DELETE(alloc, texture);
    else
        g_texLoader->texturePool.deleteInstance(texture);
}

void TextureLoaderAll::onReload(ResourceHandle handle, bx::AllocatorI* alloc)
{

}

