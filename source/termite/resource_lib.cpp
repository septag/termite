#include "pch.h"

#include "resource_lib.h"
#include "io_driver.h"

#include "bxx/logger.h"
#include "bxx/array.h"
#include "bxx/hash_table.h"
#include "bx/hash.h"
#include "bxx/path.h"

#include "../include_common/folder_png.h"

using namespace termite;

struct ResourceTypeData
{
    char name[32];
    ResourceCallbacksI* callbacks;
    int userParamsSize;
    uintptr_t failObj;
};

struct Resource
{
    ResourceHandle handle;
    ResourceCallbacksI* callbacks;
    uint8_t userParams[T_RESOURCE_MAX_USERPARAM_SIZE];
    bx::Path uri;
    uint32_t refcount;
    uintptr_t obj;
    uint32_t typeNameHash;
    uint32_t paramsHash;
    ResourceLoadState::Enum loadState;
};

struct AsyncLoadRequest
{
    ResourceLib* resLib;
    ResourceHandle handle;
    ResourceFlag::Bits flags;
};

namespace termite
{
    class ResourceLib : public IoDriverEventsI
    {
    public:
        ResourceLibInitFlag::Bits flags;
        IoDriverApi* driver;
        IoOperationMode opMode;
        bx::ArrayWithPop<ResourceTypeData> resourceTypes;
        bx::HashTableInt resourceTypesTable;    // hash(name) -> index in resourceTypes
        bx::ArrayWithPop<Resource> resources;
        bx::HashTableInt resourcesTable;        // hash(uri+params) -> index in resources
        bx::ArrayWithPop<AsyncLoadRequest> asyncLoads;
        bx::HashTableInt asyncLoadsTable;       // hash(uri) -> index in asyncLoads
        bx::MultiHashTableInt hotLoadsTable;    // hash(uri) -> list of indexes in resources
		bx::Pool<bx::MultiHashTableInt::Node> hotLoadsNodePool;
        FileModifiedCallback modifiedCallback;
        void* fileModifiedUserParam;
        bx::AllocatorI* alloc;

    public:
        ResourceLib(bx::AllocatorI* _alloc) : 
            resourceTypesTable(bx::HashTableType::Mutable),
            resourcesTable(bx::HashTableType::Mutable),
            asyncLoadsTable(bx::HashTableType::Mutable),
            hotLoadsTable(bx::HashTableType::Mutable),
            alloc(_alloc)
        {
            driver = nullptr;
            opMode = IoOperationMode::Async;
            flags = ResourceLibInitFlag::None;
            modifiedCallback = nullptr;
            fileModifiedUserParam = nullptr;
        }

        void onOpenError(const char* uri) override;
        void onReadError(const char* uri) override;
        void onReadComplete(const char* uri, MemoryBlock* mem) override;
        void onModified(const char* uri) override;

        void onWriteError(const char* uri) override  {}
        void onWriteComplete(const char* uri, size_t size) override  {}
        void onOpenStream(IoStream* stream) override  {}
        void onReadStream(IoStream* stream, MemoryBlock* mem) override {}
        void onCloseStream(IoStream* stream) override {}
        void onWriteStream(IoStream* stream, size_t size) override {}
    };
}   // namespace termite


termite::ResourceLib* termite::createResourceLib(ResourceLibInitFlag::Bits flags, IoDriverApi* driver, bx::AllocatorI* alloc)
{
	assert(driver);

	ResourceLib* resLib = BX_NEW(alloc, ResourceLib)(alloc);
	if (!resLib)
		return nullptr;

	resLib->driver = driver;
	resLib->opMode = driver->getOpMode();
	resLib->flags = flags;

	if (resLib->opMode == IoOperationMode::Async)
		driver->setCallbacks(resLib);

	if (!resLib->resourceTypes.create(20, 20, alloc) || !resLib->resourceTypesTable.create(20, alloc)) {
		destroyResourceLib(resLib);
		return nullptr;
	}

	if (!resLib->resources.create(512, 2048, alloc) || !resLib->resourcesTable.create(512, alloc)) {
		destroyResourceLib(resLib);
		return nullptr;
	}

	if (!resLib->asyncLoads.create(128, 256, alloc) || !resLib->asyncLoadsTable.create(256, alloc)) {
		destroyResourceLib(resLib);
		return nullptr;
	}

	if (!resLib->hotLoadsNodePool.create(128, alloc) ||
		!resLib->hotLoadsTable.create(128, alloc, &resLib->hotLoadsNodePool)) {
        destroyResourceLib(resLib);
        return nullptr;
    }

    return resLib;
}

void termite::destroyResourceLib(ResourceLib* resLib)
{
    assert(resLib);

    // Got to clear the callbacks of the driver if DataStore is overloading it
    if (resLib->driver && resLib->driver->getCallbacks() == resLib) {
        resLib->driver->setCallbacks(nullptr);
    }

    resLib->hotLoadsTable.destroy();
	resLib->hotLoadsNodePool.destroy();

    resLib->asyncLoads.destroy();
    resLib->asyncLoadsTable.destroy();

    resLib->resourceTypesTable.destroy();
    resLib->resourceTypes.destroy();

    resLib->resources.destroy();
    resLib->resourcesTable.destroy();

    BX_DELETE(resLib->alloc, resLib);
}

void termite::setFileModifiedCallback(ResourceLib* resLib, FileModifiedCallback callback, void* userParam)
{
    resLib->modifiedCallback = callback;
    resLib->fileModifiedUserParam = userParam;
}

IoDriverApi* termite::getResourceLibIoDriver(ResourceLib* resLib)
{
    return resLib->driver;
}

ResourceTypeHandle termite::registerResourceType(ResourceLib* resLib, const char* name, ResourceCallbacksI* callbacks,
                                                int userParamsSize /*= 0*/, uintptr_t failObj /*=0*/)
{
    assert(resLib); 
    if (userParamsSize > T_RESOURCE_MAX_USERPARAM_SIZE)
        return ResourceTypeHandle();

    int index;
    ResourceTypeData* tdata = resLib->resourceTypes.push(&index);
    bx::strlcpy(tdata->name, name, sizeof(tdata->name));
    tdata->callbacks = callbacks;
    tdata->userParamsSize = userParamsSize;
    tdata->failObj = failObj;
    
    ResourceTypeHandle handle;
    handle.value = (uint16_t)index;

    resLib->resourceTypesTable.add(bx::hashMurmur2A(tdata->name, (uint32_t)strlen(tdata->name)), index);

    return handle;
}

void termite::unregisterResourceType(ResourceLib* resLib, ResourceTypeHandle handle)
{
    if (handle.isValid()) {
        ResourceTypeData* tdata = resLib->resourceTypes.pop(handle.value);
        
        int index = resLib->resourceTypesTable.find(bx::hashMurmur2A(tdata->name, (uint32_t)strlen(tdata->name)));
        if (index != -1)
            resLib->resourceTypesTable.remove(index);
    }
}

inline uint32_t hashResource(const char* uri, const void* userParams, int userParamsSize)
{
    // Hash uri + params
    bx::HashMurmur2A hash;
    hash.begin();
    hash.add(uri, (int)strlen(uri));
    hash.add(userParams, userParamsSize);
    return hash.end();
}

static ResourceHandle newResource(ResourceLib* resLib, ResourceCallbacksI* callbacks, const char* uri, const void* userParams, 
                                    int userParamsSize, uintptr_t obj, uint32_t typeNameHash)
{
    int index;
    Resource* rs = resLib->resources.push(&index);
    if (!rs)
        return ResourceHandle();
    memset(rs, 0x00, sizeof(Resource));

    rs->handle.value = (uint16_t)index;
    rs->uri = uri;
    rs->refcount = 1;
    rs->callbacks = callbacks;
    rs->obj = obj;
    rs->typeNameHash = typeNameHash;

    if (userParams) {
        memcpy(rs->userParams, userParams, userParamsSize);
        rs->paramsHash = bx::hashMurmur2A(userParams, userParamsSize);
    }

    resLib->resourcesTable.add(hashResource(uri, userParams, userParamsSize), index);

    // Register for hot-loading
    // A URI may contain several resources (different load params), so we have to reload them all
    if ((uint8_t)(resLib->flags & ResourceLibInitFlag::HotLoading)) {
        resLib->hotLoadsTable.add(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)), index);
    }

    return rs->handle;
}

static void deleteResource(ResourceLib* resLib, ResourceHandle handle)
{
    Resource* rs = resLib->resources.itemPtr(handle.value);

    // Unregister from hot-loading
    if ((uint8_t)(resLib->flags & ResourceLibInitFlag::HotLoading)) {
        int index = resLib->hotLoadsTable.find(bx::hashMurmur2A(rs->uri.cstr(), (uint32_t)rs->uri.getLength()));
        if (index != -1) {
            bx::MultiHashTableInt::Node* node = resLib->hotLoadsTable.getNode(index);
            bx::MultiHashTableInt::Node* foundNode = nullptr;
            while (node) {
                if (resLib->resources[node->value].paramsHash == rs->paramsHash) {
                    foundNode = node;
                    break;
                }
                node = node->next;
            }

            if (foundNode)
                resLib->hotLoadsTable.remove(index, foundNode);
        }
    }
    resLib->resources.pop(handle.value);

    // Unload resource and invalidate the handle (just to set a flag that it's unloaded)
    rs->callbacks->unloadObj(resLib, rs->obj);

    int tIdx = resLib->resourcesTable.find(bx::hashMurmur2A(rs->uri.cstr(), rs->uri.getLength()));
    if (tIdx != -1)
        resLib->resourcesTable.remove(tIdx);
}

static ResourceHandle addResource(ResourceLib* resLib, ResourceCallbacksI* callbacks, const char* uri, const void* userParams,
                                  int userParamsSize, uintptr_t obj, ResourceHandle overrideHandle, uint32_t typeNameHash)
{
    ResourceHandle handle = overrideHandle;
    if (handle.isValid()) {
        Resource* rs = resLib->resources.itemPtr(handle.value);
        if (rs->handle.isValid())
            rs->callbacks->unloadObj(resLib, rs->obj);
        
        rs->handle = handle;
        rs->uri = uri;
        rs->obj = obj;
        rs->callbacks = callbacks;
        memcpy(rs->userParams, userParams, userParamsSize);
    } else {
        handle = newResource(resLib, callbacks, uri, userParams, userParamsSize, obj, typeNameHash);
    }

    return handle;
}

static void setResourceLoadFlag(ResourceLib* resLib, ResourceHandle resHandle, ResourceLoadState::Enum flag) 
{
    Resource* res = resLib->resources.itemPtr(resHandle.value);
    res->loadState = flag;
}

static ResourceHandle loadResourceHashed(ResourceLib* resLib, uint32_t nameHash, const char* uri, const void* userParams,
                                         ResourceFlag::Bits flags)
{
    assert(resLib);
    ResourceHandle handle;
    ResourceHandle overrideHandle;

    // Find resource Type
    int resTypeIdx = resLib->resourceTypesTable.find(nameHash);
    if (resTypeIdx == -1) {
        BX_WARN("ResourceType for '%s' not found in DataStore", uri);
        return ResourceHandle();
    }
    const ResourceTypeData& tdata = resLib->resourceTypes[resLib->resourceTypesTable.getValue(resTypeIdx)];

    // Find the possible already loaded handle by uri+params hash value
    int rresult = resLib->resourcesTable.find(hashResource(uri, userParams, tdata.userParamsSize));
    if (rresult != -1) {
        handle.value = (uint16_t)resLib->resourcesTable.getValue(rresult);
    }

    // Resource already exists ?
    if (handle.isValid()) {
        if (flags & ResourceFlag::Reload) {
            // Override the handle and set handle to INVALID in order to reload it later below
            overrideHandle = handle;
            handle = ResourceHandle();
        } else {
            // No Reload flag is set, just add the reference count and return
            resLib->resources[handle.value].refcount++;
        }
    }

    // Load the resource for the first time
    if (!handle.isValid()) {
        if (resLib->opMode == IoOperationMode::Async) {
            // Add resource with an empty object
            handle = addResource(resLib, tdata.callbacks, uri, userParams, tdata.userParamsSize,
                                 tdata.callbacks->getDefaultAsyncObj(resLib), overrideHandle, nameHash);
            setResourceLoadFlag(resLib, handle, ResourceLoadState::LoadInProgress);

            // Register async request
            int reqIdx;
            AsyncLoadRequest* req = resLib->asyncLoads.push(&reqIdx);
            if (!req) {
                deleteResource(resLib, handle);
                return ResourceHandle();
            }
            req->resLib = resLib;
            req->handle = handle;
            req->flags = flags;
            resLib->asyncLoadsTable.add(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)), reqIdx);

            // Load the file, result will be called in onReadComplete
            resLib->driver->read(uri);
        } else {
            // Load the file
            MemoryBlock* mem = resLib->driver->read(uri);
            if (!mem) {
                BX_WARN("Opening resource '%s' failed", uri);
                BX_WARN(getErrorString());
                if (overrideHandle.isValid())
                    deleteResource(resLib, overrideHandle);
                return ResourceHandle();
            }

            ResourceTypeParams params;
            params.uri = uri;
            params.userParams = userParams;
            params.flags = flags;
            uintptr_t obj;
            bool loaded = tdata.callbacks->loadObj(resLib, mem, params, &obj);
            releaseMemoryBlock(mem);

            if (!loaded)    {
                BX_WARN("Loading resource '%s' failed", uri);
                BX_WARN(getErrorString());
                obj = tdata.failObj;
            }

            handle = addResource(resLib, tdata.callbacks, uri, userParams, tdata.userParamsSize, obj, overrideHandle, nameHash);
            setResourceLoadFlag(resLib, handle, loaded ? ResourceLoadState::LoadOk : ResourceLoadState::LoadFailed);

            // Trigger onReload callback
            if (flags & ResourceFlag::Reload) {
                tdata.callbacks->onReload(resLib, handle);
            }
        }
    }

    return handle;
}

static ResourceHandle loadResourceHashedInMem(ResourceLib* resLib, uint32_t nameHash, const char* uri, const MemoryBlock* mem, 
                                              const void* userParams, ResourceFlag::Bits flags)
{
    assert(resLib);
    ResourceHandle handle;
    ResourceHandle overrideHandle;

    // Find resource Type
    int resTypeIdx = resLib->resourceTypesTable.find(nameHash);
    if (resTypeIdx == -1) {
        BX_WARN("ResourceType for '%s' not found in DataStore", uri);
        return ResourceHandle();
    }
    const ResourceTypeData& tdata = resLib->resourceTypes[resLib->resourceTypesTable.getValue(resTypeIdx)];

    // Find the possible already loaded handle by uri+params hash value
    int rresult = resLib->resourcesTable.find(hashResource(uri, userParams, tdata.userParamsSize));
    if (rresult != -1) {
        handle.value = (uint16_t)resLib->resourcesTable.getValue(rresult);
    }

    // Resource already exists ?
    if (handle.isValid()) {
        if (flags & ResourceFlag::Reload) {
            // Override the handle and set handle to INVALID in order to reload it later below
            overrideHandle = handle;
            handle = ResourceHandle();
        } else {
            // No Reload flag is set, just add the reference count and return
            resLib->resources[handle.value].refcount++;
        }
    }

    // Load the resource  for the first time
    if (!handle.isValid()) {
        assert(mem);

        ResourceTypeParams params;
        params.uri = uri;
        params.userParams = userParams;
        params.flags = flags;
        uintptr_t obj;
        bool loaded = tdata.callbacks->loadObj(resLib, mem, params, &obj);

        if (!loaded)    {
            BX_WARN("Loading resource '%s' failed", uri);
            BX_WARN(getErrorString());
            obj = tdata.failObj;
        }

        handle = addResource(resLib, tdata.callbacks, uri, userParams, tdata.userParamsSize, obj, overrideHandle, nameHash);
        setResourceLoadFlag(resLib, handle, loaded ? ResourceLoadState::LoadOk : ResourceLoadState::LoadFailed);

        // Trigger onReload callback
        if (flags & ResourceFlag::Reload) {
            tdata.callbacks->onReload(resLib, handle);
        }
    }

    return handle;
}

ResourceHandle termite::loadResource(ResourceLib* resLib, const char* name, const char* uri, const void* userParams,
                                     ResourceFlag::Bits flags)
{
    return loadResourceHashed(resLib, bx::hashMurmur2A(name, (uint32_t)strlen(name)), uri, userParams, flags);
}

ResourceHandle termite::loadResourceFromMem(ResourceLib* resLib, const char* name, const char* uri, const MemoryBlock* mem, 
                                            const void* userParams /*= nullptr*/, ResourceFlag::Bits flags /*= ResourceFlag::None*/)
{
    return loadResourceHashedInMem(resLib, bx::hashMurmur2A(name, (uint32_t)strlen(name)), uri, mem, userParams, flags);
}

void termite::unloadResource(ResourceLib* resLib, ResourceHandle handle)
{
    assert(resLib);
    assert(handle.isValid());

    Resource* rs = resLib->resources.itemPtr(handle.value);
    rs->refcount--;
    if (rs->refcount == 0) {
        // Unregister from async loading
        if (resLib->opMode == IoOperationMode::Async) {
            int aIdx = resLib->asyncLoadsTable.find(bx::hashMurmur2A(rs->uri.cstr(), (uint32_t)rs->uri.getLength()));
            if (aIdx != -1) {
                resLib->asyncLoads.pop(resLib->asyncLoadsTable.getValue(aIdx));
                resLib->asyncLoadsTable.remove(aIdx);
            }
        }

        deleteResource(resLib, handle);
    }
}

uintptr_t termite::getResourceObj(ResourceLib* resLib, ResourceHandle handle)
{
    assert(resLib);
    assert(handle.isValid());

    const Resource& rs = resLib->resources[handle.value];
    return rs.obj;
}

ResourceLoadState::Enum termite::getResourceLoadState(ResourceLib* resLib, ResourceHandle handle)
{
    assert(resLib);
    if (handle.isValid())
        return resLib->resources[handle.value].loadState;
    else
        return ResourceLoadState::LoadFailed;
}

int termite::getResourceParamSize(ResourceLib* resLib, const char* name)
{
    assert(resLib);

    int typeIdx = resLib->resourceTypesTable.find(bx::hashMurmur2A(name, (uint32_t)strlen(name)));
    if (typeIdx != -1)
        return resLib->resourceTypes[resLib->resourceTypesTable.getValue(typeIdx)].userParamsSize;
    else
        return 0;
}

// Async
void termite::ResourceLib::onOpenError(const char* uri)
{
    int r = this->asyncLoadsTable.find(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)));
    if (r != -1) {
        int index = this->asyncLoadsTable.getValue(r);
        AsyncLoadRequest* areq = this->asyncLoads.pop(index);
        BX_WARN("Opening resource '%s' failed", uri);

        if (areq->handle.isValid()) {
            setResourceLoadFlag(areq->resLib, areq->handle, ResourceLoadState::LoadFailed);

            // Set fail obj to resource
            Resource* res = areq->resLib->resources.itemPtr(areq->handle.value);
            int typeIdx = areq->resLib->resourceTypesTable.find(res->typeNameHash);
            if (typeIdx != -1) {
                res->obj = areq->resLib->resourceTypes[areq->resLib->resourceTypesTable.getValue(typeIdx)].failObj;
            }
        }

        this->asyncLoadsTable.remove(r);
    }
}

void termite::ResourceLib::onReadError(const char* uri)
{
    int r = this->asyncLoadsTable.find(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)));
    if (r != -1) {
        int index = this->asyncLoadsTable.getValue(r);
        AsyncLoadRequest* areq = this->asyncLoads.pop(index);
        BX_WARN("Reading resource '%s' failed", uri);

        if (areq->handle.isValid()) {
            setResourceLoadFlag(areq->resLib, areq->handle, ResourceLoadState::LoadFailed);

            // Set fail obj to resource
            Resource* res = areq->resLib->resources.itemPtr(areq->handle.value);
            int typeIdx = areq->resLib->resourceTypesTable.find(res->typeNameHash);
            if (typeIdx != -1) {
                res->obj = areq->resLib->resourceTypes[areq->resLib->resourceTypesTable.getValue(typeIdx)].failObj;
            }
        }

        this->asyncLoadsTable.remove(r);
    }
}

void termite::ResourceLib::onReadComplete(const char* uri, MemoryBlock* mem)
{
    int r = this->asyncLoadsTable.find(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)));
    if (r != -1) {
        int index = this->asyncLoadsTable.getValue(r);
        AsyncLoadRequest* areq = this->asyncLoads.pop(index);

        assert(areq->handle.isValid());
        Resource* rs = this->resources.itemPtr(areq->handle.value);

        // Load using the callback
        ResourceTypeParams params;
        params.uri = uri;
        params.userParams = rs->userParams;
        params.flags = areq->flags;
        uintptr_t obj;
        bool loadResult = rs->callbacks->loadObj(areq->resLib, mem, params, &obj);
        releaseMemoryBlock(mem);
        this->asyncLoadsTable.remove(r);

        if (!loadResult) {
            BX_WARN("Loading resource '%s' failed", uri);
            BX_WARN(getErrorString());
            rs->loadState = ResourceLoadState::LoadFailed;

            // Set fail obj to resource
            int typeIdx = areq->resLib->resourceTypesTable.find(rs->typeNameHash);
            if (typeIdx != -1) {
                rs->obj = areq->resLib->resourceTypes[areq->resLib->resourceTypesTable.getValue(typeIdx)].failObj;
            }
            return;
        }

        // Update the obj 
        rs->obj = obj;
        rs->loadState = ResourceLoadState::LoadOk;

        // Trigger onReload callback
        if (areq->flags & ResourceFlag::Reload) {
            rs->callbacks->onReload(areq->resLib, rs->handle);
        }
    } else {
        releaseMemoryBlock(mem);
    }
}

void termite::ResourceLib::onModified(const char* uri)
{
    // Hot Loading
    int index = this->hotLoadsTable.find(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)));
    if (index != -1) {
        bx::MultiHashTableInt::Node* node = this->hotLoadsTable.getNode(index);
        // Recurse resources and reload them with their params
        const Resource& rs = this->resources[node->value];
        loadResourceHashed(this, rs.typeNameHash, rs.uri.cstr(), rs.userParams, ResourceFlag::Reload);
    }

    // Run callback
    if (this->modifiedCallback)
        this->modifiedCallback(this, uri, this->fileModifiedUserParam);
}

ResourceLib* getDefaultResourceLibPtr();    // fwd (core.cpp)
ResourceLibHelper termite::getDefaultResourceLib()
{
    return ResourceLibHelper(getDefaultResourceLibPtr());
}
