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

    ProgressiveLoader* createProgressiveLoader(bx::AllocatorI* alloc);
    void destroyProgressiveLoader(ProgressiveLoader* loader);

    void beginLoaderGroup(ProgressiveLoader* loader, const LoadingScheme& scheme);
    LoaderGroupHandle endLoaderGroup(ProgressiveLoader* loader);

    // Note: Removes the group if it's done loading
    //       This function should be called after creating all groups, actually that is the purpose of ProgressiveLoader
    //       After the function returns true, handle is no longer valid, so it should be handled by caller
    bool checkLoaderGroupDone(ProgressiveLoader* loader, LoaderGroupHandle handle);

    // Receives a handle to Resource, which will be filled later
    void loadResource(ProgressiveLoader* loader, ResourceHandle* pHandle, 
                      const char* name, const char* uri, const void* userParams, 
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

        inline ProgressiveLoaderHelper& beginGroup(const LoadingScheme& scheme)
        {
            termite::beginLoaderGroup(m_loader, scheme);
            return *this;
        }

        ProgressiveLoaderHelper& loadResource(ResourceHandle* pHandle, const char* name, const char* uri, 
                                              const void* userParams, ResourceFlag::Bits flags = 0)
        {
            termite::loadResource(m_loader, pHandle, name, uri, userParams, flags);
            return *this;
        }

        ProgressiveLoaderHelper& unloadResource(ResourceHandle handle)
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
