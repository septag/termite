#include "pch.h"

#include "assetlib.h"
#include "io_driver.h"
#include "internal.h"

#include "bx/hash.h"
#include "bxx/logger.h"
#include "bxx/hash_table.h"
#include "bxx/path.h"
#include "bxx/handle_pool.h"
#include "bxx/array.h"

#include "../include_common/folder_png.h"

namespace tee {
    struct AssetTypeData
    {
        char name[32];
        AssetLibCallbacksI* callbacks;
        int userParamsSize;
        uintptr_t failObj;
        uintptr_t asyncProgressObj;
    };

    struct Asset
    {
        bx::AllocatorI* objAlloc;
        AssetHandle handle;
        AssetLibCallbacksI* callbacks;
        uint8_t userParams[TEE_ASSET_MAX_USERPARAM_SIZE];
        bx::Path uri;
        uint32_t refcount;
        uintptr_t obj;
        size_t typeNameHash;
        uint32_t paramsHash;
        AssetState::Enum loadState;
        AssetState::Enum initLoadState;
    };

    struct AsyncLoadRequest
    {
        AssetHandle handle;
        AssetFlags::Bits flags;
    };

    struct AssetExtensionOverride
    {
        const char* ext;
        const char* replacement;
    };

    class AssetLib : public IoDriverEventsI
    {
    public:
        AssetLibInitFlags::Bits flags;
        IoDriver* driver;
        IoDriver* blockingDriver;
        IoOperationMode::Enum opMode;
        bx::HandlePool assetTypes;
        bx::HashTableUint16 assetTypesTable;    // hash(name) -> handle in assetTypes
        bx::HandlePool assets;
        bx::HashTableUint16 assetsTable;        // hash(uri+params) -> handle in assets
        bx::HandlePool asyncLoads;
        bx::HashTableUint16 asyncLoadsTable;       // hash(uri) -> handle in asyncLoads
        bx::MultiHashTable<uint16_t> hotLoadsTable;    // hash(uri) -> list of handles in assets
        bx::Pool<bx::MultiHashTable<uint16_t>::Node> hotLoadsNodePool;
        asset::AssetModifiedCallback modifiedCallback;
        void* fileModifiedUserParam;
        bx::AllocatorI* alloc;
        bool ignoreUnloadResourceCalls;
        bx::Array<AssetExtensionOverride> overrides;

    public:
        AssetLib(bx::AllocatorI* _alloc) :
            assetTypesTable(bx::HashTableType::Mutable),
            assetsTable(bx::HashTableType::Mutable),
            asyncLoadsTable(bx::HashTableType::Mutable),
            hotLoadsTable(bx::HashTableType::Mutable),
            alloc(_alloc)
        {
            driver = nullptr;
            blockingDriver = nullptr;
            opMode = IoOperationMode::Async;
            flags = AssetLibInitFlags::None;
            modifiedCallback = nullptr;
            fileModifiedUserParam = nullptr;
            ignoreUnloadResourceCalls = false;
        }

        virtual ~AssetLib()
        {
        }

        void onOpenError(const char* uri) override;
        void onReadError(const char* uri) override;
        void onReadComplete(const char* uri, MemoryBlock* mem) override;
        void onModified(const char* uri) override;

        void onWriteError(const char* uri) override {}
        void onWriteComplete(const char* uri, size_t size) override {}
        void onOpenStream(IoStream* stream) override {}
        void onReadStream(IoStream* stream, MemoryBlock* mem) override {}
        void onCloseStream(IoStream* stream) override {}
        void onWriteStream(IoStream* stream, size_t size) override {}
    };

    static AssetLib* gAssetLib = nullptr;

    bool asset::init(AssetLibInitFlags::Bits flags, IoDriver* driver, bx::AllocatorI* alloc, IoDriver* blockingDriver)
    {
        assert(driver);
        if (gAssetLib) {
            assert(0);
            return false;
        }

        AssetLib* assetLib = BX_NEW(alloc, AssetLib)(alloc);
        if (!assetLib)
            return false;
        gAssetLib = assetLib;

        assetLib->driver = driver;
        assetLib->opMode = driver->getOpMode();
        if (assetLib->opMode == IoOperationMode::Async)
            assetLib->blockingDriver = blockingDriver;
        else
            assetLib->blockingDriver = driver;

        assetLib->flags = flags;

        if (assetLib->opMode == IoOperationMode::Async)
            driver->setCallbacks(assetLib);

        if (!assetLib->assetTypes.create(sizeof(AssetTypeData), 20, 20, alloc) || 
            !assetLib->assetTypesTable.create(20, alloc) ||
            !assetLib->assets.create(sizeof(Asset), 512, 1024, alloc) ||
            !assetLib->assetsTable.create(512, alloc) || 
            !assetLib->asyncLoads.create(sizeof(AsyncLoadRequest), 32, 64, alloc) ||
            !assetLib->asyncLoadsTable.create(64, alloc) ||             
            !assetLib->overrides.create(10, 10, alloc))
        {
            return false;
        }

        if (flags & AssetLibInitFlags::HotLoading) {
            if (!assetLib->hotLoadsNodePool.create(128, alloc) ||
                !assetLib->hotLoadsTable.create(128, alloc, &assetLib->hotLoadsNodePool)) 
            {
                return false;
            }
        }

        return true;
    }

    void asset::shutdown()
    {
        AssetLib* assetLib = gAssetLib;
        if (!assetLib)
            return;

        assetLib->overrides.destroy();

        // Got to clear the callbacks of the driver if DataStore is overloading it
        if (assetLib->driver && assetLib->driver->getCallbacks() == assetLib) {
            assetLib->driver->setCallbacks(nullptr);
        }

        assetLib->hotLoadsTable.destroy();
        assetLib->hotLoadsNodePool.destroy();

        assetLib->asyncLoads.destroy();
        assetLib->asyncLoadsTable.destroy();

        assetLib->assetTypesTable.destroy();
        assetLib->assetTypes.destroy();

        assetLib->assets.destroy();
        assetLib->assetsTable.destroy();

        BX_DELETE(assetLib->alloc, assetLib);
        gAssetLib = nullptr;
    }

    void asset::setModifiedCallback(AssetModifiedCallback callback, void* userParam)
    {
        AssetLib* assetLib = gAssetLib;
        assetLib->modifiedCallback = callback;
        assetLib->fileModifiedUserParam = userParam;
    }

    IoDriver* asset::getIoDriver()
    {
        AssetLib* assetLib = gAssetLib;
        return assetLib->driver;
    }

    AssetTypeHandle asset::registerType(const char* name, AssetLibCallbacksI* callbacks,
                                        int userParamsSize /*= 0*/, uintptr_t failObj /*=0*/,
                                        uintptr_t asyncProgressObj/* = 0*/)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);

        if (userParamsSize > TEE_ASSET_MAX_USERPARAM_SIZE)
            return AssetTypeHandle();

        // Check if already have the type and modify it
        for (int i = 0, c = assetLib->assetTypes.getCount(); i < c; i++) {
            AssetTypeHandle thandle = assetLib->assetTypes.handleAt(i);
            AssetTypeData* tdata = assetLib->assetTypes.getHandleData<AssetTypeData>(0, thandle);
            if (strcmp(tdata->name, name) == 0) {
                tdata->callbacks = callbacks;
                tdata->userParamsSize = userParamsSize;
                tdata->failObj = failObj;
                tdata->asyncProgressObj = asyncProgressObj;
                return thandle;
            }
        }

        // 
        uint16_t tHandle = assetLib->assetTypes.newHandle();
        assert(tHandle != UINT16_MAX);
        AssetTypeData* tdata = assetLib->assetTypes.getHandleData<AssetTypeData>(0, tHandle);
        bx::strCopy(tdata->name, sizeof(tdata->name), name);
        tdata->callbacks = callbacks;
        tdata->userParamsSize = userParamsSize;
        tdata->failObj = failObj;
        tdata->asyncProgressObj = asyncProgressObj;

        AssetTypeHandle handle(tHandle);
        assetLib->assetTypesTable.add(tinystl::hash_string(tdata->name, strlen(tdata->name)), tHandle);

        return handle;
    }

    void asset::unregisterType(AssetTypeHandle handle)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);

        if (handle.isValid()) {
            AssetTypeData* tdata = assetLib->assetTypes.getHandleData<AssetTypeData>(0, handle);

            int index = assetLib->assetTypesTable.find(tinystl::hash_string(tdata->name, strlen(tdata->name)));
            if (index != -1)
                assetLib->assetTypesTable.remove(index);
            assetLib->assetTypes.freeHandle(handle);
        }
    }

    inline uint32_t hashAsset(const char* uri, const void* userParams, int userParamsSize, bx::AllocatorI* objAlloc)
    {
        uintptr_t objAllocu = uintptr_t(objAlloc);

        // Hash uri + params
        bx::HashMurmur2A hash;
        hash.begin();
        hash.add(uri, (int)strlen(uri));
        if (userParamsSize > 0)
            hash.add(userParams, userParamsSize);
        hash.add(&objAllocu, sizeof(objAllocu));
        return hash.end();
    }

    static AssetHandle newAsset(AssetLibCallbacksI* callbacks, const char* uri, const void* userParams,
                               int userParamsSize, uintptr_t obj, size_t typeNameHash, bx::AllocatorI* objAlloc)
    {
        AssetLib* assetLib = gAssetLib;

        uint16_t rHandle = assetLib->assets.newHandle();

        if (rHandle == UINT16_MAX) {
            BX_WARN("Out of Memory");
            return AssetHandle();
        }
        Asset* rs = assetLib->assets.getHandleData<Asset>(0, rHandle);
        bx::memSet(rs, 0x00, sizeof(Asset));

        rs->handle = AssetHandle(rHandle);
        rs->uri = uri;
        rs->refcount = 1;
        rs->callbacks = callbacks;
        rs->obj = obj;
        rs->typeNameHash = typeNameHash;
        rs->objAlloc = objAlloc;
        rs->initLoadState = AssetState::LoadFailed;

        if (userParamsSize > 0) {
            assert(userParamsSize);
            memcpy(rs->userParams, userParams, userParamsSize);
            rs->paramsHash = bx::hash<bx::HashMurmur2A>(userParams, userParamsSize);
        }

        assetLib->assetsTable.add(hashAsset(uri, userParams, userParamsSize, objAlloc), rHandle);

        // Register for hot-loading
        // A URI may contain several assets (different load params), so we have to reload them all
        if (assetLib->flags & AssetLibInitFlags::HotLoading) {
            assetLib->hotLoadsTable.add(tinystl::hash_string(uri, strlen(uri)), rHandle);
        }

        return rs->handle;
    }

    static void deleteAsset(AssetHandle handle, const AssetTypeData* tdata)
    {
        AssetLib* assetLib = gAssetLib;
        Asset* rs = assetLib->assets.getHandleData<Asset>(0, handle);

        // Unregister from hot-loading
        if (assetLib->flags & AssetLibInitFlags::HotLoading) {
            int index = assetLib->hotLoadsTable.find(tinystl::hash_string(rs->uri.cstr(), rs->uri.getLength()));
            if (index != -1) {
                bx::MultiHashTable<uint16_t>::Node* node = assetLib->hotLoadsTable.getNode(index);
                bx::MultiHashTable<uint16_t>::Node* foundNode = nullptr;
                while (node) {
                    if (assetLib->assets.getHandleData<Asset>(0, node->value)->paramsHash == rs->paramsHash) {
                        foundNode = node;
                        break;
                    }
                    node = node->next;
                }

                if (foundNode)
                    assetLib->hotLoadsTable.remove(index, foundNode);
            }
        }
        assetLib->assets.freeHandle(handle);

        // Unload asset and invalidate the handle (just to set a flag that it's unloaded)
        // Ignore async and fail objects
        if (rs->obj != tdata->asyncProgressObj && rs->obj != tdata->failObj)
            rs->callbacks->unloadObj(rs->obj, rs->objAlloc);

        int tIdx = assetLib->assetsTable.find(hashAsset(rs->uri.cstr(), rs->userParams, tdata->userParamsSize, rs->objAlloc));
        if (tIdx != -1)
            assetLib->assetsTable.remove(tIdx);
    }

    static AssetHandle addAsset(AssetLibCallbacksI* callbacks, const char* uri, const void* userParams,
                                int userParamsSize, uintptr_t obj, AssetHandle overrideHandle, size_t typeNameHash,
                                bx::AllocatorI* objAlloc)
    {
        AssetLib* assetLib = gAssetLib;
        AssetHandle handle = overrideHandle;
        if (handle.isValid()) {
            Asset* rs = assetLib->assets.getHandleData<Asset>(0, handle);

            // Unload previous asset object
            if (rs->handle.isValid() && rs->loadState == AssetState::LoadOk)
                rs->callbacks->unloadObj(rs->obj, rs->objAlloc);

            rs->handle = handle;
            rs->uri = uri;
            rs->obj = obj;
            rs->callbacks = callbacks;
            if (userParamsSize > 0)
                memcpy(rs->userParams, userParams, userParamsSize);
        } else {
            handle = newAsset(callbacks, uri, userParams, userParamsSize, obj, typeNameHash, objAlloc);
        }

        return handle;
    }

    static void setAssetLoadState(AssetHandle resHandle, AssetState::Enum state)
    {
        AssetLib* assetLib = gAssetLib;
        Asset* res = assetLib->assets.getHandleData<Asset>(0, resHandle.value);
        res->loadState = state;
        res->initLoadState = state;
    }

    static bx::Path getReplacementUri(const char* uri)
    {
        AssetLib* assetLib = gAssetLib;
        bx::Path newUri(uri);
        if (assetLib->overrides.getCount() > 0) {
            bx::Path ext = newUri.getFileExt();
            ext.toLower();
            int index = assetLib->overrides.find([&ext](const AssetExtensionOverride& value)->bool {
                return ext.isEqual(value.ext);
            });
            if (index != -1) {
                // Remove the extension and replace it with the new one
                char* e = strrchr(newUri.getBuffer(), '.');
                if (e)
                    *(e + 1) = 0;
                newUri += assetLib->overrides[index].replacement;
            }
        }
        return newUri;
    }

    static bx::Path getOriginalUri(const char* uri)
    {
        AssetLib* assetLib = gAssetLib;
        bx::Path newUri(uri);
        if (assetLib->overrides.getCount() > 0) {
            bx::Path ext = newUri.getFileExt();
            ext.toLower();
            int index = assetLib->overrides.find([&ext](const AssetExtensionOverride& value)->bool {
                return ext.isEqual(value.replacement);
            });
            if (index != -1) {
                // Remove the extension and replace it with the new one
                char* e = strrchr(newUri.getBuffer(), '.');
                if (e) {
                    *e = 0;
                    e = strrchr(newUri.getBuffer(), '.');
                    if (e)
                        *e = 0;
                }

                newUri += ".";
                newUri += assetLib->overrides[index].ext;
            }
        }
        return newUri;
    }

    static AssetHandle loadAssetHashed(size_t nameHash, const char* uri, const void* userParams, AssetFlags::Bits flags,
                                       bx::AllocatorI* objAlloc)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);
        AssetHandle handle;
        AssetHandle overrideHandle;

        if (uri[0] == 0) {
            BX_WARN("Cannot load asset with empty Uri");
            return AssetHandle();
        }

        // Find asset Type
        int resTypeIdx = assetLib->assetTypesTable.find(nameHash);
        if (resTypeIdx == -1) {
            BX_WARN("ResourceType for '%s' not found in DataStore", uri);
            return AssetHandle();
        }
        const AssetTypeData* tdata = assetLib->assetTypes.getHandleData<AssetTypeData>(0,
                                                                                        assetLib->assetTypesTable[resTypeIdx]);

        // Find the possible already loaded handle by uri+params hash value
        int rresult = assetLib->assetsTable.find(hashAsset(uri, userParams, tdata->userParamsSize, objAlloc));
        if (rresult != -1) {
            handle.value = (uint16_t)assetLib->assetsTable[rresult];
        }

        // Asset already exists ?
        if (handle.isValid()) {
            if (flags & AssetFlags::Reload) {
                // Override the handle and set handle to INVALID in order to reload it later below
                overrideHandle = handle;
                handle = AssetHandle();
            } else {
                // No Reload flag is set, just add the reference count and return
                assetLib->assets.getHandleData<Asset>(0, handle)->refcount++;
            }
        }

        bx::Path newUri = getReplacementUri(uri);

        // Load the asset for the first time
        if (!handle.isValid()) {
            if (assetLib->opMode == IoOperationMode::Async && !(flags & AssetFlags::ForceBlockLoad)) {
                // Add asset with an empty object
                handle = addAsset(tdata->callbacks, uri, userParams, tdata->userParamsSize,
                                     tdata->asyncProgressObj, overrideHandle, nameHash, objAlloc);
                setAssetLoadState(handle, AssetState::LoadInProgress);

                // Register async request
                uint16_t reqHandle = assetLib->asyncLoads.newHandle();
                if (reqHandle == UINT16_MAX) {
                    deleteAsset(handle, tdata);
                    return AssetHandle();
                }
                AsyncLoadRequest* req = assetLib->asyncLoads.getHandleData<AsyncLoadRequest>(0, reqHandle);
                req->handle = handle;
                req->flags = flags;
                assetLib->asyncLoadsTable.add(tinystl::hash_string(uri, strlen(uri)), reqHandle);

                // Load the file, result will be called in onReadComplete
                assetLib->driver->read(newUri.cstr(), IoPathType::Assets, 0);
            } else {
                // Load the file
                BX_ASSERT(assetLib->blockingDriver, "Blocking driver must be set int 'init'");
                MemoryBlock* mem = assetLib->blockingDriver->read(newUri.cstr(), IoPathType::Assets, 0);
                if (!mem) {
                    BX_WARN("Opening asset '%s' failed", newUri.cstr());
                    BX_WARN(err::getString());
                    if (overrideHandle.isValid())
                        deleteAsset(overrideHandle, tdata);
                    return AssetHandle();
                }

                AssetParams params;
                params.uri = newUri.cstr();
                params.userParams = userParams;
                params.flags = flags;
                uintptr_t obj;
                bool loaded = tdata->callbacks->loadObj(mem, params, &obj, objAlloc);
                releaseMemoryBlock(mem);

                if (!loaded) {
                    BX_WARN("Loading asset '%s' failed", newUri.cstr());
                    BX_WARN(err::getString());
                    obj = tdata->failObj;
                }

                handle = addAsset(tdata->callbacks, uri, userParams, tdata->userParamsSize, obj, overrideHandle, nameHash, objAlloc);
                setAssetLoadState(handle, loaded ? AssetState::LoadOk : AssetState::LoadFailed);

                // Trigger onReload callback
                if (flags & AssetFlags::Reload) {
                    tdata->callbacks->onReload(handle, objAlloc);
                }
            }
        }

        return handle;
    }

    static AssetHandle getAssetHandleInPlace(const AssetTypeData* tdata, size_t typeNameHash, const char* uri, uintptr_t obj)
    {
        AssetLib* assetLib = gAssetLib;
        void* userParams = nullptr;
        if (tdata->userParamsSize > 0) {
            userParams = alloca(tdata->userParamsSize);
            bx::memSet(userParams, 0x00, tdata->userParamsSize);
        }

        // Find the possible already loaded handle by uri+params hash value
        int rresult = assetLib->assetsTable.find(hashAsset(uri, userParams, tdata->userParamsSize, nullptr));
        if (rresult != -1) {
            uint16_t handleIdx = (uint16_t)assetLib->assetsTable.getValue(rresult);
            Asset* rs = assetLib->assets.getHandleData<Asset>(0, handleIdx);
            rs->refcount++;
            return AssetHandle(handleIdx);
        }

        // Create a new failed asset
        AssetHandle handle = newAsset(tdata->callbacks, uri, userParams, tdata->userParamsSize, tdata->failObj,
                                         typeNameHash, nullptr);
        setAssetLoadState(handle, AssetState::LoadFailed);
        return handle;
    }

    // Secretly create a fail object of the asset and return it
    AssetHandle asset::getFailHandle(const char* name)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);

        size_t typeNameHash = tinystl::hash_string(name, strlen(name));
        int resTypeIdx = assetLib->assetTypesTable.find(typeNameHash);
        if (resTypeIdx == -1) {
            BX_WARN("ResourceType '%s' not found in DataStore", name);
            return AssetHandle();
        }

        const AssetTypeData* tdata =
            assetLib->assetTypes.getHandleData<AssetTypeData>(0, assetLib->assetTypesTable.getValue(resTypeIdx));

        return getAssetHandleInPlace(tdata, typeNameHash, "[FAIL]", tdata->failObj);
    }

    AssetHandle asset::getAsyncHandle(const char* name)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);

        size_t typeNameHash = tinystl::hash_string(name, strlen(name));
        int resTypeIdx = assetLib->assetTypesTable.find(typeNameHash);
        if (resTypeIdx == -1) {
            BX_WARN("ResourceType '%s' not found in DataStore", name);
            return AssetHandle();
        }

        const AssetTypeData* tdata =
            assetLib->assetTypes.getHandleData<AssetTypeData>(0, assetLib->assetTypesTable.getValue(resTypeIdx));

        return getAssetHandleInPlace(tdata, typeNameHash, "[ASYNC]", tdata->asyncProgressObj);
    }

    AssetHandle asset::addRef(AssetHandle handle)
    {
        assert(handle.isValid());
        Asset* rs = gAssetLib->assets.getHandleData<Asset>(0, handle);
        rs->refcount++;
        return handle;
    }

    uint32_t asset::getRefCount(AssetHandle handle)
    {
        assert(handle.isValid());
        return gAssetLib->assets.getHandleData<Asset>(0, handle)->refcount;
    }

    static AssetHandle loadAssetHashedInMem(size_t nameHash, const char* uri, const MemoryBlock* mem,
                                            const void* userParams, AssetFlags::Bits flags, bx::AllocatorI* objAlloc)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);
        AssetHandle handle;
        AssetHandle overrideHandle;

        // Find asset Type
        int resTypeIdx = assetLib->assetTypesTable.find(nameHash);
        if (resTypeIdx == -1) {
            BX_WARN("ResourceType for '%s' not found in DataStore", uri);
            return AssetHandle();
        }
        const AssetTypeData& tdata = *assetLib->assetTypes.getHandleData<AssetTypeData>(0,
                                                                                         assetLib->assetTypesTable.getValue(resTypeIdx));

        // Find the possible already loaded handle by uri+params hash value
        int rresult = assetLib->assetsTable.find(hashAsset(uri, userParams, tdata.userParamsSize, objAlloc));
        if (rresult != -1) {
            handle.value = (uint16_t)assetLib->assetsTable.getValue(rresult);
        }

        // Asset already exists ?
        if (handle.isValid()) {
            if (flags & AssetFlags::Reload) {
                // Override the handle and set handle to INVALID in order to reload it later below
                overrideHandle = handle;
                handle = AssetHandle();
            } else {
                // No Reload flag is set, just add the reference count and return
                assetLib->assets.getHandleData<Asset>(0, handle)->refcount++;
            }
        }

        // Load the asset  for the first time
        if (!handle.isValid()) {
            assert(mem);

            AssetParams params;
            params.uri = uri;
            params.userParams = userParams;
            params.flags = flags;
            uintptr_t obj;
            bool loaded = tdata.callbacks->loadObj(mem, params, &obj, objAlloc);

            if (!loaded) {
                BX_WARN("Loading asset '%s' failed", uri);
                BX_WARN(err::getString());
                obj = tdata.failObj;
            }

            handle = addAsset(tdata.callbacks, uri, userParams, tdata.userParamsSize, obj, overrideHandle, nameHash, objAlloc);
            setAssetLoadState(handle, loaded ? AssetState::LoadOk : AssetState::LoadFailed);

            // Trigger onReload callback
            if (flags & AssetFlags::Reload) {
                tdata.callbacks->onReload(handle, objAlloc);
            }
        }

        return handle;
    }

    AssetHandle asset::load(const char* name, const char* uri, const void* userParams, AssetFlags::Bits flags, 
                            bx::AllocatorI* objAlloc)
    {
        return loadAssetHashed(tinystl::hash_string(name, strlen(name)), uri, userParams, flags, objAlloc);
    }

    AssetHandle asset::loadMem(const char* name, const char* uri, const MemoryBlock* mem,
                               const void* userParams /*= nullptr*/, AssetFlags::Bits flags /*= ResourceFlag::None*/,
                               bx::AllocatorI* objAlloc)
    {
        return loadAssetHashedInMem(tinystl::hash_string(name, strlen(name)), uri, mem, userParams, flags, objAlloc);
    }

    void asset::unload(AssetHandle handle)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);
        assert(handle.isValid());

        if (!assetLib->ignoreUnloadResourceCalls) {
            Asset* rs = assetLib->assets.getHandleData<Asset>(0, handle);
            rs->refcount--;
            if (rs->refcount == 0) {
                // Unregister from async loading
                if (assetLib->opMode == IoOperationMode::Async) {
                    int aIdx = assetLib->asyncLoadsTable.find(bx::hash<bx::HashMurmur2A>(rs->uri.cstr(), (uint32_t)rs->uri.getLength()));
                    if (aIdx != -1) {
                        assetLib->asyncLoads.freeHandle(assetLib->asyncLoadsTable.getValue(aIdx));
                        assetLib->asyncLoadsTable.remove(aIdx);
                    }
                }

                // delete asset and unload asset object
                int resTypeIdx = assetLib->assetTypesTable.find(rs->typeNameHash);
                if (resTypeIdx != -1) {
                    const AssetTypeData* tdata =
                        assetLib->assetTypes.getHandleData<AssetTypeData>(0, assetLib->assetTypesTable.getValue(resTypeIdx));
                    deleteAsset(handle, tdata);
                }
            }
        }
    }

    uintptr_t asset::getObj(AssetHandle handle)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);
        assert(handle.isValid());

        return assetLib->assets.getHandleData<Asset>(0, handle)->obj;
    }

    AssetState::Enum asset::getState(AssetHandle handle)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);
        if (handle.isValid())
            return assetLib->assets.getHandleData<Asset>(0, handle)->loadState;
        else
            return AssetState::LoadFailed;
    }

    int asset::getParamSize(const char* name)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);

        int typeIdx = assetLib->assetTypesTable.find(tinystl::hash_string(name, strlen(name)));
        if (typeIdx != -1)
            return assetLib->assetTypes.getHandleData<AssetTypeData>(0,
                                                                      assetLib->assetTypesTable.getValue(typeIdx))->userParamsSize;
        else
            return 0;
    }

    const char* asset::getUri(AssetHandle handle)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);
        assert(handle.isValid());

        return assetLib->assets.getHandleData<Asset>(0, handle)->uri.cstr();
    }

    const char* asset::getName(AssetHandle handle)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);
        assert(handle.isValid());

        int r = assetLib->assetTypesTable.find(assetLib->assets.getHandleData<Asset>(0, handle)->typeNameHash);
        if (r != 0) {
            return assetLib->assetTypes.getHandleData<AssetTypeData>(0, assetLib->assetTypesTable.getValue(r))->name;
        }

        return "";
    }

    const void* asset::getParams(AssetHandle handle)
    {
        AssetLib* assetLib = gAssetLib;
        assert(assetLib);
        assert(handle.isValid());

        return assetLib->assets.getHandleData<Asset>(0, handle)->userParams;
    }

    // Async
    void AssetLib::onOpenError(const char* uri)
    {
        AssetLib* assetLib = gAssetLib;

        bx::Path origUri = getOriginalUri(uri);
        int r = this->asyncLoadsTable.find(tinystl::hash_string(origUri.cstr(), origUri.getLength()));
        if (r != -1) {
            uint16_t handle = this->asyncLoadsTable.getValue(r);
            AsyncLoadRequest* areq = this->asyncLoads.getHandleData<AsyncLoadRequest>(0, handle);
            BX_WARN("Opening asset '%s' failed", uri);

            if (areq->handle.isValid()) {
                setAssetLoadState(areq->handle, AssetState::LoadFailed);

                // Set fail obj to asset
                Asset* res = assetLib->assets.getHandleData<Asset>(0, areq->handle);
                int typeIdx = assetLib->assetTypesTable.find(res->typeNameHash);
                if (typeIdx != -1) {
                    res->obj = assetLib->assetTypes.getHandleData<AssetTypeData>(
                        0, assetLib->assetTypesTable.getValue(typeIdx))->failObj;
                }
            }

            this->asyncLoads.freeHandle(handle);
            this->asyncLoadsTable.remove(r);
        } 
    }

    void AssetLib::onReadError(const char* uri)
    {
        AssetLib* assetLib = gAssetLib;

        bx::Path origUri = getOriginalUri(uri);
        int r = this->asyncLoadsTable.find(tinystl::hash_string(origUri.cstr(), origUri.getLength()));
        if (r != -1) {
            uint16_t handle = this->asyncLoadsTable.getValue(r);
            AsyncLoadRequest* areq = this->asyncLoads.getHandleData<AsyncLoadRequest>(0, handle);
            BX_WARN("Reading asset '%s' failed", uri);

            if (areq->handle.isValid()) {
                setAssetLoadState(areq->handle, AssetState::LoadFailed);

                // Set fail obj to asset
                Asset* res = assetLib->assets.getHandleData<Asset>(0, areq->handle);
                int typeIdx = assetLib->assetTypesTable.find(res->typeNameHash);
                if (typeIdx != -1) {
                    res->obj = assetLib->assetTypes.getHandleData<AssetTypeData>(
                        0, assetLib->assetTypesTable.getValue(typeIdx))->failObj;
                }
            }

            this->asyncLoads.freeHandle(handle);
            this->asyncLoadsTable.remove(r);
        } 
    }

    void AssetLib::onReadComplete(const char* uri, MemoryBlock* mem)
    {
        AssetLib* assetLib = gAssetLib;
        bx::Path origUri = getOriginalUri(uri);
        int r = this->asyncLoadsTable.find(tinystl::hash_string(origUri.cstr(), origUri.getLength()));
        if (r != -1) {
            int handle = this->asyncLoadsTable[r];
            AsyncLoadRequest* areq = this->asyncLoads.getHandleData<AsyncLoadRequest>(0, handle);
            this->asyncLoads.freeHandle(handle);

            assert(areq->handle.isValid());
            AssetHandle aHandle = areq->handle;
            Asset* rs = this->assets.getHandleData<Asset>(0, areq->handle);

            // Load using the callback
            AssetParams params;
            params.uri = uri;
            params.userParams = rs->userParams;
            params.flags = areq->flags;
            uintptr_t obj;
            bool loadResult = rs->callbacks->loadObj(mem, params, &obj, rs->objAlloc);
            releaseMemoryBlock(mem);
            this->asyncLoadsTable.remove(r);

            // Refresh 'rs' pointer, because in 'loadObj' we may load another resource and the 'assets' HandlePool is reallocated, 
            // thus the 'rs' pointer will be mangled
            rs = this->assets.getHandleData<Asset>(0, aHandle);

            if (!loadResult) {
                BX_WARN("Loading asset '%s' failed", uri);
                BX_WARN(err::getString());
                rs->loadState = AssetState::LoadFailed;

                // Set fail obj to asset
                int typeIdx = assetLib->assetTypesTable.find(rs->typeNameHash);
                if (typeIdx != -1) {
                    rs->obj = assetLib->assetTypes.getHandleData<AssetTypeData>(
                        0, assetLib->assetTypesTable.getValue(typeIdx))->failObj;
                }
                return;
            }

            // Update the obj 
            rs->obj = obj;
            rs->loadState = AssetState::LoadOk;

            // Trigger onReload callback
            if (areq->flags & AssetFlags::Reload) {
                rs->callbacks->onReload(rs->handle, rs->objAlloc);
            }
        } else {
            releaseMemoryBlock(mem);
        }
    }

    void AssetLib::onModified(const char* uri)
    {
        // Hot Loading
        if (this->flags & AssetLibInitFlags::HotLoading) {
            bx::Path origUri = getOriginalUri(uri);
            int r = this->asyncLoadsTable.find(tinystl::hash_string(origUri.cstr(), origUri.getLength()));

            // strip "assets/" from the begining of uri
            size_t uriOffset = 0;
            if (strstr(origUri.getBuffer(), "assets/") == origUri.getBuffer())
                uriOffset = strlen("assets/");
            int index = this->hotLoadsTable.find(tinystl::hash_string(origUri.cstr() + uriOffset, strlen(origUri.cstr() + uriOffset)));
            if (index != -1) {
                bx::MultiHashTable<uint16_t>::Node* node = this->hotLoadsTable.getNode(index);
                // Recurse assets and reload them with their params
                while (node) {
                    const Asset* rs = this->assets.getHandleData<Asset>(0, node->value);
                    loadAssetHashed(rs->typeNameHash, rs->uri.cstr(), rs->userParams, AssetFlags::Reload, rs->objAlloc);
                    node = node->next;
                }
            }

            // Run user callback
            if (this->modifiedCallback)
                this->modifiedCallback(origUri.cstr(), this->fileModifiedUserParam);
        }
    }


    void asset::reloadAssets(const char* name)
    {
        AssetLib* assetLib = gAssetLib;

        int typeIdx = assetLib->assetTypesTable.find(tinystl::hash_string(name, strlen(name)));
        if (typeIdx != -1) {
            AssetTypeData* rt = assetLib->assetTypes.getHandleData<AssetTypeData>(0, assetLib->assetTypesTable[typeIdx]);
            size_t hash = tinystl::hash_string(name, strlen(name));

            for (int i = 0, c = assetLib->assets.getCount(); i < c; i++) {
                AssetHandle handle = assetLib->assets.handleAt(i);
                Asset* r = assetLib->assets.getHandleData<Asset>(0, handle);
                if (r->typeNameHash == hash && !(r->uri.isEqual("[FAIL]") | r->uri.isEqual("[ASYNC]"))) {
                    loadAssetHashed(hash, r->uri.cstr(), r->userParams, AssetFlags::Reload, r->objAlloc);
                }
            }
        }
    }

    void asset::unloadAssets(const char* name)
    {
        AssetLib* assetLib = gAssetLib;
        assetLib->ignoreUnloadResourceCalls = true;       // This is for indirect calls to unloadResource

        int typeIdx = assetLib->assetTypesTable.find(tinystl::hash_string(name, strlen(name)));
        if (typeIdx != -1) {
            AssetTypeData* rt = assetLib->assetTypes.getHandleData<AssetTypeData>(0, assetLib->assetTypesTable[typeIdx]);
            size_t hash = tinystl::hash_string(name, strlen(name));

            AssetHandle xhandles[1000];
            int numXHandles = 0;
            for (int i = 0, c = assetLib->assets.getCount(); i < c; i++) {
                AssetHandle handle = assetLib->assets.handleAt(i);
                Asset* r = assetLib->assets.getHandleData<Asset>(0, handle);
                if (r->typeNameHash == hash) {
                    if (r->loadState == AssetState::LoadOk) {
                        rt->callbacks->unloadObj(r->obj, r->objAlloc);
                        r->obj = rt->failObj;
                        r->loadState = AssetState::LoadFailed;
                    } else if (r->uri.isEqual("[FAIL]") | r->uri.isEqual("[ASYNC]")) {
                        xhandles[numXHandles++] = handle;
                    }
                }
            }

            for (int i = 0; i < numXHandles; i++)
                deleteAsset(xhandles[i], rt);
        }

        assetLib->ignoreUnloadResourceCalls = false;
    }

    bool asset::checkAssetsLoaded(const char* name)
    {
        AssetLib* assetLib = gAssetLib;
        int typeIdx = assetLib->assetTypesTable.find(tinystl::hash_string(name, strlen(name)));
        if (typeIdx != -1) {
            AssetTypeData* rt = assetLib->assetTypes.getHandleData<AssetTypeData>(0, assetLib->assetTypesTable[typeIdx]);
            size_t hash = tinystl::hash_string(name, strlen(name));

            for (int i = 0, c = assetLib->assets.getCount(); i < c; i++) {
                AssetHandle handle = assetLib->assets.handleAt(i);
                Asset* r = assetLib->assets.getHandleData<Asset>(0, handle);
                if (r->typeNameHash == hash && !(r->uri.isEqual("[FAIL]") | r->uri.isEqual("[ASYNC]"))) {
                    if (r->loadState != AssetState::LoadOk && r->initLoadState != AssetState::LoadFailed)
                        return false;
                }
            }
        }

        return true;
    }

    void asset::replaceFileExtension(const char* ext, const char* extReplacement)
    {
        assert(ext);
        AssetLib* assetLib = gAssetLib;
        int index = assetLib->overrides.find([ext](const AssetExtensionOverride& value)->bool {
            return strcmp(value.ext, ext) == 0;
        });

        if (index != -1) {
            if (extReplacement) {
                assetLib->overrides[index].replacement = extReplacement;
            } else {
                std::swap<AssetExtensionOverride>(assetLib->overrides[index], assetLib->overrides[assetLib->overrides.getCount()-1]);
                assetLib->overrides.pop();
            }
        } else {
            AssetExtensionOverride* teo = assetLib->overrides.push();
            teo->ext = ext;
            teo->replacement = extReplacement;
        }
    }
} // namespace tee