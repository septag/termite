#pragma once

#ifdef termite_SHARED_LIB
#   if BX_COMPILER_MSVC
#       define T_PLUGIN_EXPORT extern "C" __declspec(dllexport) 
#   else
#       define T_PLUGIN_EXPORT extern "C" __attribute__ ((visibility("default")))
#   endif
#else
#   define T_PLUGIN_EXPORT extern "C" 
#endif

#define BX_TRACE_API(_Api, _Fmt, ...) _Api->logPrintf(__FILE__, __LINE__, bx::LogType::Text, _Fmt, ##__VA_ARGS__)
#define BX_VERBOSE_API(_Api, _Fmt, ...) _Api->logPrintf(__FILE__, __LINE__, bx::LogType::Verbose, _Fmt, ##__VA_ARGS__)
#define BX_FATAL_API(_Api, _Fmt, ...) _Api->logPrintf(__FILE__, __LINE__, bx::LogType::Fatal, _Fmt, ##__VA_ARGS__)
#define BX_WARN_API(_Api, _Fmt, ...) _Api->logPrintf(__FILE__, __LINE__, bx::LogType::Warning, _Fmt, ##__VA_ARGS__)
#define BX_BEGINP_API(_Api, _Fmt, ...) _Api->logBeginProgress(__FILE__, __LINE__, _Fmt, ##__VA_ARGS__)
#define BX_END_OK_API(_Api) _Api->logEndProgress(bx::LogProgressResult::Ok)
#define BX_END_FATAL_API(_Api) _Api->logEndProgress(bx::LogProgressResult::Fatal)
#define BX_END_NONFATAL_API(_Api) _Api->logEndProgress(bx::LogProgressResult::NonFatal)

#define T_ERROR_API(_Api, _Fmt, ...) _Api->reportErrorf(__FILE__, __LINE__, _Fmt, ##__VA_ARGS__)

namespace termite
{
    enum class ApiId : uint16_t
    {
        Core = 0,
        Plugin, 
        Gfx
    };

    // Plugins should also implement this function
    // Should be named "termiteGetPluginApi" (extern "C")
    typedef void* (*GetApiFunc)(uint16_t apiId, uint32_t version);

    enum PluginType : uint16_t
    {
        Unknown = 0,
        GraphicsDriver,
        IoDriver,
        Renderer
    };

    struct PluginDesc
    {
        char name[32];
        char description[64];
        uint32_t version;
        PluginType type;
    };

    struct PluginApi_v0
    {
        void* (*init)(bx::AllocatorI* alloc, GetApiFunc getApi);
        void (*shutdown)();
        PluginDesc* (*getDesc)();
    };

    void* getEngineApi(uint16_t apiId, uint32_t version);
}   // namespace termite

#ifdef T_CORE_API
#include "core.h"
#include "bxx/logger.h"

namespace termite {
    struct CoreApi_v0
    {
        MemoryBlock* (*createMemoryBlock)(uint32_t size, bx::AllocatorI* alloc);
        MemoryBlock* (*refMemoryBlockPtr)(void* data, uint32_t size);
        MemoryBlock* (*refMemoryBlock)(MemoryBlock* mem);
        MemoryBlock* (*copyMemoryBlock)(const void* data, uint32_t size, bx::AllocatorI* alloc);
        void(*releaseMemoryBlock)(MemoryBlock* mem);
        MemoryBlock* (*readTextFile)(const char* filepath);
        double(*getElapsedTime)();
        void(*reportError)(const char* source, int line, const char* desc);
        void(*reportErrorf)(const char* source, int line, const char* fmt, ...);

        void(*logPrint)(const char* sourceFile, int line, bx::LogType type, const char* text);
        void(*logPrintf)(const char* sourceFile, int line, bx::LogType type, const char* fmt, ...);
        void(*logBeginProgress)(const char* sourceFile, int line, const char* fmt, ...);
        void(*logEndProgress)(bx::LogProgressResult result);

        const Config& (*getConfig)();
        uint32_t(*getEngineVersion)();
        bx::AllocatorI* (*getTempAlloc)();
    };
}
#endif

#ifdef T_GFX_API
#include "vec_math.h"
#include "gfx_defines.h"
namespace termite {
    struct GfxApi_v0
    {
        void(*calcGaussKernel)(vec4_t* kernel, int kernelSize, float stdDevSqr, float intensity,
            int direction /*=0 horizontal, =1 vertical*/, int width, int height);
        ProgramHandle(*loadShaderProgram)(GfxDriverApi* gfxDriver, IoDriverApi* ioDriver, const char* vsFilepath,
            const char* fsFilepath);
        void(*drawFullscreenQuad)(uint8_t viewId, ProgramHandle prog);

        VertexDecl* (*vdeclBegin)(VertexDecl* vdecl, RendererType _type);
        void(*vdeclEnd)(VertexDecl* vdecl);
        VertexDecl* (*vdeclAdd)(VertexDecl* vdecl, VertexAttrib _attrib, uint8_t _num, VertexAttribType _type, bool _normalized, bool _asInt);
        VertexDecl* (*vdeclSkip)(VertexDecl* vdecl, uint8_t _numBytes);
        void(*vdeclDecode)(VertexDecl* vdecl, VertexAttrib _attrib, uint8_t* _num, VertexAttribType* _type, bool* _normalized, bool* _asInt);
        bool(*vdeclHas)(VertexDecl* vdecl, VertexAttrib _attrib);
        uint32_t(*vdeclGetSize)(VertexDecl* vdecl, uint32_t _num);
    };
}
#endif

