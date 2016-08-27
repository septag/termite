#pragma once

#include "types.h"

namespace termite
{
    struct IoDriverApi;
    class ResourceLib;

    struct ResourceTypeT {};
    struct ResourceT {};
    typedef PhantomType<uint16_t, ResourceTypeT, UINT16_MAX> ResourceTypeHandle;
    typedef PhantomType<uint16_t, ResourceT, UINT16_MAX> ResourceHandle;

    enum class ResourceLibInitFlag : uint8_t
    {
        None = 0x00,
        HotLoading = 0x01,
        AsyncLoading = 0x02
    };

    enum class ResourceFlag : uint8_t
    {
        None = 0x00,
        Reload = 0x01
    };

    struct ResourceTypeParams
    {
        const char* uri;
        const void* userParams;
        ResourceFlag flags;
    };

    class BX_NO_VTABLE ResourceCallbacksI
    {
    public:
        virtual bool loadObj(ResourceLib* resLib, const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj) = 0;
        virtual void unloadObj(ResourceLib* resLib, uintptr_t obj) = 0;
        virtual void onReload(ResourceLib* resLib, ResourceHandle handle) = 0;
        virtual uintptr_t getDefaultAsyncObj(ResourceLib* resLib) = 0;
    };

    TERMITE_API ResourceLib* createResourceLib(ResourceLibInitFlag flags, IoDriverApi* driver, bx::AllocatorI* alloc);
    TERMITE_API void destroyResourceLib(ResourceLib* resLib);

    TERMITE_API ResourceTypeHandle registerResourceType(ResourceLib* resLib, const char* name, 
                                                        ResourceCallbacksI* callbacks, int userParamsSize = 0);
    TERMITE_API void unregisterResourceType(ResourceLib* resLib, ResourceTypeHandle handle);
    TERMITE_API ResourceHandle loadResource(ResourceLib* resLib, const char* name, const char* uri,
                                            const void* userParams = nullptr, ResourceFlag flags = ResourceFlag::None);
    TERMITE_API void unloadResource(ResourceLib* resLib, ResourceHandle handle);
    TERMITE_API uintptr_t getResourceObj(ResourceLib* resLib, ResourceHandle handle);
} // namespace termite

C11_DEFINE_FLAG_TYPE(termite::ResourceLibInitFlag);
C11_DEFINE_FLAG_TYPE(termite::ResourceFlag);
