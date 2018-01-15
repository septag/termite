#include "pch.h"

#include "incremental_loader.h"

#include "bxx/path.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/handle_pool.h"

#define REQUEST_POOL_SIZE 128

namespace tee {

    struct LoadResourceRequest
    {
        typedef bx::List<LoadResourceRequest*>::Node LNode;

        char name[32];
        bx::Path uri;
        uint8_t userParams[TEE_ASSET_MAX_USERPARAM_SIZE];
        AssetFlags::Bits flags;
        bx::AllocatorI* objAlloc;

        AssetHandle* pHandle;

        LNode lnode;

        LoadResourceRequest() :
            lnode(this)
        {
            objAlloc = nullptr;
        }
    };

    struct UnloadResourceRequest
    {
        typedef bx::List<UnloadResourceRequest*>::Node LNode;
        AssetHandle handle;
        LNode lnode;

        UnloadResourceRequest() :
            lnode(this)
        {
        }
    };

    struct LoaderGroup
    {
        IncrLoadingScheme scheme;
        bx::List<LoadResourceRequest*> loadRequestList;
        bx::List<UnloadResourceRequest*> unloadRequestList;

        float elapsedTime;
        int frameCount;
    };

    struct IncrLoader
    {
        bx::AllocatorI* alloc;
        bx::Pool<LoadResourceRequest> loadRequestPool;
        bx::Pool<UnloadResourceRequest> unloadRequestPool;
        bx::HandlePool groupPool;
        IncrLoaderGroupHandle curGroupHandle;

        IncrLoader(bx::AllocatorI* _alloc) :
            alloc(_alloc)
        {
        }
    };

    IncrLoader* asset::createIncrementalLoader(bx::AllocatorI* alloc)
    {
        IncrLoader* loader = BX_NEW(alloc, IncrLoader)(alloc);

        const uint32_t itemSizes[] = {
            sizeof(LoaderGroup)
        };
        if (!loader->loadRequestPool.create(REQUEST_POOL_SIZE, alloc) ||
            !loader->unloadRequestPool.create(REQUEST_POOL_SIZE, alloc) ||
            !loader->groupPool.create(itemSizes, 1, 32, 32, alloc)) {
            BX_DELETE(alloc, loader);
            return nullptr;
        }

        return loader;
    }

    void asset::destroyIncrementalLoader(IncrLoader* loader)
    {
        assert(loader);
        assert(loader->alloc);

        loader->unloadRequestPool.destroy();
        loader->loadRequestPool.destroy();
        loader->groupPool.destroy();
        BX_DELETE(loader->alloc, loader);
    }

    void asset::beginIncrLoad(IncrLoader* loader, const IncrLoadingScheme& scheme)
    {
        assert(loader);
        IncrLoaderGroupHandle handle = IncrLoaderGroupHandle(loader->groupPool.newHandle());
        assert(handle.isValid());
        LoaderGroup* group = new(loader->groupPool.getHandleData(0, handle)) LoaderGroup();
        memcpy(&group->scheme, &scheme, sizeof(scheme));
        group->frameCount = 0;
        group->elapsedTime = 0;

        loader->curGroupHandle = handle;
    }

    IncrLoaderGroupHandle asset::endIncrLoad(IncrLoader* loader)
    {
        assert(loader);
        IncrLoaderGroupHandle handle = loader->curGroupHandle;

        loader->curGroupHandle = IncrLoaderGroupHandle();
        return handle;
    }

    bool asset::isLoadDone(IncrLoader* loader, IncrLoaderGroupHandle handle)
    {
        assert(loader);
        assert(handle.isValid());

        LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, handle);
        bool done = group->loadRequestList.isEmpty() && group->unloadRequestList.isEmpty();
        if (done) {
            loader->groupPool.freeHandle(handle);
        } 
        return done;
    }

    void asset::load(IncrLoader* loader,
                     AssetHandle* pHandle,
                     const char* name, const char* uri, const void* userParams,
                     AssetFlags::Bits flags /*= 0*/, bx::AllocatorI* objAlloc/* = nullptr*/)
    {
        assert(loader->curGroupHandle.isValid());
        assert(pHandle);

        pHandle->reset();

        // create a new request
        LoadResourceRequest* req = loader->loadRequestPool.newInstance();
        if (!req) {
            assert(false);
            return;
        }

        bx::strCopy(req->name, sizeof(req->name), name);
        req->uri = uri;
        int paramSize = asset::getParamSize(name);
        if (paramSize && userParams) {
            memcpy(req->userParams, userParams, paramSize);
        } else {
            bx::memSet(req->userParams, 0x00, sizeof(req->userParams));
        }
        req->flags = flags;
        req->pHandle = pHandle;
        req->objAlloc = objAlloc;

        // Add to group
        LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, loader->curGroupHandle);
        assert(group);
        group->loadRequestList.addToEnd(&req->lnode);
    }

    void asset::unload(IncrLoader* loader, AssetHandle handle)
    {
        assert(loader->curGroupHandle.isValid());
        assert(handle.isValid());

        UnloadResourceRequest* req = loader->unloadRequestPool.newInstance();
        if (!req) {
            assert(false);
            return;
        }

        req->handle = handle;

        // Add to Group
        LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, loader->curGroupHandle);
        assert(group);
        group->unloadRequestList.addToEnd(&req->lnode);
    }

    static LoadResourceRequest* getFirstLoadRequest(IncrLoader* loader, LoaderGroup* group)
    {
        LoadResourceRequest::LNode* node = group->loadRequestList.getFirst();
        while (node) {
            LoadResourceRequest* req = node->data;
            LoadResourceRequest::LNode* next = node->next;

            if (!req->pHandle->isValid())
                return req;

            // Remove items that are not in progress
            if (asset::getState(*req->pHandle) != AssetState::LoadInProgress) {
                group->loadRequestList.remove(&req->lnode);
                loader->loadRequestPool.deleteInstance(req);
            }

            node = next;
        }

        return nullptr;
    }

    static UnloadResourceRequest* popFirstUnloadRequest(IncrLoader* loader, LoaderGroup* group)
    {
        UnloadResourceRequest::LNode* node = group->unloadRequestList.getFirst();
        if (node) {
            UnloadResourceRequest* req = node->data;
            group->unloadRequestList.remove(node);
            return req;
        }

        return nullptr;
    }

    static void processUnloadRequests(IncrLoader* loader, LoaderGroup* group)
    {
        while (1) {
            UnloadResourceRequest* unloadReq = popFirstUnloadRequest(loader, group);
            if (unloadReq) {
                assert(unloadReq->handle.isValid());
                uint32_t refcount = asset::getRefCount(unloadReq->handle);
                asset::unload(unloadReq->handle);
                loader->unloadRequestPool.deleteInstance(unloadReq);
                // RefCount == 1 means that the reseource is actually unloaded, so we return the function to process more resources later
                // If RefCount > 1 we continue releasing resources because actually it didn't take any process
                if (refcount == 1)
                    break;
            } else {
                break;
            }
        }
    }

    static void stepLoadGroupSequential(IncrLoader* loader, LoaderGroup* group)
    {
        // Process Loads
        LoadResourceRequest* req = getFirstLoadRequest(loader, group);
        if (req) {
            // check the previous request, it should be loaded in order to proceed to next one
            LoadResourceRequest::LNode* prev = req->lnode.prev;
            if (prev && asset::getState(*prev->data->pHandle) == AssetState::LoadInProgress)
                return;

            *req->pHandle = asset::load(req->name, req->uri.cstr(), req->userParams, req->flags, req->objAlloc);
            if (!req->pHandle->isValid()) {
                // Something went wrong, remove the request from the list
                group->loadRequestList.remove(&req->lnode);
                loader->loadRequestPool.deleteInstance(req);
            }
        }

        processUnloadRequests(loader, group);
    }

    static void stepLoadGroupDeltaFrame(IncrLoader* loader, LoaderGroup* group)
    {
        group->frameCount++;
        if (group->frameCount >= group->scheme.frameDelta) {
            LoadResourceRequest* req = getFirstLoadRequest(loader, group);
            if (req) {
                *req->pHandle = asset::load(req->name, req->uri.cstr(), req->userParams, req->flags, req->objAlloc);
                if (!req->pHandle->isValid()) {
                    // Something went wrong, remove the request from the list
                    group->loadRequestList.remove(&req->lnode);
                    loader->loadRequestPool.deleteInstance(req);
                }
            }

            group->frameCount = 0;
            processUnloadRequests(loader, group);
        }

    }

    static void stepLoadGroupDeltaTime(IncrLoader* loader, LoaderGroup* group, float dt)
    {
        group->elapsedTime += dt;
        if (group->elapsedTime >= group->scheme.deltaTime) {
            LoadResourceRequest* req = getFirstLoadRequest(loader, group);
            if (req) {
                *req->pHandle = asset::load(req->name, req->uri.cstr(), req->userParams, req->flags, req->objAlloc);
                if (!req->pHandle->isValid()) {
                    // Something went wrong, remove the request from the list
                    group->loadRequestList.remove(&req->lnode);
                    loader->loadRequestPool.deleteInstance(req);
                }
            }

            processUnloadRequests(loader, group);
            group->elapsedTime = 0;
        }
    }

    void asset::stepIncrLoader(IncrLoader* loader, float dt)
    {
        // move through groups and progress their loading
        uint16_t count = loader->groupPool.getCount();
        for (uint16_t i = 0; i < count; i++) {
            uint16_t groupHandle = loader->groupPool.handleAt(i);
            LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, groupHandle);

            if (!group->loadRequestList.isEmpty() || !group->unloadRequestList.isEmpty()) {
                switch (group->scheme.type) {
                case IncrLoadingScheme::LoadSequential:
                    stepLoadGroupSequential(loader, group);
                    break;
                case IncrLoadingScheme::LoadDeltaFrame:
                    stepLoadGroupDeltaFrame(loader, group);
                    break;
                case IncrLoadingScheme::LoadDeltaTime:
                    stepLoadGroupDeltaTime(loader, group, dt);
                    break;
                }
            }
        }
    }

} // namespace tee