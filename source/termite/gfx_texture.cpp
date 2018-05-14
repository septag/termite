#include "pch.h"

#include "bxx/pool.h"
#include "bxx/array.h"
#include "bx/math.h"
#include "bx/uint32_t.h"
#include "bx/error.h"
#include "bx/file.h"
#include "bx/hash.h"
#include "bx/mutex.h"

#include "gfx_driver.h"
#include "gfx_texture.h"
#include "job_dispatcher.h"
#include "internal.h"

#include "bimg/decode.h"

//#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize.h"

#include "bxx/linear_allocator.h"
#include "bxx/leakcheck_allocator.h"
#include "bxx/proxy_allocator.h"
#include "bxx/path.h"

#define TEXTURE_CACHE_FILENAME "ttcache.list"

// This (unpack) code is taken from: https://github.com/KhronosGroup/KTX/blob/master/lib/etcunpack.cxx
// fwd declare functions from etcpack
extern void decompressBlockAlphaC(uint8_t* data, uint8_t* img,
                                  int width, int height, int startx, int starty, int channels);
extern void decompressBlockETC2c(unsigned int block_part1, unsigned int block_part2, uint8_t* img,
                                 int width, int height, int startx, int starty, int channels);
extern void setupAlphaTable();

namespace tee {

    class TextureLoaderAll : public AssetLibCallbacksI
    {
    public:
        TextureLoaderAll() {}

        bool loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
        void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
        void onReload(AssetHandle handle, bx::AllocatorI* alloc) override;
    };

    struct TextureCacheItem
    {
        uint32_t nameHash;
        uint32_t dataHash;  // CRC32
    };

    struct SaveTextureCacheJob
    {
        bx::Path uri;
        void* pixelData;
        int width;
        int height;
        int numChannels;    // number of channgels
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
        GfxDriver* driver;
        bx::Array<TextureCacheItem> decodeCacheItems;
        JobHandle saveCacheJobHandle;
        bool enableTextureDecodeCache;
        bool isETC2Supported;

        TextureLoader(bx::AllocatorI* _alloc)
        {
            alloc = _alloc;
            whiteTexture = nullptr;
            asyncBlankTexture = nullptr;
            failTexture = nullptr;
            driver = nullptr;
            enableTextureDecodeCache = false;
            isETC2Supported = false;
            saveCacheJobHandle = nullptr;
        }
    };

    static TextureLoader* gTexLoader = nullptr;

    static void stbCallbackFreeImage(void* ptr, void* userData)
    {
        stbi_image_free(ptr);
    }

    static void loadTextureCacheList(const char* filepath, bx::Array<TextureCacheItem>* items)
    {
        FILE* file = fopen(filepath, "rt");
        if (file) {
            char line[256];

            char filename[128];
            char filehash[128];
            while (fgets(line, sizeof(line), file)) {
                char* sep = strchr(line, ';');
                if (sep) {
                    bx::strCopy(filename, sizeof(filename), line, (int)size_t(sep - line));
                    bx::strCopy(filehash, sizeof(filehash), sep + 1);
                    filehash[strlen(filehash)-1] = 0;   // strip the last character ('\n')

                    /*
                    // strip the extension and keep the hash number only
                    char* dot = strchr(filename, '.');
                    if (dot)
                        *dot = 0;
                        */

                    TextureCacheItem* item = items->push();
                    item->nameHash = uint32_t(bx::toUint(filename));
                    item->dataHash = uint32_t(bx::toUint(filehash));
                }
            }

            fclose(file);
        }
    }

    static void saveTextureCacheList(const char* filepath, const bx::Array<TextureCacheItem>& items)
    {
        FILE* file = fopen(filepath, "wt");
        if (file) {
            for (int i = 0; i < items.getCount(); i++) {
                TextureCacheItem item = items[i];
                fprintf(file, "%u;%u\n", item.nameHash, item.dataHash);
            }
            fclose(file);
        }
    }

    // Returns true if file is changed + update it in the cache db
    // Returns false if file is not changed
    static bool checkTextureCacheChanged(const char* filepath, uint32_t dataHash)
    {
        // Hash the filepath and check for it in the list
        uint32_t nameHash = bx::hash<bx::HashCrc32>(filepath);

        for (int i = 0, c = gTexLoader->decodeCacheItems.getCount(); i < c; i++) {
            if (gTexLoader->decodeCacheItems[i].nameHash != nameHash)
                continue;
            return gTexLoader->decodeCacheItems[i].dataHash != dataHash;
        }

        return true;
    }

    static void updateTextureCacheItem(uint32_t nameHash, uint32_t dataHash)
    {
        for (int i = 0, c = gTexLoader->decodeCacheItems.getCount(); i < c; i++) {
            if (gTexLoader->decodeCacheItems[i].nameHash != nameHash)
                continue;
            gTexLoader->decodeCacheItems[i].dataHash = dataHash;
            return;
        }

        TextureCacheItem* item = gTexLoader->decodeCacheItems.push();
        item->nameHash = nameHash;
        item->dataHash = dataHash;
    }

    static void removeTextureCacheItem(const char* filepath)
    {
        uint32_t nameHash = bx::hash<bx::HashCrc32>(filepath);
        for (int i = 0, c = gTexLoader->decodeCacheItems.getCount(); i < c; i++) {
            if (gTexLoader->decodeCacheItems[i].nameHash != nameHash)
                continue;

            std::swap<TextureCacheItem>(gTexLoader->decodeCacheItems[i],
                                        gTexLoader->decodeCacheItems[gTexLoader->decodeCacheItems.getCount()-1]);
            gTexLoader->decodeCacheItems.pop();
            return;
        }
    }

    static void saveCacheTextureJob(int jobIdx, void* userParam)
    {
        SaveTextureCacheJob* params = (SaveTextureCacheJob*)userParam;
        BX_ASSERT(params, "");

        // Save the file
        const char* cacheDir = getCacheDir();

        // save file as hashed name
        // Save file as PNG
        uint32_t nameHash = bx::hash<bx::HashCrc32>(params->uri.cstr());
        char dbfilename[256];
        bx::snprintf(dbfilename, sizeof(dbfilename), "%x.png", nameHash);
        bx::Path dbfilepath(cacheDir);
        dbfilepath.join(dbfilename);

        stbi_write_png(dbfilepath.cstr(), params->width, params->height, 4, params->pixelData, params->width*4);

        BX_FREE(getHeapAlloc(), params);
    }

    static TextureHandle loadTextureFromCache(const AssetParams& params, GfxDriver* driver)
    {
        const char* cacheDir = getCacheDir();
        bx::FileReader file;
        bx::Error err;
        char cacheTextureName[128];
        uint32_t nameHash = bx::hash<bx::HashCrc32>(params.uri);
        bx::snprintf(cacheTextureName, sizeof(cacheTextureName), "%x.png", nameHash);
        bx::Path cacheTexturePath(cacheDir);
        cacheTexturePath.join(cacheTextureName);
        bx::AllocatorI* alloc = getHeapAlloc();
        if (file.open(cacheTexturePath.cstr(), &err)) {
            uint32_t size = (uint32_t)file.seek(0, bx::Whence::End);
            if (size == 0)
                return TextureHandle();
            void* data = BX_ALLOC(alloc, size);
            if (!data)
                return TextureHandle();

            file.seek(0, bx::Whence::Begin);
            file.read(data, size, &err);
            file.close();

            int width, height, comp;
            uint8_t* pixels = stbi_load_from_memory((const stbi_uc*)data, size, &width, &height, &comp, 0);
            if (!pixels) {
                BX_FREE(alloc, data);
                return TextureHandle();
            }
            BX_FREE(alloc, data);

            return driver->createTexture2D(width, height, false, 1,
                                           TextureFormat::RGBA8, params.flags,
                                           driver->makeRef(pixels, width*height*comp, stbCallbackFreeImage, nullptr));
        }

        return TextureHandle();
    }

    bool gfx::initTextureLoader(GfxDriver* driver, bx::AllocatorI* alloc, int texturePoolSize, bool enableTextureDecodeCache)
    {
        // TODO: REMOVE THIS line later
        enableTextureDecodeCache = false;

        assert(driver);
        if (gTexLoader) {
            assert(false);
            return false;
        }

        TextureLoader* loader = BX_NEW(alloc, TextureLoader)(alloc);
        gTexLoader = loader;
        loader->driver = driver;

        if (!loader->texturePool.create(texturePoolSize, alloc)) {
            return false;
        }

        // Create a default async object (1x1 RGBA white)
        static uint32_t whitePixel = 0xffffffff;
        gTexLoader->whiteTexture = loader->texturePool.newInstance();
        gTexLoader->whiteTexture->handle = driver->createTexture2D(1, 1, false, 1, TextureFormat::RGBA8,
                                                                    TextureFlag::U_Clamp|TextureFlag::V_Clamp|TextureFlag::MinPoint|TextureFlag::MagPoint,
                                                                    driver->makeRef(&whitePixel, sizeof(uint32_t), nullptr, nullptr));
        gTexLoader->whiteTexture->info.width = 1;
        gTexLoader->whiteTexture->info.height = 1;
        gTexLoader->whiteTexture->info.format = TextureFormat::RGBA8;
        if (!gTexLoader->whiteTexture->handle.isValid()) {
            TEE_ERROR("Creating Blank 1x1 texture failed");
            return false;
        }
        gTexLoader->asyncBlankTexture = gTexLoader->whiteTexture;

        static uint32_t blackPixel = 0xff000000;
        gTexLoader->blackTexture = loader->texturePool.newInstance();
        gTexLoader->blackTexture->handle = driver->createTexture2D(1, 1, false, 1, TextureFormat::RGBA8,
                                                                    TextureFlag::U_Clamp|TextureFlag::V_Clamp|TextureFlag::MinPoint|TextureFlag::MagPoint,
                                                                    driver->makeRef(&blackPixel, sizeof(uint32_t), nullptr, nullptr));
        gTexLoader->blackTexture->info.width = 1;
        gTexLoader->blackTexture->info.height = 1;
        gTexLoader->blackTexture->info.format = TextureFormat::RGBA8;
        if (!gTexLoader->blackTexture->handle.isValid()) {
            TEE_ERROR("Creating Blank 1x1 texture failed");
            return false;
        }

        // Fail texture (Checkers 2x2)
        static uint32_t checkerPixels[] = {
            0xff0000ff,
            0xffffffff,
            0xff0000ff,
            0xffffffff
        };
        gTexLoader->failTexture = loader->texturePool.newInstance();
        if (!gTexLoader->failTexture)
            return false;
        gTexLoader->failTexture->handle = driver->createTexture2D(2, 2, false, 1, TextureFormat::RGBA8,
                                                                   TextureFlag::MinPoint | TextureFlag::MagPoint,
                                                                   driver->makeRef(checkerPixels, sizeof(checkerPixels), nullptr, nullptr));
        gTexLoader->failTexture->info.width = 2;
        gTexLoader->failTexture->info.height = 2;
        gTexLoader->failTexture->info.format = TextureFormat::RGBA8;
        if (!gTexLoader->failTexture->handle.isValid()) {
            TEE_ERROR("Creating Fail 2x2 texture failed");
            return false;
        }

        const GfxCaps& caps = driver->getCaps();
        if ((caps.formats[TextureFormat::ETC2] & TextureSupportFlag::Texture2D) &&
            (caps.formats[TextureFormat::ETC2A] & TextureSupportFlag::Texture2D)) 
        {
            gTexLoader->isETC2Supported = true;
        } 

        if (BX_ENABLED(BX_PLATFORM_ANDROID) || BX_ENABLED(BX_PLATFORM_IOS)) {
            if (!gTexLoader->isETC2Supported) {
                BX_WARN("ETC2 formated is not supported in this device. Engine will decode and cache ETC2 textures internally, may cause longer load times");
            }
        }

        gTexLoader->enableTextureDecodeCache = enableTextureDecodeCache;
        if (enableTextureDecodeCache) {
            if (!gTexLoader->decodeCacheItems.create(200, 200, alloc))
                return false;
            // Load cache file
            bx::Path cacheFilepath(getCacheDir());
            cacheFilepath.join(TEXTURE_CACHE_FILENAME);
            loadTextureCacheList(cacheFilepath.cstr(), &gTexLoader->decodeCacheItems);
        }

        return true;
    }

    void gfx::registerTextureToAssetLib()
    {
        AssetTypeHandle handle;
        handle = asset::registerType("texture", &gTexLoader->loader, sizeof(LoadTextureParams),
                                      uintptr_t(gTexLoader->failTexture), uintptr_t(gTexLoader->asyncBlankTexture));
        assert(handle.isValid());
    }

    void gfx::shutdownTextureLoader()
    {
        if (!gTexLoader)
            return;

        if (gTexLoader->enableTextureDecodeCache) {
            // Save cache file
            bx::Path cacheFilepath(getCacheDir());
            cacheFilepath.join(TEXTURE_CACHE_FILENAME);
            saveTextureCacheList(cacheFilepath.cstr(), gTexLoader->decodeCacheItems);

            gTexLoader->decodeCacheItems.destroy();
        }

        if (gTexLoader->whiteTexture) {
            if (gTexLoader->whiteTexture->handle.isValid())
                gTexLoader->driver->destroyTexture(gTexLoader->whiteTexture->handle);
        }

        if (gTexLoader->blackTexture) {
            if (gTexLoader->blackTexture->handle.isValid())
                gTexLoader->driver->destroyTexture(gTexLoader->blackTexture->handle);
        }

        if (gTexLoader->failTexture) {
            if (gTexLoader->failTexture->handle.isValid())
                gTexLoader->driver->destroyTexture(gTexLoader->failTexture->handle);
        }

        gTexLoader->texturePool.destroy();

        BX_DELETE(gTexLoader->alloc, gTexLoader);
        gTexLoader = nullptr;
    }

    TextureHandle gfx::getWhiteTexture1x1()
    {
        return gTexLoader->whiteTexture->handle;
    }

    TextureHandle gfx::getBlackTexture1x1()
    {
        return gTexLoader->blackTexture->handle;
    }

    bool gfx::blitRawPixels(uint8_t* dest, int destX, int destY, int destWidth, int destHeight, const uint8_t* src,
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

    void gfx::saveTextureCache()
    {
        if (gTexLoader && gTexLoader->enableTextureDecodeCache) {
            // Save cache file
            bx::Path cacheFilepath(getCacheDir());
            cacheFilepath.join(TEXTURE_CACHE_FILENAME);
            saveTextureCacheList(cacheFilepath.cstr(), gTexLoader->decodeCacheItems);
        }
    }

    static bool loadUncompressed(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
    {
        assert(gTexLoader);
        GfxDriver* driver = gTexLoader->driver;
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
            texture = gTexLoader->texturePool.newInstance();
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
            numMips = 1 + (int)bx::floor(bx::log2((float)bx::uint32_max(width, height)));
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
                    gTexLoader->texturePool.deleteInstance(texture);
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
            mipWidth = std::max<int>(1, width >> 1);
            mipHeight = std::max<int>(1, height >> 1);
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

        texture->handle = driver->createTexture2D(width, height, texParams->generateMips, 1, fmt, texParams->flags, gmem);
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

    inline void readBigEndian4byteWord(uint32_t* pBlock, const uint8_t* s)
    {
        *pBlock = (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
    }

    static void* decodeETC2(bx::AllocatorI* alloc, const void* etc2Blocks, TextureFormat::Enum etc2Fmt,
                            uint16_t width, uint16_t height, uint32_t* outSize)
    {
        uint16_t w = ((width + 3)/4)*4;
        uint16_t h = ((height + 3)/4)*4;

        const int bpp = 4;  // bytes per pixel
        uint32_t block1, block2;
        uint8_t* inData = (uint8_t*)etc2Blocks;
        uint8_t* outData = (uint8_t*)BX_ALLOC(alloc, w*h*bpp);
        if (!outData)
            return nullptr;

        if (etc2Fmt == TextureFormat::ETC2A) {
            setupAlphaTable();
            for (int y = 0; y < h/4; y++) {
                for (int x = 0; x < w/4; x++) {
                    // EAC block + ETC2 RGB block
                    decompressBlockAlphaC(inData, outData+3, w, h, 4*x, 4*y, 4);
                    inData += 8;
                    readBigEndian4byteWord(&block1, inData);
                    inData += 4;
                    readBigEndian4byteWord(&block2, inData);
                    inData += 4;
                    decompressBlockETC2c(block1, block2, outData, w, h, 4*x, 4*y, 4);
                }
            }
        } else {
            assert(0);  // unsupported format
            return nullptr;
        }

        // Write out actual pixels, if width != actualWidth or height != actualHeight
        if (w != width || h != height) {
            int destRow = 4 * w;
            int actualRow = 4 * width;

            uint8_t* actualData = (uint8_t*)BX_ALLOC(alloc, 4 * width * height);
            if (!actualData) {
                BX_FREE(alloc, outData);
                return nullptr;
            }
            for (uint32_t yy = 0; yy < height; yy++) {
                for (uint32_t xx = 0; xx < width; xx++) {
                    *((uint32_t*)(actualData + yy*actualRow + xx*4)) = *((uint32_t*)(outData + yy*destRow + xx*4));
                }
            }

            BX_FREE(alloc, outData);
            outData = actualData;
        }

        *outSize = w*h*bpp;
        return outData;
    }

    static bool loadCompressed(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
    {
        const LoadTextureParams* texParams = (const LoadTextureParams*)params.userParams;

        Texture* texture;
        if (alloc)
            texture = BX_NEW(alloc, Texture)();
        else
            texture = gTexLoader->texturePool.newInstance();
        if (!texture)
            return false;

        GfxDriver* driver = gTexLoader->driver;
        bimg::ImageContainer imgInfo;
        bx::Error err;
        if (!bimg::imageParse(imgInfo, mem->data, mem->size, &err)) {
            return false;
        }

        // verify format compatibility, decoded if the device does not support the format
        bool formatIsETC2 = (imgInfo.m_format == bimg::TextureFormat::ETC2) ||
            (imgInfo.m_format == bimg::TextureFormat::ETC2A) ||
            (imgInfo.m_format == bimg::TextureFormat::ETC2A1);
        bool isFormatSupported = true;
        if (formatIsETC2 && !gTexLoader->isETC2Supported)
            isFormatSupported = false;

        // 
        bimg::ImageContainer* img = bimg::imageParse(getHeapAlloc(), mem->data, mem->size);
        if (!img || !img->m_data)
            return false;

        texture->info.width = imgInfo.m_width;
        texture->info.height = imgInfo.m_height;
        texture->info.format = (TextureFormat::Enum)imgInfo.m_format;
        texture->info.numMips = imgInfo.m_numMips;
        texture->info.storageSize = img->m_size;
        texture->info.depth = img->m_depth;
        texture->info.cubeMap = img->m_cubeMap;
        texture->info.bitsPerPixel = bimg::getBitsPerPixel(imgInfo.m_format);
        texture->ratio = float(imgInfo.m_width) / float(imgInfo.m_height);

        if (isFormatSupported) {
            // TODO: support Cube/3D textures
            uint32_t size = img->m_size;
            assert(img->m_depth == 1 && !img->m_cubeMap);
            texture->handle = driver->createTexture2D(img->m_width, img->m_height, img->m_numMips>1, img->m_numLayers,
                (TextureFormat::Enum)img->m_format, texParams->flags, driver->makeRef(img->m_data, img->m_size, [](void* ptr, void* userData) {
                bimg::ImageContainer* img = (bimg::ImageContainer*)userData;
                bimg::imageFree(img);
            }, img));
        } else {
            bool decode = true;
            uint32_t dataHash = 0;
            if (gTexLoader->enableTextureDecodeCache) {
                dataHash = bx::hash<bx::HashCrc32>(mem->data, mem->size);
                decode = checkTextureCacheChanged(params.uri, dataHash);
            }

            if (decode) {
                // Decode the ETC2 image data to RGBA8
                uint32_t decodedSize;
                void* decoded = nullptr;
                if (formatIsETC2) {
                    decoded = decodeETC2(getHeapAlloc(), img->m_data, (TextureFormat::Enum)img->m_format,
                                         img->m_width, img->m_height, &decodedSize);
                } else {
                    BX_ASSERT(false, "Software Decoding format is not supported");
                }

                if (decoded) {
                    // First save it in the cache
                    if (gTexLoader->enableTextureDecodeCache) {
                        SaveTextureCacheJob* cacheJob = (SaveTextureCacheJob*)BX_ALLOC(getHeapAlloc(),
                                                                                       sizeof(SaveTextureCacheJob) + decodedSize);
                        cacheJob->uri = params.uri;
                        cacheJob->pixelData = cacheJob + 1;
                        cacheJob->width = img->m_width;
                        cacheJob->height = img->m_height;
                        cacheJob->numChannels = 4;
                        memcpy(cacheJob->pixelData, decoded, decodedSize);

                        JobDesc j(saveCacheTextureJob, cacheJob, JobPriority::Low);
                        if (gTexLoader->saveCacheJobHandle)
                            waitAndDeleteJob(gTexLoader->saveCacheJobHandle);
                        gTexLoader->saveCacheJobHandle = dispatchBigJobs(&j, 1);

                        if (gTexLoader->saveCacheJobHandle) {
                            updateTextureCacheItem(bx::hash<bx::HashCrc32>(params.uri), dataHash);
                        } else {
                            BX_WARN("SaveCacheJob Error");
                        }
                    }

                    texture->handle = driver->createTexture2D(img->m_width, img->m_height, img->m_numMips>1, img->m_numLayers,
                                                              TextureFormat::RGBA8, texParams->flags,
                                                              driver->makeRef(decoded, decodedSize, [](void* ptr, void* userData) {
                        BX_FREE(getHeapAlloc(), ptr);
                    }, nullptr));
                }
            } else {
                // Try to open the decoded file from cache
                texture->handle = loadTextureFromCache(params, driver);
                if (!texture->handle.isValid()) {
                    // Some error occured on opening cached file, remove it from cache db
                    removeTextureCacheItem(params.uri);
                }
            }

            bimg::imageFree(img);
        }
        if (!texture->handle.isValid()) {
            if (alloc)
                BX_DELETE(alloc, texture);
            else
                gTexLoader->texturePool.deleteInstance(texture);
            return false;
        }
        *obj = uintptr_t(texture);
        return true;
    }


    bool TextureLoaderAll::loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj,
                                   bx::AllocatorI* alloc)
    {
        bx::Path path(params.uri);
        bx::Path ext = path.getFileExt();
        ext.toLower();
        // strip LZ4 from extension
        char* lz4Ext = strstr(ext.getBuffer(), ".lz4");
        if (lz4Ext)
            *lz4Ext = 0;

        if (ext.isEqual("ktx") || ext.isEqual("dds") || ext.isEqual("pvr")) {
            return loadCompressed(mem, params, obj, alloc);
        } else if (ext.isEqual("png") || ext.isEqual("tga") || ext.isEqual("jpg") || ext.isEqual("bmp") ||
                   ext.isEqual("jpeg") || ext.isEqual("psd") || ext.isEqual("hdr") || ext.isEqual("gif")) {
            return loadUncompressed(mem, params, obj, alloc);
        } else {
            return false;
        }
    }

    void TextureLoaderAll::unloadObj(uintptr_t obj, bx::AllocatorI* alloc)
    {
        assert(gTexLoader);
        assert(obj);

        Texture* texture = (Texture*)obj;
        if (texture->handle.isValid())
            gTexLoader->driver->destroyTexture(texture->handle);

        if (alloc)
            BX_DELETE(alloc, texture);
        else
            gTexLoader->texturePool.deleteInstance(texture);
    }

    void TextureLoaderAll::onReload(AssetHandle handle, bx::AllocatorI* alloc)
    {

    }

} // namespace tee

