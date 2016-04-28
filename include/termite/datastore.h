#pragma once

namespace termite
{
    class dsDriver;
    class dsDataStore;

    enum class dsInitFlag : uint8_t
    {
        None = 0x00,
        HotLoading = 0x01,
        AsyncLoading = 0x02
    };

    enum class dsFlag : uint8_t
    {
        None = 0x00,
        Reload = 0x01
    };

    T_HANDLE(dsResourceTypeHandle);
    T_HANDLE(dsResourceHandle);

    struct dsResourceTypeParams
    {
        const char* uri;
        const void* userParams;
    };

    class BX_NO_VTABLE dsResourceCallbacks
    {
    public:
        virtual bool loadObj(const MemoryBlock* mem, const dsResourceTypeParams& params, uintptr_t* obj) = 0;
        virtual void unloadObj(uintptr_t obj) = 0;
        virtual void onReload(dsResourceHandle handle) = 0;
        virtual uintptr_t getDefaultAsyncObj() = 0;
    };

    TERMITE_API dsDataStore* dsCreate(dsInitFlag flags, dsDriver* driver);
    TERMITE_API void dsDestroy(dsDataStore* ds);

    TERMITE_API dsResourceTypeHandle dsRegisterResourceType(dsDataStore* ds, const char* name,
                                                             dsResourceCallbacks* callbacks, int userParamsSize = 0);
    TERMITE_API void dsUnregisterResourceType(dsDataStore* ds, dsResourceTypeHandle handle);
    TERMITE_API dsResourceHandle dsLoadResource(dsDataStore* ds, const char* name, const char* uri,
                                                 const void* userParams = nullptr, dsFlag flags = dsFlag::None);
    TERMITE_API void dsUnloadResource(dsDataStore* ds, dsResourceHandle handle);
    TERMITE_API uintptr_t dsGetObj(dsDataStore* ds, dsResourceHandle handle);

    template <typename Ty> Ty dsGetObjHandle(dsDataStore* ds, dsResourceHandle handle)
    {
        Ty o;
        o.idx = (uint16_t)dsGetObj(ds, handle);
        return o;
    }

    template <typename Ty> Ty dsGetObjPtr(dsDataStore* ds, dsResourceHandle handle)
    {
        return (Ty)dsGetObj(ds, handle);
    }
} // namespace st

C11_DEFINE_FLAG_TYPE(termite::dsInitFlag);
C11_DEFINE_FLAG_TYPE(termite::dsFlag);
