#include "termite/core.h"
#include "termite/io_driver.h"

#define T_CORE_API
#include "termite/plugin_api.h"

#include "bx/platform.h"
#include "bx/crtimpl.h"
#include "bx/cpu.h"
#include "bxx/path.h"
#include "bxx/logger.h"
#include "bxx/pool.h"
#include "bxx/queue.h"
#include "bx/thread.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include <mutex>
#include <condition_variable>

#define MAX_FILE_SIZE 1073741824    // 1GB

using namespace termite;

static CoreApi_v0* g_core = nullptr;

struct BlockingAssetDriver
{
    bx::AllocatorI* alloc;
    AAssetManager* mgr;

    BlockingAssetDriver()
    {
        alloc = nullptr;
        mgr = nullptr;
    }
};

struct AsyncRequest
{
    bx::Path uri;
};

struct AsyncResponse
{
    enum Type
    {
        RequestOpenFailed,
        RequestReadFailed,
        RequestOk
    };

    Type type;
    bx::Path uri;
    MemoryBlock* mem;
};

struct AsyncAssetDriver
{
    bx::AllocatorI* alloc;
    AAssetManager* mgr;
    IoDriverEventsI* callbacks;
    bx::Thread loadThread;

    bx::Pool<bx::SpScUnboundedQueuePool<AsyncRequest>::Node> requestPool;
    bx::SpScUnboundedQueuePool<AsyncRequest>* requestQueue;

    bx::Pool<bx::SpScUnboundedQueuePool<AsyncResponse>::Node> responsePool;
    bx::SpScUnboundedQueuePool<AsyncResponse>* responseQueue;

    volatile int32_t stop;

    // Used to wake-up request thread
    int32_t numRequests;
    std::mutex reqMutex;
    std::condition_variable reqCv;

    AsyncAssetDriver()
    {
        alloc = nullptr;
        mgr = nullptr;
        callbacks = nullptr;
        requestQueue = nullptr;
        responseQueue = nullptr;
        stop = 0;
        numRequests = 0;
    }
};

static BlockingAssetDriver g_blocking;
static AsyncAssetDriver g_async;
static AAssetManager* g_amgr = nullptr;

// JNI
extern "C" JNIEXPORT void JNICALL Java_com_termite_utils_PlatformUtils_termiteInitAssetManager(JNIEnv* env, jclass cls, jobject jassetManager)
{
    BX_UNUSED(cls);
    g_amgr = AAssetManager_fromJava(env, jassetManager);
}

// BlockingIO
static int blockInit(bx::AllocatorI* alloc, const char* uri, const void* params, IoDriverEventsI* callbacks)
{
    g_blocking.alloc = alloc;
    g_blocking.mgr = g_amgr;
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

static MemoryBlock* blockRead(const char* uri)
{
    AAsset* asset = AAssetManager_open(g_blocking.mgr, uri, AASSET_MODE_UNKNOWN);
    if (!asset) {
        T_ERROR_API(g_core, "Unable to open file '%s' for reading", uri);
        return nullptr;
    }

    MemoryBlock* mem = nullptr;
    off_t size = AAsset_getLength(asset);
    if (size) {
        mem = g_core->createMemoryBlock(size, g_blocking.alloc);
        if (mem) {
            AAsset_read(asset, mem->data, size);
        }
    }
    AAsset_close(asset);    

    if (!mem) {
        T_ERROR_API(g_core, "Unable to read file '%s'", uri);
    }
    return mem;
}

static size_t blockWrite(const char* uri, const MemoryBlock* mem)
{
    BX_FATAL("Access denied writing data to AssetManager");
    assert(false);
    return 0;
}

static void blockRunAsyncLoop()
{
}

static IoOperationMode blockGetOpMode()
{
    return IoOperationMode::Blocking;
}

static const char* blockGetUri()
{
    return "";
}

// AsyncIO
static int32_t asyncLoadThread(void* userData)
{
    AsyncAssetDriver* driver = (AsyncAssetDriver*)userData;
    AsyncRequest request;
    AsyncResponse response;

    while (!driver->stop) {
        while (driver->requestQueue->pop(&request)) {
            {
                std::lock_guard<std::mutex> lk(driver->reqMutex);
                driver->numRequests--;
            }

            AAsset* asset = AAssetManager_open(driver->mgr, request.uri.cstr(), AASSET_MODE_UNKNOWN);
            if (!asset) {
                response.type = AsyncResponse::RequestOpenFailed;
                response.uri = request.uri;
                driver->responseQueue->push(response);
                continue;
            }

            MemoryBlock* mem = nullptr;
            off_t size = AAsset_getLength(asset);
            if (size) {
                mem = g_core->createMemoryBlock(size, driver->alloc);
                if (mem) {
                    AAsset_read(asset, mem->data, size);
                }
            }
            AAsset_close(asset);

            if (mem) {
                response.type = AsyncResponse::RequestOk;
                response.uri = request.uri;
                response.mem = mem;
                driver->responseQueue->push(response);
            } else {
                response.type = AsyncResponse::RequestReadFailed;
                response.uri = request.uri;
                driver->responseQueue->push(response);
            }
        }   // dequeue all requests and process them

        // Wait for incoming requests
        {
            std::unique_lock<std::mutex> lk(driver->reqMutex);
            driver->reqCv.wait(lk, [&driver] { return driver->numRequests > 0; });
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
    if (!g_async.requestPool.create(32, alloc))
        return T_ERR_OUTOFMEM;
    g_async.requestQueue = BX_NEW(alloc, bx::SpScUnboundedQueuePool<AsyncRequest>)(&g_async.requestPool);

    if (!g_async.responsePool.create(32, alloc))
        return T_ERR_OUTOFMEM;
    g_async.responseQueue = BX_NEW(alloc, bx::SpScUnboundedQueuePool<AsyncResponse>)(&g_async.responsePool);

    // Start the load thread
    g_async.loadThread.init(asyncLoadThread, &g_async, 128*1024, "AsyncLoadThread");
    return 0;
}

static void asyncShutdown()
{
    if (!g_async.alloc)
        return;

    bx::AllocatorI* alloc = g_async.alloc;
    bx::atomicExchange<int32_t>(&g_async.stop, 1);
    g_async.loadThread.shutdown();
    
    BX_DELETE(alloc, g_async.responseQueue);
    g_async.responsePool.destroy();

    BX_DELETE(alloc, g_async.requestQueue);
    g_async.requestPool.destroy();
}

static void asyncSetCallbacks(IoDriverEventsI* callbacks)
{  
    assert(callbacks);
    g_async.callbacks = callbacks;
}

static IoDriverEventsI* asyncGetCallbacks()
{
    return g_async.callbacks;
}

static MemoryBlock* asyncRead(const char* uri)
{
    AsyncRequest request;
    request.uri = uri;

    g_async.reqMutex.lock();
    g_async.numRequests++;
    g_async.reqMutex.unlock();

    g_async.requestQueue->push(request);
    g_async.reqCv.notify_one();

    return nullptr;
}

static size_t asyncWrite(const char* uri, const MemoryBlock* mem)
{
    BX_FATAL("Access denied writing data to AssetManager");
    assert(false);
    return 0;
}

static void asyncRunAsyncLoop()
{
    if (!g_async.callbacks)
        return;

    AsyncResponse response;
    while (g_async.responseQueue->pop(&response)) {
        switch (response.type) {
        case AsyncResponse::RequestOk:
            g_async.callbacks->onReadComplete(response.uri.cstr(), response.mem);
            break;
        case AsyncResponse::RequestOpenFailed:
            g_async.callbacks->onOpenError(response.uri.cstr());
            break;
        case AsyncResponse::RequestReadFailed:
            g_async.callbacks->onReadError(response.uri.cstr());
            break;
        }
    }
}

static IoOperationMode asyncGetOpMode()
{
    return IoOperationMode::Async;
}

static const char* asyncGetUri()
{
    return "";
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

