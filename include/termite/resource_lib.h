#pragma once

#include "types.h"

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
        virtual bool loadObj(ResourceLib* resLib, const MemoryBlock* mem, const ResourceTypeParams& params, uintptr_t* obj) = 0;
        virtual void unloadObj(ResourceLib* resLib, uintptr_t obj) = 0;
        virtual void onReload(ResourceLib* resLib, ResourceHandle handle) = 0;
        virtual uintptr_t getDefaultAsyncObj(ResourceLib* resLib) = 0;
    };

    typedef void(*FileModifiedCallback)(ResourceLib* resLib, const char* uri, void* userParam);

    TERMITE_API ResourceLib* createResourceLib(ResourceLibInitFlag::Bits flags, IoDriverApi* driver, bx::AllocatorI* alloc);
    TERMITE_API void destroyResourceLib(ResourceLib* resLib);
    TERMITE_API void setFileModifiedCallback(ResourceLib* resLib, FileModifiedCallback callback, void* userParam);
    TERMITE_API IoDriverApi* getResourceLibIoDriver(ResourceLib* resLib);

    TERMITE_API ResourceTypeHandle registerResourceType(ResourceLib* resLib, const char* name, 
                                                        ResourceCallbacksI* callbacks, int userParamsSize = 0,
                                                        uintptr_t failObj = 0);
    TERMITE_API void unregisterResourceType(ResourceLib* resLib, ResourceTypeHandle handle);
    TERMITE_API ResourceHandle loadResource(ResourceLib* resLib, const char* name, const char* uri,
                                            const void* userParams, ResourceFlag::Bits flags = ResourceFlag::None);
    TERMITE_API ResourceHandle loadResourceFromMem(ResourceLib* resLib, const char* name, const char* uri, const MemoryBlock* mem, 
                                                   const void* userParams = nullptr, ResourceFlag::Bits flags = ResourceFlag::None);
    TERMITE_API void unloadResource(ResourceLib* resLib, ResourceHandle handle);
    TERMITE_API uintptr_t getResourceObj(ResourceLib* resLib, ResourceHandle handle);
    TERMITE_API ResourceLoadState::Enum getResourceLoadState(ResourceLib* resLib, ResourceHandle handle);
    TERMITE_API int getResourceParamSize(ResourceLib* resLib, const char* name);

    template <typename Ty>
    Ty* getResourcePtr(ResourceLib* resLib, ResourceHandle handle)
    {
        return (Ty*)getResourceObj(resLib, handle);
    }

    // Helper Wrapper class
    class ResourceLibHelper
    {
    public:
        ResourceLib* m_resLib;

    public:
        ResourceLibHelper()
        {
            m_resLib = nullptr;
        }

        explicit ResourceLibHelper(ResourceLib* resLib) :
            m_resLib(resLib)
        {
        }

        inline bool isValid() const
        {
            return m_resLib != nullptr;
        }

        inline void setFileModifiedCallback(FileModifiedCallback callback, void* userParam)
        {
            termite::setFileModifiedCallback(m_resLib, callback, userParam);
        }

        inline IoDriverApi* getResourceLibIoDriver()
        {
            return termite::getResourceLibIoDriver(m_resLib);
        }

        inline ResourceTypeHandle registerResourceType(const char* name, ResourceCallbacksI* callbacks, 
                                                       int userParamsSize = 0, uintptr_t failObj = 0)
        {
            return termite::registerResourceType(m_resLib, name, callbacks, userParamsSize, failObj);
        }

        inline void unregisterResourceType(ResourceTypeHandle handle)
        {
            termite::unregisterResourceType(m_resLib, handle);
        }

        inline ResourceHandle loadResource(const char* name, const char* uri, const void* userParams = nullptr, 
                                           ResourceFlag::Bits flags = ResourceFlag::None)
        {
            return termite::loadResource(m_resLib, name, uri, userParams, flags);
        }

        inline ResourceHandle loadResourceFromMem(const char* name, const char* uri, const MemoryBlock* mem,
                                                  const void* userParams = nullptr, ResourceFlag::Bits flags = ResourceFlag::None)
        {
            return termite::loadResourceFromMem(m_resLib, name, uri, mem, userParams, flags);
        }

        inline void unloadResource(ResourceHandle handle)
        {
            return termite::unloadResource(m_resLib, handle);
        }

        inline uintptr_t getResourceObj(ResourceHandle handle)
        {
            return termite::getResourceObj(m_resLib, handle);
        }

        inline getResourceParamSize(const char* name)
        {
            return termite::getResourceParamSize(m_resLib, name);
        }

        template <typename Ty>
        Ty* getResourcePtr(ResourceHandle handle) const
        {
            return (Ty*)termite::getResourceObj(m_resLib, handle);
        }

        ResourceLoadState::Enum getResourceLoadState(ResourceHandle handle) const
        {
            return termite::getResourceLoadState(m_resLib, handle);
        }

        ResourceLib* getResourceLib() const
        {
            return m_resLib;
        }
    };

    TERMITE_API ResourceLibHelper getDefaultResourceLib();
} // namespace termite

