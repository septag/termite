#pragma once

#include "bx/allocator.h"
#include "math.h"
#include "assetlib.h"

// Internal API file
// Only accessed by engine internals
namespace tee {
    struct GfxDriver;
    struct HardwareInfo;

    void platformSetVars(const char* dataDir, const char* cacheDir, const char* version);
    void platformSetHwInfo(const HardwareInfo& hwinfo);
    void platformSetGfxReset(bool gfxReset);
    void platformSetHardwareKey(bool hasHardwareKey);

    bool initJobDispatcher(bx::AllocatorI* alloc,
                           uint16_t maxSmallFibers = 0, uint32_t smallFiberStackSize = 0,
                           uint16_t maxBigFibers = 0, uint32_t bigFiberStackSize = 0,
                           bool lockThreadsToCores = true, uint8_t numThreads = UINT8_MAX);
    void shutdownJobDispatcher();

    bool initPluginSystem(const char* pluginPath, bx::AllocatorI* alloc);
    void shutdownPluginSystem();

    namespace asset {
        bool init(AssetLibInitFlags::Bits flags, IoDriver* driver, bx::AllocatorI* alloc, IoDriver* blockingDriver = nullptr);
        void shutdown();
    }

    namespace sdl {
        bool init(bx::AllocatorI* alloc);
        void shutdown();
    }

    namespace ecs {
        bool init(bx::AllocatorI* alloc);
        void shutdown();
    }

    namespace cmd {
        bool init(uint16_t historySize, bx::AllocatorI* alloc);
        void shutdown();
    }

    namespace err {
        bool init(bx::AllocatorI* alloc);
        void shutdown();
    }

    bool initEventDispatcher(bx::AllocatorI* alloc);
    void shutdownEventDispatcher();
    void runEventDispatcher(float dt);

    namespace gfx {
        bool initDebugDraw(bx::AllocatorI* alloc, GfxDriver* driver);
        void shutdownDebugDraw();

        bool initFontSystem(bx::AllocatorI* alloc, const vec2_t refScreenSize = vec2(-1.0f, -1.0f));
        void shutdownFontSystem();

        bool initFontSystemGraphics();
        void shutdownFontSystemGraphics();
        void registerFontToAssetLib();

        bool initModelLoader(GfxDriver* driver, bx::AllocatorI* alloc);
        void shutdownModelLoader();
        void registerModelToAssetLib();

        void registerSpriteSheetToAssetLib();
        bool initSpriteSystem(GfxDriver* driver, bx::AllocatorI* alloc);
        void shutdownSpriteSystem();

        bool initSpriteSystemGraphics(GfxDriver* driver);
        void shutdownSpriteSystemGraphics();

        bool initTextureLoader(GfxDriver* driver, bx::AllocatorI* alloc, int texturePoolSize = 64,
                               bool enableTextureDecodeCache = true);
        void shutdownTextureLoader();
        void registerTextureToAssetLib();

        bool initGfxUtils(GfxDriver* driver);
        void shutdownGfxUtils();

        bool initDebugDraw2D(bx::AllocatorI* alloc, GfxDriver* driver);
        void shutdownDebugDraw2D();

        bool initMaterialLib(bx::AllocatorI* alloc, GfxDriver* driver);
        void shutdownMaterialLib();
        void destroyMaterialUniforms();
        bool createMaterialUniforms(GfxDriver* driver);
    }

    namespace http {
        bool init(bx::AllocatorI* alloc);
        void shutdown();
        void update();
    }

    namespace lang {
        void registerToAssetLib();
    }

}   // namespace tee

