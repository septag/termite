#pragma once

#include "bx/bx.h"

// Export/Import Macro
// Export/Import API Def
#ifdef termite_SHARED_LIB
#ifdef termite_EXPORTS
#   if BX_COMPILER_MSVC
#       define TEE_API __declspec(dllexport) 
#   else
#       define TEE_API __attribute__ ((visibility("default")))
#   endif
#else
#   if BX_COMPILER_MSVC
#       define TEE_API __declspec(dllimport)
#   else
#       define TEE_API __attribute__ ((visibility("default")))
#   endif
#endif
#else
#   define TEE_API 
#endif

#if BX_COMPILER_CLANG || BX_COMPILER_GCC
#   define TEE_HIDDEN __attribute__((visibility("hidden")))
#else
#   define TEE_HIDDEN
#endif

// Versioning Macros
#define TEE_MAKE_VERSION(_Major, _Minor)  (uint32_t)(((_Major & 0xffff)<<16) | (_Minor & 0xffff))
#define TEE_VERSION_MAJOR(_Ver) (uint16_t)((_Ver >> 16) & 0xffff)
#define TEE_VERSION_MINOR(_Ver) (uint16_t)(_Ver & 0xffff)

// Shader and Path
#define TEE_XSTR(s) #s
#define TEE_STR(s) TEE_XSTR(s)
#define TEE_CONCAT_PATH_3(s1, s2, s3) TEE_STR(s1/s2/s3)
#define TEE_MAKE_SHADER_PATH_PLATFORM(Prefix, Platform, Filename) TEE_CONCAT_PATH_3(Prefix,Platform,Filename)
#define TEE_MAKE_SHADER_PATH(Prefix, Filename) TEE_MAKE_SHADER_PATH_PLATFORM(Prefix,termite_SHADER_APPEND_PATH,Filename)

// For self-documenting code
#define TEE_THREAD_SAFE

namespace tee
{
    // Phantom types are used to define strong types that use the same base type
    // For example Phantom<int, TextureHandle>, Pantom<int, ResourceHandle>
    template <typename Tx, typename _Meaning, Tx _Invalid = 0>
    struct PhantomType
    {
        inline PhantomType()
        {
            value = _Invalid;
        }

        inline PhantomType(Tx _value) : value(_value)
        {
        }

        inline operator Tx()
        {
            return value;
        }

        inline bool isValid() const
        {
            return value != _Invalid;
        }

        inline void reset()
        {
            value = _Invalid;
        }

        inline operator Tx() const
        {
            return value;
        }

        Tx value;
    };

    template <typename Tx>
    void* toPtr(Tx n)
    {
        return (void*)(uintptr_t(n));
    }

    template <typename Tx>
    Tx ptrTo(void* ptr)
    {
        return Tx(uintptr_t(ptr));
    }
} // namespace tee