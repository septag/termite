#pragma once

#include "types.h"
#include "bx/allocator.h"
#include "tee.h"

#define TEE_ASSET_MAX_USERPARAM_SIZE 256   // maximum size of userParam to be passed to resource loader

namespace tee
{
    struct IoDriver;
    struct AssetTypeT {};
    struct AssetT {};
    typedef PhantomType<uint16_t, AssetTypeT, UINT16_MAX> AssetTypeHandle;
    typedef PhantomType<uint16_t, AssetT, UINT16_MAX> AssetHandle;

    struct AssetLibInitFlags
    {
        enum Enum
        {
            None = 0x00,
            HotLoading = 0x01,
            AsyncLoading = 0x02
        };

        typedef uint8_t Bits;
    };

    struct AssetFlags
    {
        enum Enum
        {
            None = 0x00,
            Reload = 0x01,
            ForceBlockLoad = 0x02
        };

        typedef uint8_t Bits;
    };

    struct AssetParams
    {
        const char* uri;
        const void* userParams;
        AssetFlags::Bits flags;
    };

    struct AssetState
    {
        enum Enum
        {
            LoadOk,
            LoadFailed,
            LoadInProgress
        };
    };

    class BX_NO_VTABLE AssetLibCallbacksI
    {
    public:
        virtual bool loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc) = 0;
        virtual void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) = 0;
        virtual void onReload(AssetHandle handle, bx::AllocatorI* alloc) = 0;
    };

    namespace asset {
        typedef void(*AssetModifiedCallback)(const char* uri, void* userParam);

        TEE_API void setModifiedCallback(AssetModifiedCallback callback, void* userParam);
        TEE_API IoDriver* getIoDriver();

        TEE_API AssetTypeHandle registerType(const char* name, AssetLibCallbacksI* callbacks,
                                             int userParamsSize = 0, uintptr_t failObj = 0,
                                             uintptr_t asyncProgressObj = 0);
        TEE_API void unregisterType(AssetTypeHandle handle);
        TEE_API AssetHandle load(const char* name, const char* uri,
                                 const void* userParams, AssetFlags::Bits flags = AssetFlags::None,
                                 bx::AllocatorI* objAlloc = nullptr);
        TEE_API AssetHandle loadMem(const char* name, const char* uri, const MemoryBlock* mem,
                                    const void* userParams, AssetFlags::Bits flags = AssetFlags::None,
                                    bx::AllocatorI* objAlloc = nullptr);
        TEE_API void unload(AssetHandle handle);

        TEE_API uintptr_t getObj(AssetHandle handle);
        TEE_API AssetState::Enum getState(AssetHandle handle);
        TEE_API int getParamSize(const char* name);
        TEE_API const char* getUri(AssetHandle handle);
        TEE_API const char* getName(AssetHandle handle);
        TEE_API const void* getParams(AssetHandle handle);
        TEE_API AssetHandle getFailHandle(const char* name);
        TEE_API AssetHandle getAsyncHandle(const char* name);
        TEE_API AssetHandle addRef(AssetHandle handle);
        TEE_API uint32_t getRefCount(AssetHandle handle);
        TEE_API void reloadAssets(const char* name);
        TEE_API void unloadAssets(const char* name);
        TEE_API bool checkAssetsLoaded(const char* name);

        // Recommended: pass 'ext' and 'extReplacement' as lower-case
        // Pass extReplacement = nullptr to remove the override
        TEE_API void replaceFileExtension(const char* ext, const char* extReplacement = nullptr);
        TEE_API void replaceAsset(const char* uri, const char* replaceUri);

        template <typename Ty>
        Ty* getObjPtr(AssetHandle handle)
        {
            return (Ty*)getObj(handle);
        }
    }   // namespace asset

} // namespace tee

