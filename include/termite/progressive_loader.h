#pragma once

#include "bx/bx.h"
#include "bx/allocator.h"
#include "types.h"
#include "resource_lib.h"

namespace termite
{
    struct ProgressiveLoader;
    struct LoaderGroupT {};
    typedef PhantomType<uint16_t, LoaderGroupT, UINT16_MAX> LoaderGroupHandle;

    struct LoadingScheme
    {
        enum Type
        {
            LoadDeltaTime,
            LoadDeltaFrame,
            Sequential
        };

        Type type;
        union 
        {
            int frameDelta;
            float deltaTime;
        };

        LoadingScheme(LoadingScheme::Type _type = LoadingScheme::Sequential, float _value = 0)
        {
            type = _type;

            switch (_type) {
            case LoadingScheme::LoadDeltaFrame:
                frameDelta = int(_value);
                break;
            case LoadingScheme::LoadDeltaTime:
                deltaTime = _value;
                break;
            default:
                break;
            }
        }
    };

    TERMITE_API ProgressiveLoader* createProgressiveLoader(bx::AllocatorI* alloc);
    TERMITE_API void destroyProgressiveLoader(ProgressiveLoader* loader);

    TERMITE_API void beginLoaderGroup(ProgressiveLoader* loader, const LoadingScheme& scheme = LoadingScheme());
    TERMITE_API LoaderGroupHandle endLoaderGroup(ProgressiveLoader* loader);

    // Note: Removes the group if it's done loading
    //       This function should be called after creating all groups, actually that is the purpose of ProgressiveLoader
    //       After the function returns true, handle is no longer valid, so it should be handled by caller
    TERMITE_API bool checkLoaderGroupDone(ProgressiveLoader* loader, LoaderGroupHandle handle);

    // Receives a handle to Resource, which will be filled later
    TERMITE_API void loadResource(ProgressiveLoader* loader, ResourceHandle* pHandle,
                                  const char* name, const char* uri, const void* userParams, 
                                  ResourceFlag::Bits flags = 0);
    TERMITE_API void unloadResource(ProgressiveLoader* loader, ResourceHandle handle);

    TERMITE_API void stepLoader(ProgressiveLoader* loader, float dt);

    class CProgressiveLoader
    {
    private:
        ProgressiveLoader* m_loader;

    public:
        CProgressiveLoader() :
            m_loader(nullptr)
        {
        }

        explicit CProgressiveLoader(ProgressiveLoader* loader) :
            m_loader(loader)
        {
        }

        inline bool create(bx::AllocatorI* alloc)
        {
            m_loader = createProgressiveLoader(alloc);
            return m_loader != nullptr;
        }

        inline void destroy()
        {
            if (m_loader) {
                destroyProgressiveLoader(m_loader);
                m_loader = nullptr;
            }
        }

        inline CProgressiveLoader& beginGroup(const LoadingScheme& scheme = LoadingScheme())
        {
            termite::beginLoaderGroup(m_loader, scheme);
            return *this;
        }

        CProgressiveLoader& loadResource(ResourceHandle* pHandle, const char* name, const char* uri, 
                                              const void* userParams, ResourceFlag::Bits flags = 0)
        {
            termite::loadResource(m_loader, pHandle, name, uri, userParams, flags);
            return *this;
        }

        CProgressiveLoader& unloadResource(ResourceHandle handle)
        {
            termite::unloadResource(m_loader, handle);
            return *this;
        }

        inline LoaderGroupHandle endGroup()
        {
            return termite::endLoaderGroup(m_loader);
        }

        inline bool checkGroupDone(LoaderGroupHandle handle)
        {
            return termite::checkLoaderGroupDone(m_loader, handle);
        }

        inline void step(float dt)
        {
            return termite::stepLoader(m_loader, dt);
        }
    };



} // namespace termite
