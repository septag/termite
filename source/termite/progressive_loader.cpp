#include "pch.h"

#include "progressive_loader.h"

#include "bxx/path.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/handle_pool.h"

#define REQUEST_POOL_SIZE 128

using namespace termite;

struct LoadResourceRequest
{
    typedef bx::List<LoadResourceRequest*>::Node LNode;

    char name[32];
    bx::Path uri;
    uint8_t userParams[T_RESOURCE_MAX_USERPARAM_SIZE];
    ResourceFlag::Bits flags;
    bx::AllocatorI* objAlloc;

    ResourceHandle* pHandle;

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
    ResourceHandle handle;
    LNode lnode;

    UnloadResourceRequest() :
        lnode(this)
    {
    }
};

struct LoaderGroup
{
    LoadingScheme scheme;
    bx::List<LoadResourceRequest*> loadRequestList;
    bx::List<UnloadResourceRequest*> unloadRequestList;

    float elapsedTime;
    int frameCount;
};

namespace termite
{
    struct ProgressiveLoader
    {
        bx::AllocatorI* alloc;
        bx::Pool<LoadResourceRequest> loadRequestPool;
        bx::Pool<UnloadResourceRequest> unloadRequestPool;
        bx::HandlePool groupPool;
        LoaderGroupHandle curGroupHandle;

        ProgressiveLoader(bx::AllocatorI* _alloc) :
            alloc(_alloc)
        {
        }
    };
}

ProgressiveLoader* termite::createProgressiveLoader(bx::AllocatorI* alloc)
{
    ProgressiveLoader* loader = BX_NEW(alloc, ProgressiveLoader)(alloc);

    const uint32_t itemSizes[] = {
        sizeof(LoaderGroup)
    };
    if (!loader->loadRequestPool.create(REQUEST_POOL_SIZE, alloc) ||
        !loader->unloadRequestPool.create(REQUEST_POOL_SIZE, alloc) ||
        !loader->groupPool.create(itemSizes, 1, 32, 32, alloc))
    {
        BX_DELETE(alloc, loader);
        return nullptr;
    }

    return loader;
}

void termite::destroyProgressiveLoader(ProgressiveLoader* loader)
{
    assert(loader);
    assert(loader->alloc);

    loader->unloadRequestPool.destroy();
    loader->loadRequestPool.destroy();
    loader->groupPool.destroy();
    BX_DELETE(loader->alloc, loader);
}

void termite::beginLoaderGroup(ProgressiveLoader* loader, const LoadingScheme& scheme)
{
    assert(loader);
    LoaderGroupHandle handle = LoaderGroupHandle(loader->groupPool.newHandle());
    assert(handle.isValid());
    LoaderGroup* group = new(loader->groupPool.getHandleData(0, handle)) LoaderGroup();
    memcpy(&group->scheme, &scheme, sizeof(scheme));
    group->frameCount = 0;
    group->elapsedTime = 0;

    loader->curGroupHandle = handle;
}

LoaderGroupHandle termite::endLoaderGroup(ProgressiveLoader* loader)
{
    assert(loader);
    LoaderGroupHandle handle = loader->curGroupHandle;

    loader->curGroupHandle = LoaderGroupHandle();
    return handle;
}

bool termite::checkLoaderGroupDone(ProgressiveLoader* loader, LoaderGroupHandle handle)
{
    assert(loader);
    assert(handle.isValid());

    LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, handle);
    bool done = group->loadRequestList.isEmpty() && group->unloadRequestList.isEmpty();
    if (done)
        loader->groupPool.freeHandle(handle);
    return done;
}

void termite::loadResource(ProgressiveLoader* loader, 
                           ResourceHandle* pHandle,
                           const char* name, const char* uri, const void* userParams, 
                           ResourceFlag::Bits flags /*= 0*/, bx::AllocatorI* objAlloc/* = nullptr*/)
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
    int paramSize = getResourceParamSize(name);
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

void termite::unloadResource(ProgressiveLoader* loader, ResourceHandle handle)
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

static LoadResourceRequest* getFirstLoadRequest(ProgressiveLoader* loader, LoaderGroup* group)
{
    LoadResourceRequest::LNode* node = group->loadRequestList.getFirst();
    while (node) {
        LoadResourceRequest* req = node->data;
        LoadResourceRequest::LNode* next = node->next;

        if (!req->pHandle->isValid())
            return req;

        // Remove items that are not in progress
        if (getResourceLoadState(*req->pHandle) != ResourceLoadState::LoadInProgress) {
            group->loadRequestList.remove(&req->lnode);
            loader->loadRequestPool.deleteInstance(req);
        }

        node = next;
    }

    return nullptr;
}

static UnloadResourceRequest* popFirstUnloadRequest(ProgressiveLoader* loader, LoaderGroup* group)
{
    UnloadResourceRequest::LNode* node = group->unloadRequestList.getFirst();
    if (node) {
        UnloadResourceRequest* req = node->data;
        group->unloadRequestList.remove(node);
        return req;
    }

    return nullptr;
}

static void processUnloadRequests(ProgressiveLoader* loader, LoaderGroup* group)
{
    while (1) {
        UnloadResourceRequest* unloadReq = popFirstUnloadRequest(loader, group);
        if (unloadReq) {
            assert(unloadReq->handle.isValid());
            uint32_t refcount = getResourceRefCount(unloadReq->handle);
            unloadResource(unloadReq->handle);
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

static void stepLoadGroupSequential(ProgressiveLoader* loader, LoaderGroup* group)
{
    // Process Loads
    LoadResourceRequest* req = getFirstLoadRequest(loader, group);
    if (req) {
        // check the previous request, it should be loaded in order to proceed to next one
        LoadResourceRequest::LNode* prev = req->lnode.prev;
        if (prev && getResourceLoadState(*prev->data->pHandle) == ResourceLoadState::LoadInProgress)
            return;

        *req->pHandle = loadResource(req->name, req->uri.cstr(), req->userParams, req->flags, req->objAlloc);
        if (!req->pHandle->isValid()) {
            // Something went wrong, remove the request from the list
            group->loadRequestList.remove(&req->lnode);
            loader->loadRequestPool.deleteInstance(req);
        }
    }

    processUnloadRequests(loader, group);
}

static void stepLoadGroupDeltaFrame(ProgressiveLoader* loader, LoaderGroup* group)
{
    group->frameCount++;
    if (group->frameCount >= group->scheme.frameDelta) {
        LoadResourceRequest* req = getFirstLoadRequest(loader, group);
        if (req) {
            *req->pHandle = loadResource(req->name, req->uri.cstr(), req->userParams, req->flags, req->objAlloc);
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

static void stepLoadGroupDeltaTime(ProgressiveLoader* loader, LoaderGroup* group, float dt)
{
    group->elapsedTime += dt;
    if (group->elapsedTime >= group->scheme.deltaTime) {
        LoadResourceRequest* req = getFirstLoadRequest(loader, group);
        if (req) {
            *req->pHandle = loadResource(req->name, req->uri.cstr(), req->userParams, req->flags, req->objAlloc);
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

void termite::stepLoader(ProgressiveLoader* loader, float dt)
{
    // move through groups and progress their loading
    uint16_t count = loader->groupPool.getCount();
    for (uint16_t i = 0; i < count; i++) {
        uint16_t groupHandle = loader->groupPool.handleAt(i);
        LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, groupHandle);

        if (!group->loadRequestList.isEmpty() || !group->unloadRequestList.isEmpty()) {
            switch (group->scheme.type) {
            case LoadingScheme::LoadSequential:
                stepLoadGroupSequential(loader, group);
                break;
            case LoadingScheme::LoadDeltaFrame:
                stepLoadGroupDeltaFrame(loader, group);
                break;
            case LoadingScheme::LoadDeltaTime:
                stepLoadGroupDeltaTime(loader, group, dt);
                break;
            }
        }
    }
}

