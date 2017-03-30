#pragma once

#include <cassert>

// Stuff we need from stl
#include <algorithm>
#include <functional>

#include "bx/allocator.h"
#include "tinystl/hash.h"

// Windows
#if BX_PLATFORM_WINDOWS
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#endif

#include "types.h"
#include "error_report.h"

// For self-documenting code
#define T_THREAD_SAFE

#define T_MID_TEMP 0x1fece76b992f595e 

namespace termite
{
    struct GfxPlatformData;
    class ResourceLib;
    struct GfxDriverApi;
    struct IoDriverApi;
    struct RendererApi;
    struct PhysDriver2DApi;

    struct InitEngineFlags
    {
        enum Enum
        {
            None = 0,
            EnableJobDispatcher = 0x1,
            LockThreadsToCores = 0x2,
            ScanFontsDirectory = 0x4
        };

        typedef uint8_t Bits;
    };

    struct Config
    {
        // Plugins
        char pluginPath[256];
        char dataUri[256];

        char ioName[32];
        char rendererName[32];
        char gfxName[32];
        char uiIniFilename[32];
        char phys2dName[32];    // Physics2D Driver name

        // Graphics
        uint16_t gfxDeviceId;
        uint16_t gfxWidth;
        uint16_t gfxHeight;
        uint32_t gfxDriverFlags; 
        int keymap[19];

        // Job Dispatcher
        uint16_t maxSmallFibers;
        uint16_t smallFiberSize;    // in Kb
        uint16_t maxBigFibers;
        uint16_t bigFiberSize;      // in Kb
        uint8_t numWorkerThreads;
        InitEngineFlags::Bits engineFlags;

        // Memory
        uint32_t pageSize;          // in Kb
        int maxPagesPerPool;

        // Developer
        uint16_t cmdHistorySize;

        Config()
        {
            strcpy(pluginPath, "");
            strcpy(rendererName, "");
            strcpy(ioName, "");
            strcpy(dataUri, "");
            strcpy(gfxName, "Bgfx");
            strcpy(uiIniFilename, "");
            strcpy(phys2dName, "Box2D");

            gfxWidth = 0;
            gfxHeight = 0;
            gfxDeviceId = 0;
            gfxDriverFlags = 0;
            memset(keymap, 0x00, sizeof(keymap));

            maxSmallFibers = maxBigFibers = 0;
            smallFiberSize = bigFiberSize = 0;
            numWorkerThreads = UINT8_MAX;
            engineFlags = InitEngineFlags::EnableJobDispatcher;

            pageSize = 0;
            maxPagesPerPool = 0;
            cmdHistorySize = 32;
        }
    };

    struct MemoryBlock
    {
        uint8_t* data;
        uint32_t size;
    };

    typedef void(*UpdateCallback)(float dt);
    typedef void(*ShutdownCallback)(void* userData);
    typedef void(*FixedUpdateCallback)(float dt, void* userData);

    ///
    /// TimeStep: Used to call an update function in fixed intervals
    ///           mainly used for physics
    class TimeStepper
    {
    public:
        explicit TimeStepper(float timestep) :
            m_callback(nullptr),
            m_timestep(timestep),
            m_accum(0)
        {
        }

        float step(float dt, FixedUpdateCallback callback, void* userData)
        {
            const float timestep = m_timestep;
            float accum = m_accum + dt;
            while (accum >= timestep) {
                callback(timestep, userData);
                accum -= timestep;
            }
            m_accum = accum;
            return accum / timestep;
        }

    private:
        FixedUpdateCallback m_callback;
        float m_accum;
        float m_timestep;
    };

    // Public
    TERMITE_API Config* loadConfig(const char* confFilepath);
    TERMITE_API void freeConfig(Config* conf);

    TERMITE_API result_t initialize(const Config& conf, UpdateCallback updateFn = nullptr, 
                                    const GfxPlatformData* platformData = nullptr);

    // Note: User Shutdown happens before IO and memory stuff
    //       In order for user to clean-up any memory or save stuff
    TERMITE_API void shutdown(ShutdownCallback callback = nullptr, void* userData = nullptr);
    TERMITE_API void doFrame();
    TERMITE_API void pause();
    TERMITE_API void resume();
    TERMITE_API bool isPaused();
    TERMITE_API void resetTempAlloc();

    TERMITE_API double getFrameTime();
    TERMITE_API double getElapsedTime();
    TERMITE_API double getFps();
    TERMITE_API double getSmoothFrameTime();
    TERMITE_API int64_t getFrameIndex();

    TERMITE_API MemoryBlock* createMemoryBlock(uint32_t size, bx::AllocatorI* alloc = nullptr);
    TERMITE_API MemoryBlock* refMemoryBlockPtr(const void* data, uint32_t size);
    TERMITE_API MemoryBlock* refMemoryBlock(MemoryBlock* mem);
    TERMITE_API MemoryBlock* copyMemoryBlock(const void* data, uint32_t size, bx::AllocatorI* alloc = nullptr);
    TERMITE_API void releaseMemoryBlock(MemoryBlock* mem);
    TERMITE_API MemoryBlock* readTextFile(const char* filepath);

    TERMITE_API float getRandomFloatUniform(float a, float b);
    TERMITE_API int getRandomIntUniform(int a, int b);    
    TERMITE_API float getRandomFloatNormal(float mean, float sigma);

    // UI Input
    TERMITE_API void inputSendMouse(float mousePos[2], int mouseButtons[3], float mouseWheel);
    TERMITE_API void inputSendChars(const char* chars);
    TERMITE_API void inputSendKeys(const bool keysDown[512], bool shift, bool alt, bool ctrl);

    // Development
    TERMITE_API GfxDriverApi* getGfxDriver() T_THREAD_SAFE;
    TERMITE_API IoDriverApi* getBlockingIoDriver() T_THREAD_SAFE;
    TERMITE_API IoDriverApi* getAsyncIoDriver() T_THREAD_SAFE;
    TERMITE_API RendererApi* getRenderer() T_THREAD_SAFE;
    TERMITE_API PhysDriver2DApi* getPhys2dDriver() T_THREAD_SAFE;
    TERMITE_API uint32_t getEngineVersion() T_THREAD_SAFE;
    TERMITE_API bx::AllocatorI* getHeapAlloc() T_THREAD_SAFE;
    TERMITE_API bx::AllocatorI* getTempAlloc() T_THREAD_SAFE;
    TERMITE_API const Config& getConfig() T_THREAD_SAFE;
    TERMITE_API const char* getCacheDir() T_THREAD_SAFE;
    TERMITE_API const char* getDataDir() T_THREAD_SAFE;
    TERMITE_API void dumpGfxLog() T_THREAD_SAFE;
} // namespace termite


