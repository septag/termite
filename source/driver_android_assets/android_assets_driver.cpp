#include "termite/core.h"
#include "termite/io_driver.h"

#define T_CORE_API
#include "termite/plugin_api.h"

#include "bx/platform.h"
#include "bx/crtimpl.h"
#include "bx/cpu.h"
#include "bxx/path.h"
#include "bxx/logger.h"
#include "bxx/queue.h"
#include "bx/thread.h"
#include "bx/mutex.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include <mutex>
#include <condition_variable>

using namespace termite;

static CoreApi_v0* g_core = nullptr;

struct BlockingAssetDriver
{
    bx::AllocatorI* alloc;
    AAssetManager* mgr;
    bx::Path rootDir;

    BlockingAssetDriver()
    {
        alloc = nullptr;
        mgr = nullptr;
    }
};

struct AsyncRequest
{
    enum Type
    {
        Read,
        Write
    };

    Type type;
    bx::Path uri;
    MemoryBlock* mem;
    IoPathType::Enum pathType;
    bx::Queue<AsyncRequest*>::Node qnode;

    AsyncRequest() : 
        mem(nullptr),
        qnode(this)
    {
    }
};

struct AsyncResponse
{
    enum Type
    {
        RequestOpenFailed,
        RequestReadFailed,
        RequestReadOk,
        RequestWriteFailed,
        RequestWriteOk
    };

    Type type;
    bx::Path uri;
    MemoryBlock* mem;
    size_t bytesWritten;
    bx::Queue<AsyncResponse*>::Node qnode;

    AsyncResponse() :
        mem(nullptr),
        bytesWritten(0),
        qnode(this)
    {
    }
};

struct AsyncAssetDriver
{
    bx::AllocatorI* alloc;
    AAssetManager* mgr;
    IoDriverEventsI* callbacks;
    bx::Thread loadThread;
    bx::Path rootDir;

    bx::Mutex reqQueueMtx;
    bx::Mutex resQueueMtx;
    bx::Queue<AsyncRequest*> reqQueue;
    bx::Queue<AsyncResponse*> resQueue;    

    volatile int32_t stop;

    bx::Semaphore reqSem;
    bx::Pool<AsyncRequest> reqPool;
    bx::Pool<AsyncResponse> resPool;

    AsyncAssetDriver()
    {
        alloc = nullptr;
        mgr = nullptr;
        callbacks = nullptr;
        stop = 0;
    }
};

static BlockingAssetDriver g_blocking;
static AsyncAssetDriver g_async;
static AAssetManager* g_amgr = nullptr;

// JNI
extern "C" JNIEXPORT void JNICALL Java_com_termite_util_Platform_termiteInitAssetManager(JNIEnv* env, jclass cls, jobject jassetManager)
{
    BX_UNUSED(cls);
    g_amgr = AAssetManager_fromJava(env, jassetManager);
}

static bx::Path resolvePath(const char* uri, const bx::Path& rootDir, IoPathType::Enum pathType)
{
    assert(pathType != IoPathType::Assets);

    bx::Path filepath;
    switch (pathType) {
    case IoPathType::Relative:
        filepath = rootDir;
        filepath.join(uri);
        break;
    case IoPathType::Absolute:
        filepath = uri;
        break;
    }
    return filepath;
}

// BlockingIO
static int blockInit(bx::AllocatorI* alloc, const char* uri, const void* params, IoDriverEventsI* callbacks)
{
    g_blocking.alloc = alloc;
    g_blocking.mgr = g_amgr;
    g_blocking.rootDir = uri;
    g_blocking.rootDir.normalizeSelf();

    if (!g_blocking.mgr) {
        T_ERROR_API(g_core, "JNI AssetManager is not initialized. Call "
                    "com.termite.utils.PlatformUtils.termiteInitAssetManager before initializing the engine");
        return T_ERR_FAILED;
    }

    return 0;
}

static void blockShutdown()
{
}

static void blockSetCallbacks(IoDriverEventsI* callbacks)
{
}

static IoDriverEventsI* blockGetCallbacks()
{
    return nullptr;
}

static MemoryBlock* blockReadRaw(const char* uri, IoPathType::Enum pathType, AsyncResponse::Type* pRes)
{
    MemoryBlock* mem = nullptr;
    if (pathType == IoPathType::Assets) {
        AAsset* asset = AAssetManager_open(g_blocking.mgr, uri, AASSET_MODE_UNKNOWN);
        if (!asset) {
            *pRes = AsyncResponse::RequestOpenFailed;
            return nullptr;
        }

        off_t size = AAsset_getLength(asset);
        if (size) {
            mem = g_core->createMemoryBlock(size, g_blocking.alloc);
            if (mem) {
                AAsset_read(asset, mem->data, size);
                *pRes = AsyncResponse::RequestReadOk;
            } else {
                *pRes = AsyncResponse::RequestReadFailed;
            }
        }
        AAsset_close(asset);
    } else {
        bx::Path filepath = resolvePath(uri, g_blocking.rootDir, pathType);

        bx::CrtFileReader file;
        bx::Error err;
        if (!file.open(filepath.cstr(), &err)) {
            *pRes = AsyncResponse::RequestOpenFailed;
            return nullptr;
        }

        // Get file size
        int64_t size = file.seek(0, bx::Whence::End);
        file.seek(0, bx::Whence::Begin);

        // Read Contents
        if (size) {
            mem = g_core->createMemoryBlock((uint32_t)size, g_blocking.alloc);
            if (mem)
                file.read(mem->data, mem->size, &err);
        }

        file.close();
    }

    *pRes = mem ? AsyncResponse::RequestReadOk : AsyncResponse::RequestReadFailed;

    return mem;
}

static size_t blockWriteRaw(const char* uri, const MemoryBlock* mem, IoPathType::Enum pathType, AsyncResponse::Type* pRes)
{
    size_t size = 0;
    if (pathType != IoPathType::Assets) {
        bx::Path filepath = resolvePath(uri, g_blocking.rootDir, pathType);

        bx::CrtFileWriter file;
        bx::Error err;
        if (!file.open(filepath.cstr(), false, &err)) {
            *pRes = AsyncResponse::RequestOpenFailed;
            return 0;
        }

        // Write memory
        size = file.write(mem->data, mem->size, &err);
        file.close();
    }

    *pRes = size != 0 ? AsyncResponse::RequestWriteOk : AsyncResponse::RequestWriteFailed;
    return size;
}

static MemoryBlock* blockRead(const char* uri, IoPathType::Enum pathType)
{
    AsyncResponse::Type res;
    MemoryBlock* mem = blockReadRaw(uri, pathType, &res);
    switch (res) {
    case AsyncResponse::RequestReadOk:
        break;
    case AsyncResponse::RequestOpenFailed:
        T_ERROR_API(g_core, "Unable to open file '%s' for reading", uri);
        break;
    case AsyncResponse::RequestReadFailed:
        T_ERROR_API(g_core, "Unable read file '%s'", uri);
        break;
    }

    return mem;
}

static size_t blockWrite(const char* uri, const MemoryBlock* mem, IoPathType::Enum pathType)
{
    AsyncResponse::Type res;
    size_t size = blockWriteRaw(uri, mem, pathType, &res);

    switch (res) {
    case AsyncResponse::RequestWriteOk:
        break;
    case AsyncResponse::RequestOpenFailed:
        T_ERROR_API(g_core, "Unable to open file '%s' for writing", uri);
        break;
    case AsyncResponse::RequestWriteFailed:
        T_ERROR_API(g_core, "Unable write file '%s'", uri);
        break;
    }

    return size;
}

static void blockRunAsyncLoop()
{
}

static IoOperationMode::Enum blockGetOpMode()
{
    return IoOperationMode::Blocking;
}

static const char* blockGetUri()
{
    return g_blocking.rootDir.cstr();
}

// AsyncIO
static int32_t asyncThread(void* userData)
{
    AsyncAssetDriver* driver = (AsyncAssetDriver*)userData;

    while (!driver->stop) {
        // Wait for incoming requests
        driver->reqSem.wait();

        AsyncRequest req;
        // Pull one request and execute it
        {
            bx::MutexScope mtx(driver->reqQueueMtx);
            AsyncRequest* pReq;
            if (!driver->reqQueue.pop(&pReq))
                continue;
            memcpy(&req, pReq, sizeof(AsyncRequest));
            driver->reqPool.deleteInstance(pReq);
        }        

        AsyncResponse::Type resType;
        if (req.type == AsyncRequest::Read) {
            MemoryBlock* mem = blockReadRaw(req.uri.cstr(), req.pathType, &resType);

            bx::MutexScope mtx(driver->resQueueMtx);
            AsyncResponse* res = driver->resPool.newInstance<>();
            res->type = resType;
            res->uri = req.uri;
            res->mem = mem;
            driver->resQueue.push(&res->qnode);
        } else if (req.type == AsyncRequest::Write) {
            size_t size = blockWriteRaw(req.uri.cstr(), req.mem, req.pathType, &resType);

            bx::MutexScope mtx(driver->resQueueMtx);
            AsyncResponse* res = driver->resPool.newInstance<>();
            res->type = resType;
            res->bytesWritten = size;
            driver->resQueue.push(&res->qnode);
        }
    }

    return 0;
}

static int asyncInit(bx::AllocatorI* alloc, const char* uri, const void* params, IoDriverEventsI* callbacks)
{
    assert(!g_async.alloc);

    g_async.alloc = alloc;
    g_async.callbacks = callbacks;
    g_async.mgr = g_amgr;

    if (!g_async.mgr) {
        T_ERROR_API(g_core, "JNI AssetManager is not initialized. Call "
                    "com.termite.utils.PlatformUtils.termiteInitAssetManager before initializing the engine");
        return T_ERR_FAILED;
    }

    // Initialize pools and their queues
    if (!g_async.reqPool.create(32, alloc))
        return T_ERR_OUTOFMEM;

    if (!g_async.resPool.create(32, alloc))
        return T_ERR_OUTOFMEM;

    // Start the load thread
    g_async.loadThread.init(asyncThread, &g_async, 128*1024, "AsyncLoadThread");
    return 0;
}

static void asyncShutdown()
{
    if (!g_async.alloc)
        return;

    bx::AllocatorI* alloc = g_async.alloc;
    bx::atomicExchange<int32_t>(&g_async.stop, 1);
    g_async.reqSem.post(100);
    g_async.loadThread.shutdown();
    
    g_async.resPool.destroy();
    g_async.reqPool.destroy();
}

static void asyncSetCallbacks(IoDriverEventsI* callbacks)
{  
    g_async.callbacks = callbacks;
}

static IoDriverEventsI* asyncGetCallbacks()
{
    return g_async.callbacks;
}

static MemoryBlock* asyncRead(const char* uri, IoPathType::Enum pathType)
{
    bx::MutexScope mtx(g_async.reqQueueMtx);

    AsyncRequest* req = g_async.reqPool.newInstance<>();
    req->type = AsyncRequest::Read;
    req->uri = uri;
    req->pathType = pathType;

    g_async.reqQueue.push(&req->qnode);
    g_async.reqSem.post();

    return nullptr;
}

static size_t asyncWrite(const char* uri, const MemoryBlock* mem, IoPathType::Enum pathType)
{
    bx::MutexScope mtx(g_async.reqQueueMtx);

    AsyncRequest* req = g_async.reqPool.newInstance<>();
    req->type = AsyncRequest::Write;
    req->uri = uri;
    req->pathType = pathType;
    req->mem = g_core->refMemoryBlock(const_cast<MemoryBlock*>(mem));

    g_async.reqQueue.push(&req->qnode);
    g_async.reqSem.post();

    return 0;
}

static void asyncRunAsyncLoop()
{
    if (!g_async.callbacks)
        return;

    AsyncResponse* res;
    g_async.resQueueMtx.lock();
    while (g_async.resQueue.pop(&res)) {
        g_async.resQueueMtx.unlock();

        switch (res->type) {
        case AsyncResponse::RequestReadOk:
            g_async.callbacks->onReadComplete(res->uri.cstr(), res->mem);
            break;
        case AsyncResponse::RequestOpenFailed:
            g_async.callbacks->onOpenError(res->uri.cstr());
            break;
        case AsyncResponse::RequestReadFailed:
            g_async.callbacks->onReadError(res->uri.cstr());
            break;
        case AsyncResponse::RequestWriteOk:
            g_async.callbacks->onWriteComplete(res->uri.cstr(), res->bytesWritten);
            break;
        case AsyncResponse::RequestWriteFailed:
            g_async.callbacks->onWriteError(res->uri.cstr());
            break;
        }

        g_async.resQueueMtx.lock();
        g_async.resPool.deleteInstance(res);
    }
    g_async.resQueueMtx.unlock();
}

static IoOperationMode::Enum asyncGetOpMode()
{
    return IoOperationMode::Async;
}

static const char* asyncGetUri()
{
    return g_async.rootDir.cstr();
}

//
PluginDesc* getAndroidAssetDriverDesc()
{
    static PluginDesc desc;
    strcpy(desc.name, "AssetIO");
    strcpy(desc.description, "AssetIO Android Driver (Blocking and Async)");
    desc.type = PluginType::IoDriver;
    desc.version = T_MAKE_VERSION(1, 0);
    return &desc;
}

void* initAndroidAssetDriver(bx::AllocatorI* alloc, GetApiFunc getApi)
{
    g_core = (CoreApi_v0*)getApi(uint16_t(ApiId::Core), 0);
    if (!g_core)
        return nullptr;
    
    static IoDriverApi asyncApi;
    static IoDriverApi blockApi;
    static IoDriverDual driver = {
        &blockApi,
        &asyncApi
    };

    memset(&asyncApi, 0x00, sizeof(asyncApi));
    memset(&blockApi, 0x00, sizeof(blockApi));

    asyncApi.init = asyncInit;
    asyncApi.shutdown = asyncShutdown;
    asyncApi.setCallbacks = asyncSetCallbacks;
    asyncApi.getCallbacks = asyncGetCallbacks;
    asyncApi.read = asyncRead;
    asyncApi.write = asyncWrite;
    asyncApi.runAsyncLoop = asyncRunAsyncLoop;
    asyncApi.getOpMode = asyncGetOpMode;
    asyncApi.getUri = asyncGetUri;

    blockApi.init = blockInit;
    blockApi.shutdown = blockShutdown;
    blockApi.setCallbacks = blockSetCallbacks;
    blockApi.getCallbacks = blockGetCallbacks;
    blockApi.read = blockRead;
    blockApi.write = blockWrite;
    blockApi.runAsyncLoop = blockRunAsyncLoop;
    blockApi.getOpMode = blockGetOpMode;
    blockApi.getUri = blockGetUri;

    return &driver;
}

void shutdownAndroidAssetDriver()
{
}

#ifdef termite_SHARED_LIB
T_PLUGIN_EXPORT void* termiteGetPluginApi(uint16_t apiId, uint32_t version)
{
    static PluginApi_v0 v0;

    if (T_VERSION_MAJOR(version) == 0) {
        v0.init = initAndroidAssetDriver;
        v0.shutdown = shutdownAndroidAssetDriver;
        v0.getDesc = getAndroidAssetDriverDesc;
        return &v0;
    } else {
        return nullptr;
    }
}
#endif

