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

    // API
    namespace asset {
        TEE_API IncrLoader* createIncrementalLoader(bx::AllocatorI* alloc);
        TEE_API void destroyIncrementalLoader(IncrLoader* loader);

        TEE_API void beginIncrLoad(IncrLoader* loader, const IncrLoadingScheme& scheme = IncrLoadingScheme());
        TEE_API IncrLoaderGroupHandle endIncrLoad(IncrLoader* loader);

        // Note: Removes the group if it's done loading
        //       This function should be called after creating all groups, actually that is the purpose of ProgressiveLoader
        //       After the function returns true, handle is no longer valid, so it should be handled by caller
        TEE_API bool isLoadDone(IncrLoader* loader, IncrLoaderGroupHandle handle);

        // Receives a handle to Resource, which will be filled later
        TEE_API void load(IncrLoader* loader, AssetHandle* pHandle,
                          const char* name, const char* uri, const void* userParams,
                          AssetFlags::Bits flags = 0, bx::AllocatorI* objAlloc = nullptr);
        TEE_API void unload(IncrLoader* loader, AssetHandle handle);

        TEE_API void stepIncrLoader(IncrLoader* loader, float dt);
    }   // namespace asset

    // Wrapper class around incremental loader
    class CIncrLoader
    {
    private:
        IncrLoader* m_loader;

    public:
        CIncrLoader() :
            m_loader(nullptr)
        {
        }

        explicit CIncrLoader(IncrLoader* loader) :
            m_loader(loader)
        {
        }

        inline bool create(bx::AllocatorI* alloc)
        {
            assert(!m_loader);
            m_loader = asset::createIncrementalLoader(alloc);
            return m_loader != nullptr;
        }

        inline void destroy()
        {
            if (m_loader) {
                asset::destroyIncrementalLoader(m_loader);
                m_loader = nullptr;
            }
        }

        inline const CIncrLoader& beginGroup(const IncrLoadingScheme& scheme = IncrLoadingScheme()) const 
        {
            asset::beginIncrLoad(m_loader, scheme);
            return *this;
        }

        const CIncrLoader& loadResource(AssetHandle* pHandle, const char* name, const char* uri, 
                                               const void* userParams, AssetFlags::Bits flags = 0,
                                               bx::AllocatorI* objAlloc = nullptr) const
        {
            asset::load(m_loader, pHandle, name, uri, userParams, flags, objAlloc);
            return *this;
        }

        const CIncrLoader& unloadResource(AssetHandle handle) const
        {
            asset::unload(m_loader, handle);
            return *this;
        }

        inline IncrLoaderGroupHandle endGroup() const
        {
            return asset::endIncrLoad(m_loader);
        }

        inline bool checkGroupDone(IncrLoaderGroupHandle handle) const
        {
            return asset::isLoadDone(m_loader, handle);
        }

        inline void step(float dt) const
        {
            return asset::stepIncrLoader(m_loader, dt);
        }
    };
} // namespace tee
