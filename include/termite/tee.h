#pragma once

#include <assert.h>
#include <math.h>

// Stuff we need from stl
#include <algorithm>
#include <functional>

#include "bx/allocator.h"
#include "bxx/path.h"
#include "tinystl/hash.h"

// Windows
#if BX_PLATFORM_WINDOWS
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#endif

#include "types.h"
#include "error_report.h"
#include "tmath.h"

#include "gfx_defines.h"
#include "sound_driver.h"

#define TEE_MEMID_TEMP 0x666ce76b992f595e 

#if BX_PLATFORM_ANDROID
#include <jni.h>
#endif

namespace tee
{
    struct GfxPlatformData;
    class AssetLib;
    struct GfxDriver;
    struct IoDriver;
    struct RendererApi;
    struct PhysDriver2D;

    struct InitEngineFlags
    {
        enum Enum
        {
            None = 0,
            EnableJobDispatcher = 0x1,
            LockThreadsToCores = 0x2
        };

        typedef uint8_t Bits;
    };

    struct Config
    {
        // Plugins
        bx::Path pluginPath;
        bx::Path dataUri;

        bx::String32 ioName;
        bx::String32 rendererName;
        bx::String32 gfxName;
        bx::String32 uiIniFilename;
        bx::String32 phys2dName;    // Physics2D Driver name
        bx::String32 soundName;     // Sound driver name

        // 
        uint16_t refScreenWidth;
        uint16_t refScreenHeight;

        // Graphics
        uint16_t gfxDeviceId;
        uint16_t gfxWidth;
        uint16_t gfxHeight;
        GfxResetFlag::Bits gfxDriverFlags; 
        int keymap[19];

        // Sound
        AudioFreq::Enum audioFreq;
        AudioChannels::Enum audioChannels;
        int audioBufferSize;

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
            // Default drivers
            gfxName = "Bgfx";
            soundName = "SDL_mixer";
            phys2dName = "Box2D";
            ioName = "DiskIO_Lite";
            uiIniFilename = "termite_imgui.ini";

            refScreenWidth = 0;
            refScreenHeight = 0;

            gfxWidth = 0;
            gfxHeight = 0;
            gfxDeviceId = 0;
            gfxDriverFlags = BX_ENABLED(BX_PLATFORM_IOS) ? GfxResetFlag::HiDPi : 0;
            bx::memSet(keymap, 0x00, sizeof(keymap));

            audioFreq = AudioFreq::Freq22Khz;
            audioChannels = AudioChannels::Mono;
            audioBufferSize = 4096;

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

    struct HardwareInfo
    {
        char brand[16];
        char model[16];
        char uniqueId[32];
        uint64_t totalMem;
        int apiVersion;
        uint16_t numCores;

        HardwareInfo()
        {
            brand[0] = 0;
            model[0] = 0;
            uniqueId[0] = 0;
            totalMem = 0;
            apiVersion = 0;
            numCores = 0;
        }
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
            m_accum(0),
            m_timestep(timestep)
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
        float m_accum;
        float m_timestep;
    };

    // Public
    TEE_API bool init(const Config& conf, UpdateCallback updateFn = nullptr, const GfxPlatformData* platformData = nullptr);

    // Note: User Shutdown happens before IO and memory stuff
    //       In order for user to clean-up any memory or save stuff
    TEE_API void shutdown(ShutdownCallback callback = nullptr, void* userData = nullptr);
    TEE_API void doFrame();
    TEE_API void pause();
    TEE_API void resume();
    TEE_API bool isPaused();
    TEE_API void resetTempAlloc();
    TEE_API void resetBackbuffer(uint16_t width, uint16_t height);

    TEE_API double getFrameTime();
    TEE_API double getElapsedTime();
    TEE_API double getFps();
    TEE_API double getSmoothFrameTime();
    TEE_API uint64_t getFrameIndex();

    TEE_API MemoryBlock* createMemoryBlock(uint32_t size, bx::AllocatorI* alloc = nullptr);
    TEE_API MemoryBlock* refMemoryBlockPtr(const void* data, uint32_t size);
    TEE_API MemoryBlock* refMemoryBlock(MemoryBlock* mem);
    TEE_API MemoryBlock* copyMemoryBlock(const void* data, uint32_t size, bx::AllocatorI* alloc = nullptr);
    TEE_API void releaseMemoryBlock(MemoryBlock* mem);

    TEE_API MemoryBlock* readTextFile(const char* absFilepath);
    TEE_API MemoryBlock* readBinaryFile(const char* absFilepath);
    TEE_API bool saveBinaryFile(const char* absFilepath, const MemoryBlock* mem);

    TEE_API MemoryBlock* encryptMemoryAES128(const MemoryBlock* mem, bx::AllocatorI* alloc = nullptr, 
                                                const uint8_t* key = nullptr, const uint8_t* iv = nullptr);
    TEE_API MemoryBlock* decryptMemoryAES128(const MemoryBlock* mem, bx::AllocatorI* alloc = nullptr, 
                                                const uint8_t* key = nullptr, const uint8_t* iv = nullptr);
    TEE_API void cipherXOR(uint8_t* outputBuff, const uint8_t* inputBuff, size_t buffSize, const uint8_t* key, size_t keySize);

    TEE_API void restartRandom();
    TEE_API float getRandomFloatUniform(float a, float b);
    TEE_API int getRandomIntUniform(int a, int b);    
    TEE_API float getRandomFloatNormal(float mean, float sigma);

    // UI Input
    TEE_API void inputSendMouse(float mousePos[2], int mouseButtons[3], float mouseWheel);
    TEE_API void inputSendChars(const char* chars);
    TEE_API void inputSendKeys(const bool keysDown[512], bool shift, bool alt, bool ctrl);

    // Development
    TEE_API GfxDriver* getGfxDriver() TEE_THREAD_SAFE;
    TEE_API IoDriver* getBlockingIoDriver() TEE_THREAD_SAFE;
    TEE_API IoDriver* getAsyncIoDriver() TEE_THREAD_SAFE;
    TEE_API RendererApi* getRenderer() TEE_THREAD_SAFE;
    TEE_API SimpleSoundDriver* getSoundDriver() TEE_THREAD_SAFE;
    TEE_API PhysDriver2D* getPhys2dDriver() TEE_THREAD_SAFE;
    TEE_API uint32_t getEngineVersion() TEE_THREAD_SAFE;
    TEE_API bx::AllocatorI* getHeapAlloc() TEE_THREAD_SAFE;
    TEE_API bx::AllocatorI* getTempAlloc() TEE_THREAD_SAFE;
    TEE_API const Config& getConfig() TEE_THREAD_SAFE;
    TEE_API Config* getMutableConfig() TEE_THREAD_SAFE;
    TEE_API const char* getCacheDir() TEE_THREAD_SAFE;
    TEE_API const char* getDataDir() TEE_THREAD_SAFE;
    TEE_API void dumpGfxLog() TEE_THREAD_SAFE;
    TEE_API bool needGfxReset() TEE_THREAD_SAFE;

    TEE_API void shutdownGraphics();
    TEE_API bool resetGraphics(const GfxPlatformData* platform);

    // Remote Console
    TEE_API void registerConsoleCommand(const char* name, std::function<void(int, const char**)> callback);

    TEE_API const HardwareInfo& getHardwareInfo();

#if BX_PLATFORM_ANDROID
    struct JavaMethod
    {
        JNIEnv* env;
        jclass cls;
        jobject obj;
        jmethodID methodId;
    };

    struct JavaMethodType
    {
        enum Enum {
            Method,
            StaticMethod
        };
    };

    // Calls a method in java
    // for classPath, methodName and methodSig parameters, see:
    //      http://journals.ecs.soton.ac.uk/java/tutorial/native1.1/implementing/method.html
    TEE_API JavaMethod androidFindMethod(const char* methodName, const char* methodSig, const char* classPath = nullptr,
                                             JavaMethodType::Enum type = JavaMethodType::Method);
#endif

} // namespace tee

