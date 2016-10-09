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

        LoadingScheme(LoadingScheme _type = LoadingScheme::Sequential, float _value = 0)
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

    ProgressiveLoader* createProgressiveLoader(ResourceLib* resLib, bx::AllocatorI* alloc);
    void destroyProgressiveLoader(ProgressiveLoader* loader);

    void beginLoaderGroup(ProgressiveLoader* loader, const LoadingScheme& scheme);
    LoaderGroupHandle endLoaderGroup(ProgressiveLoader* loader);
    bool isLoaderGroupDone(ProgressiveLoader* loader, LoaderGroupHandle handle);

    ResourceHandle loadResource(ProgressiveLoader* loader, const char* name, const char* uri, const void* userParams, 
                                ResourceFlag::Bits flags = 0);
    void unloadResource(ProgressiveLoader* loader, ResourceHandle handle);

    void stepLoader(ProgressiveLoader* loader, float dt);

    class ProgressiveLoaderHelper
    {
    private:
        ProgressiveLoader* m_loader;

    public:
        ProgressiveLoaderHelper() :
            m_loader(nullptr)
        {
        }

        explicit ProgressiveLoaderHelper(ProgressiveLoader* loader) :
            m_loader(loader)
        {
        }

        inline void beginGroup(const LoadingScheme& scheme)
        {
            termite::beginLoaderGroup(m_loader, scheme);
        }

        inline ResourceHandle loadResource(const char* name, const char* uri, const void* userParams,
                                           ResourceFlag::Bits flags = 0)
        {
            return termite::loadResource(m_loader, name, uri, userParams, flags);
        }

        inline void unloadResource(ResourceHandle handle)
        {
            termite::unloadResource(m_loader, handle);
        }

        inline LoaderGroupHandle endGroup()
        {
            return termite::endLoaderGroup(m_loader);
        }

        inline bool isGroupDone(LoaderGroupHandle handle)
        {
            return termite::isLoaderGroupDone(m_loader, handle);
        }

        inline void step(float dt)
        {
            return termite::stepLoader(m_loader, dt);
        }
    };



} // namespace termite
