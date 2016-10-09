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

    ResourceHandle handle;

    LNode lnode;
};

struct LoaderGroup
{
    LoadingScheme scheme;
    LoadResourceRequest::LNode* requestList;
};

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

    loader->curGroupHandle = handle;
}

termite::LoaderGroupHandle termite::endLoaderGroup(ProgressiveLoader* loader)
{
    assert(loader);
    loader->curGroupHandle = LoaderGroupHandle();
}

bool termite::isLoaderGroupDone(ProgressiveLoader* loader, LoaderGroupHandle handle)
{
    assert(loader);
    LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, handle.value);
    return group->requestList == nullptr;
}

void termite::loadResource(ProgressiveLoader* loader, const char* name, const char* uri, const void* userParams, ResourceFlag::Bits flags /*= 0*/)
{
    assert(loader->curGroupHandle.isValid());

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
        req->userParams = nullptr;
    }
    req->flags = flags;
    req->handle.reset();

    // Add to group
    LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(0, loader->curGroupHandle.value);
    assert(group);
    bx::addListNodeToEnd<LoaderGroup*>(&group->requestList, &req->lnode, req);
}

void termite::unloadResource(ProgressiveLoader* loader, ResourceHandle handle)
{
    assert(loader->curGroupHandle.isValid());
    assert(false);  // not implemented
}

void termite::stepLoader(ProgressiveLoader* loader, float dt)
{
    // move through groups and progress their loading
    uint16_t count = loader->groupPool.getCount();
    for (uint16_t i = 0; i < count; i++) {
        LoaderGroup* group = loader->groupPool.getHandleData<LoaderGroup>(loader->groupPool.handleAt(i));
        switch (group->scheme.type) {
        case LoadingScheme::Sequential:
            break;
        case LoadingScheme::LoadDeltaFrame:
            break;
        case LoadingScheme::LoadDeltaTime:
            break;
        }
    }
}

