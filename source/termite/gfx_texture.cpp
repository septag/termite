#include "pch.h"

#include "bxx/pool.h"
#include "bxx/array.h"
#include "bx/fpumath.h"
#include "bx/uint32_t.h"

#include "gfx_driver.h"
#include "gfx_texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize.h"

using namespace termite;

// JPG, PNG, TGA, ...
class TextureLoaderRaw : public dsResourceCallbacksI
{
public:
    TextureLoaderRaw();

    bool loadObj(const MemoryBlock* mem, const dsResourceTypeParams& params, uintptr_t* obj) override;
    void unloadObj(uintptr_t obj) override;
    void onReload(dsDataStore* ds, dsResourceHandle handle) override;
    uintptr_t getDefaultAsyncObj() override;
};

// KTX/DDS loader
class TextureLoaderKTX : public dsResourceCallbacksI
{
public:
    TextureLoaderKTX();

    bool loadObj(const MemoryBlock* mem, const dsResourceTypeParams& params, uintptr_t* obj) override;
    void unloadObj(uintptr_t obj) override;
    void onReload(dsDataStore* ds, dsResourceHandle handle) override;
    uintptr_t getDefaultAsyncObj() override;
};

struct TextureLoader
{
    bx::Pool<gfxTexture> texturePool;
    TextureLoaderRaw rawLoader;
    TextureLoaderKTX loader;
    bx::AllocatorI* alloc;
    gfxTexture* whiteTexture;
    gfxTexture* asyncBlankTexture;
    gfxDriverI* driver;

    TextureLoader(bx::AllocatorI* _alloc)
    {
        alloc = _alloc;
        whiteTexture = nullptr;
        asyncBlankTexture = nullptr;
        driver = nullptr;
    }
};

static TextureLoader* g_texLoader = nullptr;

result_t termite::textureInitLoader(gfxDriverI* driver, bx::AllocatorI* alloc, int texturePoolSize)
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
    g_texLoader->whiteTexture = loader->texturePool.newInstance();
    if (!g_texLoader->whiteTexture)
        return T_ERR_OUTOFMEM;

    static uint32_t whitePixel = 0xffffffff;
    g_texLoader->whiteTexture->handle = driver->createTexture2D(1, 1, 1, gfxTextureFormat::RGBA8,
        gfxTextureFlag::U_Clamp|gfxTextureFlag::V_Clamp|gfxTextureFlag::MinPoint|gfxTextureFlag::MagPoint,
        driver->makeRef(&whitePixel, sizeof(uint32_t)));
    g_texLoader->whiteTexture->info.width = 1;
    g_texLoader->whiteTexture->info.height = 1;
    g_texLoader->whiteTexture->info.format = gfxTextureFormat::RGBA8;
    if (!T_ISVALID(g_texLoader->whiteTexture->handle)) {
        T_ERROR("Creating blank 1x1 texture failed");
        return T_ERR_FAILED;
    }
    g_texLoader->asyncBlankTexture = g_texLoader->whiteTexture;

    return T_OK;
}

void termite::textureRegisterToDatastore(dsDataStore* ds)
{
    dsResourceTypeHandle handle;
    handle = dsRegisterResourceType(ds, "image", &g_texLoader->rawLoader, sizeof(gfxTextureLoadParams));
    assert(T_ISVALID(handle));

    handle = dsRegisterResourceType(ds, "texture", &g_texLoader->loader, sizeof(gfxTextureLoadParams));
    assert(T_ISVALID(handle));
}

void termite::textureShutdownLoader()
{
    if (!g_texLoader)
        return;
    
    if (g_texLoader->whiteTexture) {
        if (T_ISVALID(g_texLoader->whiteTexture->handle))
            g_texLoader->driver->destroyTexture(g_texLoader->whiteTexture->handle);
    }

    g_texLoader->texturePool.destroy();

    BX_DELETE(g_texLoader->alloc, g_texLoader);
    g_texLoader = nullptr;
}

gfxTextureHandle termite::textureGetWhite1x1()
{
    return g_texLoader->whiteTexture->handle;
}

TextureLoaderRaw::TextureLoaderRaw()
{

}

static void stbCallbackFreeImage(void* ptr, void* userData)
{
    stbi_image_free(ptr);
}

bool TextureLoaderRaw::loadObj(const MemoryBlock* mem, const dsResourceTypeParams& params, uintptr_t* obj)
{
    assert(g_texLoader);
    gfxDriverI* driver = g_texLoader->driver;

    // Note: Currently we always load RGBA8 format for non-compressed textures
    int width, height, comp;
    uint8_t* pixels = stbi_load_from_memory(mem->data, mem->size, &width, &height, &comp, 4);
    if (!pixels)
        return false;
    
    gfxTexture* texture = g_texLoader->texturePool.newInstance();
    if (!texture) {
        stbi_image_free(pixels);
        return false;
    }

    // Generate mips
    int numMips = 1;    // default
    const gfxTextureLoadParams* texParams = (const gfxTextureLoadParams*)params.userParams;
    const gfxMemory* gmem = nullptr;
    if (texParams->generateMips) {
        numMips = 1 + (int)bx::ffloor(bx::flog2((float)bx::uint32_max(width, height)));
        int skipMips = bx::uint32_min(numMips - 1, texParams->skipMips);
        int origWidth = width, origHeight = height;

        // We have the first mip, calculate image size
        uint32_t sizeBytes = 0;
        int mipWidth = width, mipHeight = height;
        for (int i = 0; i < numMips; i++) {
            if (i >= skipMips) {
                sizeBytes += mipWidth*mipHeight * 4;
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
            g_texLoader->texturePool.deleteInstance(texture);
            return false;
        }

        uint8_t* srcPixels = gmem->data;
        if (skipMips > 0) {
            stbir_resize_uint8(pixels, origWidth, origHeight, 0, srcPixels, width, height, 0, 4);
        } else {
            memcpy(srcPixels, pixels, width*height * 4);
        }
        stbi_image_free(pixels);

        int srcWidth = width, srcHeight = height;
        mipWidth = width >> 1;   
        mipHeight = height >> 1;
        for (int i = 1; i < numMips; i++) {
            uint8_t* destPixels = srcPixels + srcWidth*srcHeight*4;

            stbir_resize_uint8(srcPixels, srcWidth, srcHeight, 0, destPixels, mipWidth, mipHeight, 0, 4);

            srcWidth = mipWidth;
            srcHeight = mipHeight;
            srcPixels = destPixels;

            mipWidth = bx::uint32_max(1, mipWidth >> 1);
            mipHeight = bx::uint32_max(1, mipHeight >> 1);
        }        
    } else {
        gmem = driver->makeRef(pixels, width*height * 4, stbCallbackFreeImage, nullptr);
    }

    texture->handle = driver->createTexture2D(width, height, numMips, gfxTextureFormat::RGBA8, texParams->flags, gmem);
    if (!T_ISVALID(texture->handle))
        return false;

    gfxTextureInfo* info = &texture->info;
    info->width = width;
    info->height = height;
    info->format = gfxTextureFormat::RGBA8;
    info->numMips = 1;
    info->storageSize = width * height * 4;
    info->bitsPerPixel = 32;

    *obj = uintptr_t(texture);
    return true;
}

void TextureLoaderRaw::unloadObj(uintptr_t obj)
{
    assert(g_texLoader);
    assert(obj);

    gfxTexture* texture = (gfxTexture*)obj;
    if (texture == g_texLoader->asyncBlankTexture)
        return;

    if (T_ISVALID(texture->handle))
        g_texLoader->driver->destroyTexture(texture->handle);

    g_texLoader->texturePool.deleteInstance(texture);
}

void TextureLoaderRaw::onReload(dsDataStore* ds, dsResourceHandle handle)
{
}

uintptr_t TextureLoaderRaw::getDefaultAsyncObj()
{
    return uintptr_t(g_texLoader->asyncBlankTexture);
}


TextureLoaderKTX::TextureLoaderKTX()
{

}

bool TextureLoaderKTX::loadObj(const MemoryBlock* mem, const dsResourceTypeParams& params, uintptr_t* obj)
{
    const gfxTextureLoadParams* texParams = (const gfxTextureLoadParams*)params.userParams;
    
    gfxTexture* texture = g_texLoader->texturePool.newInstance();
    if (!texture)
        return false;

    gfxDriverI* driver = g_texLoader->driver;
    texture->handle = driver->createTexture(driver->copy(mem->data, mem->size), texParams->flags, texParams->skipMips,
                                            &texture->info);
    if (!T_ISVALID(texture->handle))
        return false;

    *obj = uintptr_t(texture);
    return true;
}

void TextureLoaderKTX::unloadObj(uintptr_t obj)
{
    assert(g_texLoader);
    assert(obj);

    gfxTexture* texture = (gfxTexture*)obj;
    if (texture == g_texLoader->asyncBlankTexture)
        return;

    if (T_ISVALID(texture->handle))
        g_texLoader->driver->destroyTexture(texture->handle);

    g_texLoader->texturePool.deleteInstance(texture);
}

void TextureLoaderKTX::onReload(dsDataStore* ds, dsResourceHandle handle)
{
}

uintptr_t TextureLoaderKTX::getDefaultAsyncObj()
{
    return uintptr_t(g_texLoader->asyncBlankTexture);
}
