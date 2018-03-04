#pragma once

#include "bx/bx.h"
#include "bx/allocator.h"
#include "types.h"
#include "assetlib.h"

namespace tee
{
    struct IncrLoader;
    struct IncrLoaderGroup {};
    typedef PhantomType<uint16_t, IncrLoaderGroup, UINT16_MAX> IncrLoaderGroupHandle;

    struct IncrLoadingScheme
    {
        enum Type
        {
            LoadDeltaTime,
            LoadDeltaFrame,
            LoadSequential
        };

        Type type;
        union 
        {
            int frameDelta;
            float deltaTime;
        };

        IncrLoadingScheme(IncrLoadingScheme::Type _type = IncrLoadingScheme::LoadSequential, float _value = 0)
        {
            type = _type;

            switch (_type) {
            case IncrLoadingScheme::LoadDeltaFrame:
                frameDelta = int(_value);
                break;
            case IncrLoadingScheme::LoadDeltaTime:
                deltaTime = _value;
                break;
            default:
                break;
            }
        }
    };

    struct IncrLoaderFlags
    {
        enum Enum {
            None = 0,
            DeleteGroup = 0x1,
            RetryFailed = 0x2      // Retry loading failed assets once
        };
        typedef uint8_t Bits;
    };

    // API
    namespace asset {
        TEE_API IncrLoader* createIncrementalLoader(bx::AllocatorI* alloc);
        TEE_API void destroyIncrementalLoader(IncrLoader* loader);

        TEE_API void beginIncrLoadGroup(IncrLoader* loader, const IncrLoadingScheme& scheme = IncrLoadingScheme());
        TEE_API IncrLoaderGroupHandle endIncrLoadGroup(IncrLoader* loader);
        TEE_API void deleteIncrloadGroup(IncrLoader* loader, IncrLoaderGroupHandle handle);

        // Note: Removes the group if it's done loading
        //       This function should be called after creating all groups, actually that is the purpose of ProgressiveLoader
        //       After the function returns true, handle is no longer valid, so it should be handled by caller
        TEE_API bool isLoadDone(IncrLoader* loader, IncrLoaderGroupHandle handle, IncrLoaderFlags::Bits flags);

        // Receives a handle to Resource, which will be filled later
        TEE_API void load(IncrLoader* loader, AssetHandle* pHandle,
                          const char* name, const char* uri, const void* userParams,
                          AssetFlags::Bits flags = 0, bx::AllocatorI* objAlloc = nullptr);
        TEE_API void unload(IncrLoader* loader, AssetHandle handle);

        TEE_API void stepIncrLoader(IncrLoader* loader, float dt);
    }   // namespace asset

} // namespace tee
