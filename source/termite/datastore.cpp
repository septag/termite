#include "pch.h"

#include "datastore.h"
#include "datastore_driver.h"
#include "driver_mgr.h"

#include "bxx/logger.h"
#include "bxx/array.h"
#include "bxx/hash_table.h"
#include "bx/hash.h"
#include "bxx/path.h"

using namespace termite;

#define MAX_USERPARAM_SIZE 256

struct ResourceTypeData
{
    char name[32];
    dsResourceCallbacks* callbacks;
    int userParamsSize;
};

struct Resource
{
    dsResourceHandle handle;
    dsResourceCallbacks* callbacks;
    uint8_t userParams[MAX_USERPARAM_SIZE];
    bx::Path uri;
    uint32_t refcount;
    uintptr_t obj;
    uint32_t typeNameHash;
    uint32_t paramsHash;
};

struct AsyncLoadRequest
{
    dsResourceHandle handle;
    dsFlag flags;
};

namespace termite
{
    class dsDataStore : public dsDriverCallbacks
    {
    public:
        dsInitFlag flags;
        dsDriver* driver;
        dsOperationMode opMode;
        bx::ArrayWithPop<ResourceTypeData> resourceTypes;
        bx::HashTableInt resourceTypesTable;    // hash(name) -> index in resourceTypes
        bx::ArrayWithPop<Resource> resources;
        bx::HashTableInt resourcesTable;        // hash(uri+params) -> index in resources
        bx::ArrayWithPop<AsyncLoadRequest> asyncLoads;
        bx::HashTableInt asyncLoadsTable;       // hash(uri) -> index in asyncLoads
        bx::MultiHashTableInt hotLoadsTable;    // hash(uri) -> list of indexes in resources

    public:
        dsDataStore() : 
            resourceTypesTable(bx::HashTableType::Mutable),
            resourcesTable(bx::HashTableType::Mutable),
            asyncLoadsTable(bx::HashTableType::Mutable),
            hotLoadsTable(bx::HashTableType::Mutable)
        {
            driver = nullptr;
            opMode = dsOperationMode::Async;
            flags = dsInitFlag::None;
        }

        void onOpenError(const char* uri) override;
        void onReadError(const char* uri) override;
        void onReadComplete(const char* uri, MemoryBlock* mem) override;
        void onModified(const char* uri) override;

        void onWriteError(const char* uri) override  {}
        void onWriteComplete(const char* uri, size_t size) override  {}
        void onOpenStream(dsStream* stream) override  {}
        void onReadStream(dsStream* stream, MemoryBlock* mem) override {}
        void onCloseStream(dsStream* stream) override {}
        void onWriteStream(dsStream* stream, size_t size) override {}
    };
}   // namespace st


termite::dsDataStore* termite::dsCreate(termite::dsInitFlag flags, termite::dsDriver* driver)
{
    assert(driver);

    BX_BEGINP("Initializing DataStore with Driver '%s'", drvGetName(drvFindHandleByPtr(driver)));
    bx::AllocatorI* alloc = coreGetAlloc();
    dsDataStore* ds = BX_NEW(alloc, dsDataStore);
    if (!ds) {
        BX_END_FATAL();
        return nullptr;
    }

    ds->driver = driver;   
    ds->opMode = driver->getOpMode();
    ds->flags = flags;

    if (ds->opMode == dsOperationMode::Async)   
        driver->setCallbacks(ds);

    if (!ds->resourceTypes.create(20, 20, alloc) || !ds->resourceTypesTable.create(20, alloc)) {
        BX_END_FATAL();
        return nullptr;
    }

    if (!ds->resources.create(512, 2048, alloc) || !ds->resourcesTable.create(512, alloc)) {
        BX_END_FATAL();
        return nullptr;
    }

    if (!ds->asyncLoads.create(128, 256, alloc) || !ds->asyncLoadsTable.create(256, alloc)) {
        BX_END_FATAL();
        return nullptr;
    }

    if (!ds->hotLoadsTable.create(128, alloc, alloc)) {
        BX_END_FATAL();
        return nullptr;
    }

    BX_END_OK();
    return ds;
}

void termite::dsDestroy(dsDataStore* ds)
{
    if (!ds) {
        assert(false);
        return;
    }

    BX_BEGINP("Shutting down DataStore");

    // Got to clear the callbacks of the driver if DataStore is overloading it
    if (ds->driver && ds->driver->getCallbacks() == ds) {
        ds->driver->setCallbacks(nullptr);
    }

    ds->hotLoadsTable.destroy();

    ds->asyncLoads.destroy();
    ds->asyncLoadsTable.destroy();

    ds->resourceTypesTable.destroy();
    ds->resourceTypes.destroy();

    ds->resources.destroy();
    ds->resourcesTable.destroy();

    BX_DELETE(coreGetAlloc(), ds);
    BX_END_OK();
}

dsResourceTypeHandle termite::dsRegisterResourceType(dsDataStore* ds, const char* name, dsResourceCallbacks* callbacks, 
                                                int userParamsSize /*= 0*/)
{
    if (ds == nullptr)
        ds = coreGetDefaultDataStore();

    if (userParamsSize > MAX_USERPARAM_SIZE)
        return T_INVALID_HANDLE;

    int index;
    ResourceTypeData* tdata = ds->resourceTypes.push(&index);
    bx::strlcpy(tdata->name, name, sizeof(tdata->name));
    tdata->callbacks = callbacks;
    tdata->userParamsSize = userParamsSize;
    
    dsResourceTypeHandle handle;
    handle.idx = (uint16_t)index;

    ds->resourceTypesTable.add(bx::hashMurmur2A(tdata->name, (uint32_t)strlen(tdata->name)), index);

    return handle;
}

void termite::dsUnregisterResourceType(dsDataStore* ds, dsResourceTypeHandle handle)
{
    if (T_ISVALID(handle)) {
        assert(handle.idx < ds->resourceTypes.getCount());

        ResourceTypeData* tdata = ds->resourceTypes.pop(handle.idx);
        
        int index = ds->resourceTypesTable.find(bx::hashMurmur2A(tdata->name, (uint32_t)strlen(tdata->name)));
        if (index != -1)
            ds->resourceTypesTable.remove(index);
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

static dsResourceHandle newResource(dsDataStore* ds, dsResourceCallbacks* callbacks, const char* uri, const void* userParams, 
                                    int userParamsSize, uintptr_t obj, uint32_t typeNameHash)
{
    int index;
    Resource* rs = ds->resources.push(&index);
    if (!rs)
        return T_INVALID_HANDLE;
    memset(rs, 0x00, sizeof(Resource));

    rs->handle.idx = (uint16_t)index;
    rs->uri = uri;
    rs->refcount = 1;
    rs->callbacks = callbacks;
    rs->obj = obj;
    rs->typeNameHash = typeNameHash;

    if (userParams) {
        memcpy(rs->userParams, userParams, userParamsSize);
        rs->paramsHash = bx::hashMurmur2A(userParams, userParamsSize);
    }

    ds->resourcesTable.add(hashResource(uri, userParams, userParamsSize), index);

    // Register for hot-loading
    // A URI may contain several resources (different load params), so we have to reload them all
    if ((uint8_t)(ds->flags & dsInitFlag::HotLoading)) {
        ds->hotLoadsTable.add(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)), index);
    }

    return rs->handle;
}

static void deleteResource(dsDataStore* ds, dsResourceHandle handle)
{
    assert(handle.idx < ds->resources.getCount());

    Resource* rs = ds->resources.itemPtr(handle.idx);

    // Unregister from hot-loading
    if ((uint8_t)(ds->flags & dsInitFlag::HotLoading)) {
        int index = ds->hotLoadsTable.find(bx::hashMurmur2A(rs->uri.cstr(), (uint32_t)rs->uri.getLength()));
        if (index != -1) {
            bx::MultiHashTableInt::Node* node = ds->hotLoadsTable.getNode(index);
            bx::MultiHashTableInt::Node* foundNode = nullptr;
            while (node) {
                if (ds->resources[node->value].paramsHash == rs->paramsHash) {
                    foundNode = node;
                    break;
                }
                node = node->next;
            }

            if (foundNode)
                ds->hotLoadsTable.remove(index, foundNode);
        }
    }
    ds->resources.pop(handle.idx);

    // Unload resource and invalidate the handle (just to set a flag that it's unloaded)
    rs->callbacks->unloadObj(rs->obj);
    rs->handle = T_INVALID_HANDLE;

    int tIdx = ds->resourcesTable.find(bx::hashMurmur2A(rs->uri.cstr(), rs->uri.getLength()));
    if (tIdx != -1)
        ds->resourcesTable.remove(tIdx);
}

static dsResourceHandle addResource(dsDataStore* ds, dsResourceCallbacks* callbacks, const char* uri, const void* userParams,
                                    int userParamsSize, uintptr_t obj, dsResourceHandle overrideHandle, uint32_t typeNameHash)
{
    dsResourceHandle handle = overrideHandle;
    if (T_ISVALID(handle)) {
        Resource* rs = ds->resources.itemPtr(handle.idx);
        if (T_ISVALID(rs->handle))
            rs->callbacks->unloadObj(rs->obj);
        
        rs->handle = handle;
        rs->uri = uri;
        rs->obj = obj;
        rs->callbacks = callbacks;
        memcpy(rs->userParams, userParams, userParamsSize);
    } else {
        handle = newResource(ds, callbacks, uri, userParams, userParamsSize, obj, typeNameHash);
    }

    return handle;
}

static termite::dsResourceHandle loadResource(dsDataStore* ds, uint32_t nameHash, const char* uri, const void* userParams,
                                         dsFlag flags)
{
    if (ds == nullptr)
        ds = coreGetDefaultDataStore();

    dsResourceHandle handle = T_INVALID_HANDLE;
    dsResourceHandle overrideHandle = T_INVALID_HANDLE;

    // Find resource Type
    int resTypeIdx = ds->resourceTypesTable.find(nameHash);
    if (resTypeIdx == -1) {
        BX_WARN("ResourceType for '%s' not found in DataStore", uri);
        return T_INVALID_HANDLE;
    }
    const ResourceTypeData& tdata = ds->resourceTypes[ds->resourceTypesTable.getValue(resTypeIdx)];

    // Find the possible already loaded handle by uri+params hash value
    int rresult = ds->resourcesTable.find(hashResource(uri, userParams, tdata.userParamsSize));
    if (rresult != -1) {
        handle.idx = (uint16_t)ds->resourcesTable.getValue(rresult);
    }

    // Resource already exists ?
    if (T_ISVALID(handle)) {
        if ((uint8_t)(flags & dsFlag::Reload)) {
            // Override the handle and set handle to INVALID in order to reload it later below
            overrideHandle = handle;
            handle = T_INVALID_HANDLE;
        } else {
            // No Reload flag is set, just add the reference count and return
            ds->resources[handle.idx].refcount++;
        }
    }

    // Load the resource from DataStoreDriver for the first time
    if (!T_ISVALID(handle)) {
        if (ds->opMode == dsOperationMode::Async) {
            // Add resource with a fake object
            handle = addResource(ds, tdata.callbacks, uri, userParams, tdata.userParamsSize,
                                 tdata.callbacks->getDefaultAsyncObj(), overrideHandle, nameHash);

            // Register async request
            int reqIdx;
            AsyncLoadRequest* req = ds->asyncLoads.push(&reqIdx);
            if (!req) {
                deleteResource(ds, handle);
                return T_INVALID_HANDLE;
            }
            req->handle = handle;
            req->flags = flags;
            ds->asyncLoadsTable.add(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)), reqIdx);

            // Load the file, result will be called in onReadComplete
            ds->driver->read(uri);
        } else {
            // Load the file
            MemoryBlock* mem = ds->driver->read(uri);
            if (!mem) {
                BX_WARN("Opening resource '%s' failed", uri);
                BX_WARN(errGetString());
                if (T_ISVALID(overrideHandle))
                    deleteResource(ds, overrideHandle);
                return T_INVALID_HANDLE;
            }

            dsResourceTypeParams params;
            params.uri = uri;
            params.userParams = userParams;
            uintptr_t obj;
            if (!tdata.callbacks->loadObj(mem, params, &obj)) {
                coreReleaseMemory(mem);
                BX_WARN("Loading resource '%s' failed", uri);
                BX_WARN(errGetString());
                if (T_ISVALID(overrideHandle))
                    deleteResource(ds, overrideHandle);
                return T_INVALID_HANDLE;
            }
            coreReleaseMemory(mem);

            handle = addResource(ds, tdata.callbacks, uri, userParams, tdata.userParamsSize, obj, overrideHandle, nameHash);

            // Trigger onReload callback
            if ((uint8_t)(flags & dsFlag::Reload)) {
                tdata.callbacks->onReload(handle);
            }

            BX_VERBOSE("Loaded (%s): '%s'", tdata.name, uri);
        }
    }

    return handle;
}

termite::dsResourceHandle termite::dsLoadResource(dsDataStore* ds, const char* name, const char* uri, const void* userParams,
                                        dsFlag flags)
{
    return loadResource(ds, bx::hashMurmur2A(name, (uint32_t)strlen(name)), uri, userParams, flags);
}

void termite::dsUnloadResource(dsDataStore* ds, dsResourceHandle handle)
{
    assert(T_ISVALID(handle));

    if (ds == nullptr)
        ds = coreGetDefaultDataStore();
    assert(handle.idx < ds->resources.getCount());

    Resource* rs = ds->resources.itemPtr(handle.idx);
    rs->refcount--;
    if (rs->refcount == 0) {
        // Unregister from async loading
        if (ds->opMode == dsOperationMode::Async) {
            int aIdx = ds->asyncLoadsTable.find(bx::hashMurmur2A(rs->uri.cstr(), (uint32_t)rs->uri.getLength()));
            if (aIdx != -1) {
                ds->asyncLoads.pop(ds->asyncLoadsTable.getValue(aIdx));
                ds->asyncLoadsTable.remove(aIdx);
            }
        }

        deleteResource(ds, handle);
    }
}

uintptr_t termite::dsGetObj(dsDataStore* ds, dsResourceHandle handle)
{
    assert(T_ISVALID(handle));

    if (ds == nullptr)
        ds = coreGetDefaultDataStore();
    assert(handle.idx < ds->resources.getCount());

    const Resource& rs = ds->resources[handle.idx];
    return rs.obj;
}

// Async
void termite::dsDataStore::onOpenError(const char* uri)
{
    int r = this->asyncLoadsTable.find(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)));
    if (r != -1) {
        int index = this->asyncLoadsTable.getValue(r);
        AsyncLoadRequest* areq = this->asyncLoads.pop(index);
        BX_WARN("Opening resource '%s' failed", uri);
        if (T_ISVALID(areq->handle)) {
            deleteResource(this, areq->handle);
            areq->handle = T_INVALID_HANDLE;
        }

        this->asyncLoadsTable.remove(r);
    }
}

void termite::dsDataStore::onReadError(const char* uri)
{
    int r = this->asyncLoadsTable.find(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)));
    if (r != -1) {
        int index = this->asyncLoadsTable.getValue(r);
        AsyncLoadRequest* areq = this->asyncLoads.pop(index);
        BX_WARN("Reading resource '%s' failed", uri);
        if (T_ISVALID(areq->handle)) {
            deleteResource(this, areq->handle);
            areq->handle = T_INVALID_HANDLE;
        }

        this->asyncLoadsTable.remove(r);
    }
}

void termite::dsDataStore::onReadComplete(const char* uri, MemoryBlock* mem)
{
    int r = this->asyncLoadsTable.find(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)));
    if (r != -1) {
        int index = this->asyncLoadsTable.getValue(r);
        AsyncLoadRequest* areq = this->asyncLoads.pop(index);

        assert(T_ISVALID(areq->handle));
        Resource* rs = this->resources.itemPtr(areq->handle.idx);

        // Load using the callback
        dsResourceTypeParams params;
        params.uri = uri;
        params.userParams = rs->userParams;
        uintptr_t obj;
        bool loadResult = rs->callbacks->loadObj(mem, params, &obj);
        coreReleaseMemory(mem);
        this->asyncLoadsTable.remove(r);

        if (!loadResult) {
            BX_WARN("Loading resource '%s' failed", uri);
            BX_WARN(errGetString());
            if (T_ISVALID(areq->handle)) {
                deleteResource(this, areq->handle);
                areq->handle = T_INVALID_HANDLE;
            }
            return;
        }

        // Update the obj 
        rs->obj = obj;

        // Trigger onReload callback
        if ((uint8_t)(areq->flags & dsFlag::Reload)) {
            rs->callbacks->onReload(rs->handle);
        }
    } else {
        coreReleaseMemory(mem);
    }
}

void termite::dsDataStore::onModified(const char* uri)
{
    // Hot Loading
    int index = this->hotLoadsTable.find(bx::hashMurmur2A(uri, (uint32_t)strlen(uri)));
    if (index != -1) {
        bx::MultiHashTableInt::Node* node = this->hotLoadsTable.getNode(index);
        // Recurse resources and reload them with their params
        const Resource& rs = this->resources[node->value];
        loadResource(this, rs.typeNameHash, rs.uri.cstr(), rs.userParams, dsFlag::Reload);
    }
}

