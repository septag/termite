#pragma once

#include <cassert>
#include "bx/bx.h"
#include "bx/allocator.h"
#include "bxx/stack_allocator.h"
#include "bxx/bitmask_operators.hpp"

// Windows
#if BX_PLATFORM_WINDOWS
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#endif

// Export/Import API Def
#ifdef termite_SHARED_LIB
#ifdef termite_EXPORTS
#   if BX_COMPILER_MSVC
#       define TERMITE_API __declspec(dllexport) 
#   else
#       define TERMITE_API __attribute__ ((visibility("default")))
#   endif
#else
#   if BX_COMPILER_MSVC
#       define TERMITE_API __declspec(dllimport)
#   else
#       define TERMITE_API __attribute__ ((visibility("default")))
#   endif
#endif
#else
#   define TERMITE_API 
#endif

#include "error_report.h"

// Versioning Macros
#define T_MAKE_VERSION(_Major, _Minor)  (uint32_t)(((_Major & 0xffff)<<16) | (_Minor & 0xffff))
#define T_VERSION_MAJOR(_Ver) (uint16_t)((_Ver >> 16) & 0xffff)
#define T_VERSION_MINOR(_Ver) (uint16_t)(_Ver & 0xffff)

// Handles
#define T_HANDLE(_Name) struct _Name { uint16_t idx; }
namespace termite { 
    static const uint16_t sInvalidHandle = UINT16_MAX; 
}
#define T_INVALID_HANDLE  {termite::sInvalidHandle}
#define T_ISVALID(_Handle) (_Handle.idx != termite::sInvalidHandle)

// For self-documenting code
#define T_THREAD_SAFE

namespace termite
{
    struct gfxPlatformData;
    class dsDataStore;

    struct coreConfig
    {
        // Plugins
        char pluginPath[128];

        // Graphics
        uint16_t gfxDeviceId;
        uint16_t gfxWidth;
        uint16_t gfxHeight;
        uint32_t gfxDriverFlags;    // see gfxResetFlag
        int keymap[19];

        coreConfig()
        {
            strcpy(pluginPath, "");
            gfxWidth = 0;
            gfxHeight = 0;
            gfxDeviceId = 0;
            gfxDriverFlags = 0;
            memset(keymap, 0x00, sizeof(keymap));
        }
    };

    struct MemoryBlock
    {
        uint8_t* data;
        uint32_t size;
    };

    typedef void(*coreFnUpdate)(float dt);

    // Public
    TERMITE_API coreConfig* coreLoadConfig(const char* confFilepath);
    TERMITE_API void coreFreeConfig(coreConfig* conf);

    TERMITE_API int coreInit(const coreConfig& conf, coreFnUpdate updateFn = nullptr, 
                              const gfxPlatformData* platformData = nullptr);
    TERMITE_API void coreShutdown();
    TERMITE_API void coreFrame();
    TERMITE_API uint32_t coreGetVersion();

    TERMITE_API bx::AllocatorI* coreGetAlloc() T_THREAD_SAFE;
    TERMITE_API const coreConfig& coreGetConfig();
    TERMITE_API dsDataStore* coreGetDefaultDataStore();

    TERMITE_API double coreGetFrameTime();
    TERMITE_API double coreGetElapsedTime();
    TERMITE_API double coreGetFps();

    TERMITE_API MemoryBlock* coreCreateMemory(uint32_t size, bx::AllocatorI* alloc = nullptr);
    TERMITE_API MemoryBlock* coreRefMemoryPtr(void* data, uint32_t size);
    TERMITE_API MemoryBlock* coreRefMemory(MemoryBlock* mem);
    TERMITE_API MemoryBlock* coreCopyMemory(const void* data, uint32_t size, bx::AllocatorI* alloc = nullptr);
    TERMITE_API void coreReleaseMemory(MemoryBlock* mem);
    TERMITE_API MemoryBlock* coreReadFile(const char* filepath);

    // UI Input
    TERMITE_API void coreSendInputMouse(float mousePos[2], int mouseButtons[3], float mouseWheel);
    TERMITE_API void coreSendInputChars(const char* chars);
    TERMITE_API void coreSendInputKeys(const bool keysDown[512], bool shift, bool alt, bool ctrl);

} // namespace st

