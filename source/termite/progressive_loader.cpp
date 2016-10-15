#include "pch.h"

#include "progressive_loader.h"

#include "bxx/path.h"
#include "bxx/linked_list.h"
#include "bxx/pool.h"
#include "bxx/indexed_pool.h"

#define REQUEST_POOL_SIZE 128

using namespace termite;

struct LoadResourceRequest
{
    typedef bx::ListNode<LoadResourceRequest*> LNode;

    char name[32];
    bx::Path uri;
    uint8_t userParams[T_RESOURCE_MAX_USERPARAM_SIZE];
    ResourceFlag::Bits flags;

    ResourceHandle* pHandle;

    LNode lnode;

    LoadResourceRequest() :
        lnode(this)
    {
    }
};

struct LoaderGroup
{
    LoadingScheme scheme;
    bx::List<LoadResourceRequest*> requestList;

    float elapsedTime;
    int frameCount;
};

namespace termite
{
    struct ProgressiveLoader
    {
        bx::AllocatorI* alloc;
        ResourceLibHelper resLib;
        bx::Pool<LoadResourceRequest> requestPool;
        bx::IndexedPool groupPool;
        LoaderGroupHandle curGroupHandle;

        ProgressiveLoader(ResourceLib* _resLib, bx::AllocatorI* _alloc) :
            alloc(_alloc),
            resLib(_resLib)
        {
        }
    };
}

ProgressiveLoader* termite::createProgressiveLoader(ResourceLib* resLib, bx::AllocatorI* alloc)
{
    ProgressiveLoader* loader = BX_NEW(alloc, ProgressiveLoader)(resLib, alloc);

    const uint32_t itemSizes[] = {
        sizeof(LoaderGroup)
    };
    if (!loader->requestPool.create(REQUEST_POOL_SIZE, alloc) ||
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

    loader->requestPool.destroy();
    loader->groupPool.destroy();
    BX_DELETE(loader->alloc, loader);
}

void termite::beginLoaderGroup(ProgressiveLoader* loader, const LoadingScheme& scheme)
{
    assert(loader);
    LoaderGroupHandle handle = LoaderGroupHandle(loader->groupPool.newHandle());
    assert(handle.isValid());
    LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, handle.value);
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

    LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, handle.value);
    memset(group, 0x00, sizeof(LoaderGroup));
    bool done = group->requestList.isEmpty();
    if (done) {
        loader->groupPool.freeHandle(handle.value);
    }
    return done;
}

void termite::loadResource(ProgressiveLoader* loader, 
                           ResourceHandle* pHandle,
                           const char* name, const char* uri, const void* userParams, ResourceFlag::Bits flags /*= 0*/)
{
    assert(loader->curGroupHandle.isValid());
    assert(pHandle);

    // create a new request
    LoadResourceRequest* req = loader->requestPool.newInstance();
    if (!req) {
        assert(false);
        return;
    }

    bx::strlcpy(req->name, name, sizeof(req->name));
    req->uri = uri;
    int paramSize = loader->resLib.getResourceParamSize(name);
    if (paramSize && userParams) {
        memcpy(req->userParams, userParams, paramSize);
    } else {
        memset(req->userParams, 0x00, sizeof(req->userParams));
    }
    req->flags = flags;
    req->pHandle = pHandle;

    // Add to group
    LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, loader->curGroupHandle.value);
    assert(group);
    group->requestList.addToEnd(&req->lnode);
}

void termite::unloadResource(ProgressiveLoader* loader, ResourceHandle handle)
{
    assert(loader->curGroupHandle.isValid());
    assert(false);  // not implemented
}

static LoadResourceRequest* getFirstLoadRequest(ProgressiveLoader* loader, LoaderGroup* group)
{
    LoadResourceRequest::LNode* node = group->requestList.getFirst();
    while (node) {
        LoadResourceRequest* req = node->data;
        LoadResourceRequest::LNode* next = node->next;

        if (!req->pHandle->isValid())
            return req;

        // Remove items that are not in progress
        if (loader->resLib.getResourceLoadState(*req->pHandle) != ResourceLoadState::LoadInProgress) {
            group->requestList.remove(&req->lnode);
            loader->requestPool.deleteInstance(req);
        }

        node = next;
    }

    return nullptr;
}

static void stepLoadGroupSequential(ProgressiveLoader* loader, LoaderGroup* group)
{
    LoadResourceRequest* req = getFirstLoadRequest(loader, group);
    if (req) {
        // check the previous request, it should be loaded in order to proceed to next one
        LoadResourceRequest::LNode* prev = req->lnode.prev;
        if (prev && loader->resLib.getResourceLoadState(*prev->data->pHandle) == ResourceLoadState::LoadInProgress)
            return;

        *req->pHandle = loader->resLib.loadResource(req->name, req->uri.cstr(), req->userParams, req->flags);
        if (!req->pHandle->isValid()) {
            // Something went wrong, remove the request from the list
            group->requestList.remove(&req->lnode);
            loader->requestPool.deleteInstance(req);
        }
    }
}

static void stepLoadGroupDeltaFrame(ProgressiveLoader* loader, LoaderGroup* group)
{
    group->frameCount++;
    if (group->frameCount >= group->scheme.frameDelta) {
        LoadResourceRequest* req = getFirstLoadRequest(loader, group);
        *req->pHandle = loader->resLib.loadResource(req->name, req->uri.cstr(), req->userParams, req->flags);
        if (!req->pHandle->isValid()) {
            // Something went wrong, remove the request from the list
            group->requestList.remove(&req->lnode);
            loader->requestPool.deleteInstance(req);
        }

        group->frameCount = 0;
    }
}

static void stepLoadGroupDeltaTime(ProgressiveLoader* loader, LoaderGroup* group, float dt)
{
    group->elapsedTime += dt;
    if (group->elapsedTime >= group->scheme.deltaTime) {
        LoadResourceRequest* req = getFirstLoadRequest(loader, group);
        *req->pHandle = loader->resLib.loadResource(req->name, req->uri.cstr(), req->userParams, req->flags);
        if (!req->pHandle->isValid()) {
            // Something went wrong, remove the request from the list
            group->requestList.remove(&req->lnode);
            loader->requestPool.deleteInstance(req);
        }

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

        if (!group->requestList.isEmpty()) {
            switch (group->scheme.type) {
            case LoadingScheme::Sequential:
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

