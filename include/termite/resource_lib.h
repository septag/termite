#pragma once

#include "types.h"
#include "bx/allocator.h"
#include "core.h"

#define T_RESOURCE_MAX_USERPARAM_SIZE 256   // maximum size of userParam to be passed to resource loader

namespace termite
{
    struct IoDriverApi;
    class ResourceLib;

    struct ResourceTypeT {};
    struct ResourceT {};
    typedef PhantomType<uint16_t, ResourceTypeT, UINT16_MAX> ResourceTypeHandle;
    typedef PhantomType<uint16_t, ResourceT, UINT16_MAX> ResourceHandle;

    struct ResourceLibInitFlag
    {
        enum Enum
        {
            None = 0x00,
            HotLoading = 0x01,
            AsyncLoading = 0x02
        };

        typedef uint8_t Bits;
    };

    struct ResourceFlag
    {
        enum Enum
        {
            None = 0x00,
            Reload = 0x01
        };

        typedef uint8_t Bits;
    };

    struct ResourceTypeParams
    {
        const char* uri;
        const void* userParams;
        ResourceFlag::Bits flags;
    };

    struct ResourceLoadState
    {
        enum Enum
        {
            LoadOk,
            LoadFailed,
            LoadInProgress
        };
    };

    class BX_NO_VTABLE ResourceCallbacksI
    {
    public:
        virtual bool loadObj(const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj, bx::AllocatorI* alloc) = 0;
        virtual void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) = 0;
        virtual void onReload(ResourceHandle handle, bx::AllocatorI* alloc) = 0;
    };

    typedef void(*FileModifiedCallback)(const char* uri, void* userParam);

    TERMITE_API result_t initResourceLib(ResourceLibInitFlag::Bits flags, IoDriverApi* driver, bx::AllocatorI* alloc);
    TERMITE_API void shutdownResourceLib();
    TERMITE_API void setFileModifiedCallback(FileModifiedCallback callback, void* userParam);
    TERMITE_API IoDriverApi* getResourceLibIoDriver();

    TERMITE_API ResourceTypeHandle registerResourceType(const char* name, ResourceCallbacksI* callbacks, 
                                                        int userParamsSize = 0, uintptr_t failObj = 0, 
                                                        uintptr_t asyncProgressObj = 0);
    TERMITE_API void unregisterResourceType(ResourceTypeHandle handle);
    TERMITE_API ResourceHandle loadResource(const char* name, const char* uri,
                                            const void* userParams, ResourceFlag::Bits flags = ResourceFlag::None,
                                            bx::AllocatorI* objAlloc = nullptr);
    TERMITE_API ResourceHandle loadResourceFromMem(const char* name, const char* uri, const MemoryBlock* mem, 
                                                   const void* userParams = nullptr, ResourceFlag::Bits flags = ResourceFlag::None,
                                                   bx::AllocatorI* objAlloc = nullptr);
    TERMITE_API void unloadResource(ResourceHandle handle);

    TERMITE_API uintptr_t getResourceObj(ResourceHandle handle);
    TERMITE_API ResourceLoadState::Enum getResourceLoadState(ResourceHandle handle);
    TERMITE_API int getResourceParamSize(const char* name);
    TERMITE_API const char* getResourceUri(ResourceHandle handle);
    TERMITE_API const char* getResourceName(ResourceHandle handle);
    TERMITE_API const void* getResourceParams(ResourceHandle handle);
    TERMITE_API ResourceHandle getResourceFailHandle(const char* name);
    TERMITE_API ResourceHandle getResourceAsyncHandle(const char* name);
    TERMITE_API ResourceHandle addResourceRef(ResourceHandle handle);
    TERMITE_API uint32_t getResourceRefCount(ResourceHandle handle);

    template <typename Ty>
    Ty* getResourcePtr(ResourceHandle handle)
    {
        return (Ty*)getResourceObj(handle);
    }

    template <typename Ty>
    Ty getResourceCast(ResourceHandle handle)
    {
        return Ty((void*)getResourceObj(handle));
    }

} // namespace termite

