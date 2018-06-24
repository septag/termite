#include "pch.h"

#include "incremental_loader.h"

#include "bxx/path.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/handle_pool.h"

#define REQUEST_POOL_SIZE 128

namespace tee {

    struct LoadAssetRequest
    {
        typedef bx::List<LoadAssetRequest*>::Node LNode;

        char name[32];
        bx::Path uri;
        uint8_t userParams[TEE_ASSET_MAX_USERPARAM_SIZE];
        AssetFlags::Bits flags;
        bx::AllocatorI* objAlloc;

        AssetHandle* pHandle;

        LNode lnode;

        LoadAssetRequest() :
            lnode(this)
        {
            objAlloc = nullptr;
        }
    };

    struct UnloadAssetRequest
    {
        typedef bx::List<UnloadAssetRequest*>::Node LNode;
        AssetHandle handle;
        LNode lnode;

        UnloadAssetRequest() :
            lnode(this)
        {
        }
    };

    struct LoaderGroup
    {
        IncrLoadingScheme scheme;
        bx::List<LoadAssetRequest*> loadRequestList;
        bx::List<UnloadAssetRequest*> unloadRequestList;
        bx::List<LoadAssetRequest*> loadFailedList;

        float elapsedTime;
        int frameCount;
        int retryCount;
    };

    struct IncrLoader
    {
        bx::AllocatorI* alloc;
        bx::Pool<LoadAssetRequest> loadRequestPool;
        bx::Pool<UnloadAssetRequest> unloadRequestPool;
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
        BX_ASSERT(loader);
        BX_ASSERT(loader->alloc);

        loader->unloadRequestPool.destroy();
        loader->loadRequestPool.destroy();
        loader->groupPool.destroy();
        BX_DELETE(loader->alloc, loader);
    }

    void asset::beginIncrLoadGroup(IncrLoader* loader, const IncrLoadingScheme& scheme)
    {
        BX_ASSERT(loader);
        IncrLoaderGroupHandle handle = IncrLoaderGroupHandle(loader->groupPool.newHandle());
        BX_ASSERT(handle.isValid());
        LoaderGroup* group = BX_PLACEMENT_NEW(loader->groupPool.getHandleData(0, handle), LoaderGroup);
        bx::memCopy(&group->scheme, &scheme, sizeof(scheme));
        group->frameCount = 0;
        group->elapsedTime = 0;
        group->retryCount = 0;

        loader->curGroupHandle = handle;
    }

    IncrLoaderGroupHandle asset::endIncrLoadGroup(IncrLoader* loader)
    {
        BX_ASSERT(loader);
        IncrLoaderGroupHandle handle = loader->curGroupHandle;

        loader->curGroupHandle = IncrLoaderGroupHandle();
        return handle;
    }

    void asset::deleteIncrloadGroup(IncrLoader* loader, IncrLoaderGroupHandle handle)
    {
        BX_ASSERT(handle.isValid(), "Invalid handle");
        LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, handle);

        if (!group->loadRequestList.isEmpty()) {
            // just empty the list and delete the group
            bx::List<LoadAssetRequest*>::Node* node = group->loadRequestList.getFirst();
            while (node) {
                bx::List<LoadAssetRequest*>::Node* next = node->next;
                loader->loadRequestPool.deleteInstance(node->data);
                node = next;
            }
        }


        if (!group->unloadRequestList.isEmpty()) {
            // just empty the list and delete the group
            bx::List<UnloadAssetRequest*>::Node* node = group->unloadRequestList.getFirst();
            while (node) {
                bx::List<UnloadAssetRequest*>::Node* next = node->next;
                loader->unloadRequestPool.deleteInstance(node->data);
                node = next;
            }
        }


        if (!group->loadFailedList.isEmpty()) {
            // just empty the list and delete the group
            bx::List<LoadAssetRequest*>::Node* node = group->loadFailedList.getFirst();
            while (node) {
                bx::List<LoadAssetRequest*>::Node* next = node->next;
                group->loadFailedList.remove(node);
                loader->loadRequestPool.deleteInstance(node->data);
                node = next;
            }
        }

        loader->groupPool.freeHandle(handle);
    }

    bool asset::isLoadDone(IncrLoader* loader, IncrLoaderGroupHandle handle, IncrLoaderFlags::Bits flags)
    {
        BX_ASSERT(loader);
        BX_ASSERT(handle.isValid());

        LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, handle);
        bool done = group->loadRequestList.isEmpty() & group->unloadRequestList.isEmpty();
        if (done) {
            if ((flags & IncrLoaderFlags::RetryFailed) && !group->loadFailedList.isEmpty() && group->retryCount < 2) {
                // The flag is set and there are some failed resources
                // Reactivate the group and load those resources once again
                loader->curGroupHandle = handle;
                bx::List<LoadAssetRequest*>::Node* node = group->loadFailedList.getFirst();
                while (node) {
                    bx::List<LoadAssetRequest*>::Node* next = node->next;
                    LoadAssetRequest* req = node->data;
                    load(loader, req->pHandle, req->name, req->uri.cstr(), req->userParams, 
                         req->flags|AssetFlags::Reload, req->objAlloc);

                    group->loadFailedList.remove(node);
                    loader->loadRequestPool.deleteInstance(req);
                    node = next;
                }
                loader->curGroupHandle = IncrLoaderGroupHandle();
                ++group->retryCount;
                done = false;   // so we are not done yet
            } else if (flags & IncrLoaderFlags::DeleteGroup) {
                if (!group->loadFailedList.isEmpty()) {
                    // just empty the list and delete the group
                    bx::List<LoadAssetRequest*>::Node* node = group->loadFailedList.getFirst();
                    while (node) {
                        bx::List<LoadAssetRequest*>::Node* next = node->next;
                        group->loadFailedList.remove(node);
                        loader->loadRequestPool.deleteInstance(node->data);
                        node = next;
                    }
                }
                loader->groupPool.freeHandle(handle);
            }
        } 
        return done;
    }

    void asset::load(IncrLoader* loader,
                     AssetHandle* pHandle,
                     const char* name, const char* uri, const void* userParams,
                     AssetFlags::Bits flags /*= 0*/, bx::AllocatorI* objAlloc/* = nullptr*/)
    {
        BX_ASSERT(loader->curGroupHandle.isValid());
        BX_ASSERT(pHandle);

        pHandle->reset();

        // create a new request
        LoadAssetRequest* req = loader->loadRequestPool.newInstance();
        if (!req) {
            BX_ASSERT(false);
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
        BX_ASSERT(group);
        group->loadRequestList.addToEnd(&req->lnode);
    }

    void asset::unload(IncrLoader* loader, AssetHandle handle)
    {
        BX_ASSERT(loader->curGroupHandle.isValid());
        BX_ASSERT(handle.isValid());

        UnloadAssetRequest* req = loader->unloadRequestPool.newInstance();
        if (!req) {
            BX_ASSERT(false);
            return;
        }

        req->handle = handle;

        // Add to Group
        LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, loader->curGroupHandle);
        BX_ASSERT(group);
        group->unloadRequestList.addToEnd(&req->lnode);
    }

    static LoadAssetRequest* getFirstLoadRequest(IncrLoader* loader, LoaderGroup* group)
    {
        LoadAssetRequest::LNode* node = group->loadRequestList.getFirst();
        while (node) {
            LoadAssetRequest* req = node->data;
            LoadAssetRequest::LNode* next = node->next;

            if (!req->pHandle->isValid())
                return req;

            // Remove items that are not in progress
            AssetState::Enum astate = asset::getState(*req->pHandle);
            if (astate != AssetState::LoadInProgress) {
                group->loadRequestList.remove(&req->lnode);

                if (astate == AssetState::LoadOk)
                    loader->loadRequestPool.deleteInstance(req);
                else if (astate == AssetState::LoadFailed)
                    group->loadFailedList.addToEnd(&req->lnode);    // Keep track of failed ones so we can report them later
            }

            node = next;
        }

        return nullptr;
    }

    static UnloadAssetRequest* popFirstUnloadRequest(IncrLoader* loader, LoaderGroup* group)
    {
        UnloadAssetRequest::LNode* node = group->unloadRequestList.getFirst();
        if (node) {
            UnloadAssetRequest* req = node->data;
            group->unloadRequestList.remove(node);
            return req;
        }

        return nullptr;
    }

    static void processUnloadRequests(IncrLoader* loader, LoaderGroup* group)
    {
        while (1) {
            UnloadAssetRequest* unloadReq = popFirstUnloadRequest(loader, group);
            if (unloadReq) {
                BX_ASSERT(unloadReq->handle.isValid());
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
        LoadAssetRequest* req = getFirstLoadRequest(loader, group);
        if (req) {
            // check the previous request, it should be loaded in order to proceed to next one
            LoadAssetRequest::LNode* prev = req->lnode.prev;
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
            LoadAssetRequest* req = getFirstLoadRequest(loader, group);
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
            LoadAssetRequest* req = getFirstLoadRequest(loader, group);
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