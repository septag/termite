#include "bxx/path.h"
#include "bxx/pool.h"
#include "bx/platform.h"
#include "bx/file.h"

#include "termite/core.h"
#include "termite/io_driver.h"

#define T_CORE_API
#include "termite/plugin_api.h"

#include "uv.h"
#include <fcntl.h>
#include <ctime>

#define MAX_FILE_SIZE 1073741824    // 1GB

using namespace termite;

static CoreApi_v0* g_core = nullptr;

struct DiskFileRequest
{
    bx::Path uri;
    uv_fs_t openReq;
    uv_fs_t rwReq;
    uv_fs_t statReq;
    uv_buf_t buff;
    MemoryBlock* mem;

    DiskFileRequest()
    {
        mem = nullptr;
        bx::memSet(&buff, 0x00, sizeof(buff));

        uv_fs_req_cleanup(&openReq);
        uv_fs_req_cleanup(&rwReq);
        uv_fs_req_cleanup(&statReq);
    }

    ~DiskFileRequest()
    {
        if (mem)
            g_core->releaseMemoryBlock(mem);
        uv_fs_req_cleanup(&openReq);
        uv_fs_req_cleanup(&rwReq);
        uv_fs_req_cleanup(&statReq);
    }
};

struct AsyncDiskDriver
{
    IoDriverEventsI* callbacks;
    bx::Path rootDir;
    bx::AllocatorI* alloc;
    uv_loop_t loop;
    bx::Pool<DiskFileRequest> fsReqPool;
    uv_fs_event_t dirChange;

    AsyncDiskDriver()
    {
        callbacks = nullptr;
        alloc = nullptr;
        bx::memSet(&dirChange, 0x00, sizeof(dirChange));
        bx::memSet(&loop, 0x00, sizeof(loop));
    }
};

static AsyncDiskDriver g_async;

static bx::Path resolvePath(const char* uri, const bx::Path& rootDir, IoPathType::Enum pathType)
{
    bx::Path filepath;
    switch (pathType) {
    case IoPathType::Assets:
        filepath = rootDir;
        filepath.join("assets").join(uri);
        break;
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

static void uvCallbackFsEvent(uv_fs_event_t* handle, const char* filename, int events, int status)
{
    static bx::Path lastModFile;
    static double lastModTime = 0.0;

    if (!filename)
        return;

    if (g_async.callbacks) {
        bx::Path filepath(g_async.rootDir);
        filepath.join(filename);

        if (filepath.getType() == bx::PathType::File) {
#ifdef BX_PLATFORM_WINDOWS
            // Transform '\\' to '/' on windows to match the original references
            bx::Path filenamep(filename);
            filenamep.toUnix();
            filename = filenamep.cstr();
#endif
            // Ignore file changes less than 0.1 second
            double curTime = g_core->getElapsedTime();
            if (lastModFile.isEqual(filename) && (curTime - lastModTime) < 0.1)
                return;            
            
            g_async.callbacks->onModified(filename);
            lastModFile = filename;
            lastModTime = curTime;
        }
    }
}

static result_t asyncInit(bx::AllocatorI* alloc, const char* uri, const void* params, IoDriverEventsI* callbacks)
{
    g_async.alloc = alloc;

    g_async.rootDir = uri;
    g_async.rootDir.normalizeSelf();

    uv_loop_init(&g_async.loop);
    uv_fs_event_init(&g_async.loop, &g_async.dirChange);

#if !BX_PLATFORM_ANDROID
    if (g_async.rootDir.getType() != bx::PathType::Directory) {
        T_ERROR_API(g_core, "Root Directory '%s' does not exist", g_async.rootDir.cstr());
        return T_ERR_FAILED;
    }
#endif

    g_async.callbacks = callbacks;

    if (!g_async.fsReqPool.create(32, alloc)) {
        return T_ERR_OUTOFMEM;
    }

#if termite_DEV && !BX_PLATFORM_ANDROID
    // Monitor root dir
    // Note: Recursive flag does not work under linux, according to documentation
    if (uv_fs_event_start(&g_async.dirChange, uvCallbackFsEvent, g_async.rootDir.cstr(), UV_FS_EVENT_RECURSIVE)) {
        T_ERROR_API(g_core, "Could not monitor root directory '%s' for changes", g_async.rootDir.cstr());
        return T_ERR_FAILED;
    }
#endif

    return 0;
}

static void uvWalk(uv_handle_t* handle, void* arg)
{
    uv_close(handle, nullptr);
}

static void asyncShutdown()
{
#if termite_DEV && !BX_PLATFORM_ANDROID
    uv_fs_event_stop(&g_async.dirChange);
#endif

    // Walk the event loop handles and close all
    uv_walk(&g_async.loop, uvWalk, nullptr);
    uv_run(&g_async.loop, UV_RUN_DEFAULT);
    uv_loop_close(&g_async.loop);

    g_async.fsReqPool.destroy();
}

static void asyncSetCallbacks(IoDriverEventsI* callbacks)
{
    g_async.callbacks = callbacks;
}

static IoDriverEventsI* asyncGetCallbacks()
{
    return g_async.callbacks;
}

static void uvCloseFile(uv_fs_t* req)
{
    DiskFileRequest* rr = (DiskFileRequest*)req->data;

    uv_fs_t closeReq;
    uv_fs_close(&g_async.loop, &closeReq, (uv_file)rr->openReq.result, nullptr);   // Synchronous
    g_async.fsReqPool.deleteInstance(rr);
}

// Read the file in blocks and put them into 
static void uvCallbackRead(uv_fs_t* req)
{
    DiskFileRequest* rr = (DiskFileRequest*)req->data;
    if (req->result < 0) {
        if (g_async.callbacks)
            g_async.callbacks->onReadError(rr->uri.cstr());
    } else if (req->result > 0) {
        if (g_async.callbacks)
            g_async.callbacks->onReadComplete(rr->uri.cstr(), g_core->refMemoryBlock(rr->mem));
    } else {
        assert(false);
    }
    uvCloseFile(req);
}

// Get stats of the file (file size), allocate memory for it and start reading the file
static void uvCallbackStat(uv_fs_t* req)
{
    DiskFileRequest* rr = (DiskFileRequest*)req->data;
    if (req->result >= 0) {
        if (req->statbuf.st_size > 0) {
            rr->mem = g_core->createMemoryBlock((uint32_t)req->statbuf.st_size, g_async.alloc);
            if (!rr->mem) {
                if (g_async.callbacks)
                    g_async.callbacks->onOpenError(rr->uri.cstr());
            }
            rr->buff = uv_buf_init((char*)rr->mem->data, rr->mem->size);
            rr->rwReq.data = rr;
            uv_fs_read(&g_async.loop, &rr->rwReq, (uv_file)rr->openReq.result, &rr->buff, 1, -1, uvCallbackRead);
        } else {
            uvCloseFile(req);
        }
    } else {
        if (g_async.callbacks)
            g_async.callbacks->onOpenError(rr->uri.cstr());
        uvCloseFile(req);
    }
}

static void uvCallbackOpenForRead(uv_fs_t* req)
{
    DiskFileRequest* rr = (DiskFileRequest*)req->data;
    if (req->result >= 0) {
        rr->statReq.data = rr;
        uv_fs_fstat(&g_async.loop, &rr->statReq, (uv_file)rr->openReq.result, uvCallbackStat);
    } else {
        if (g_async.callbacks)
            g_async.callbacks->onOpenError(rr->uri.cstr());
        g_async.fsReqPool.deleteInstance(rr);
    }
}

static MemoryBlock* asyncRead(const char* uri, IoPathType::Enum pathType)
{
    bx::Path filepath = resolvePath(uri, g_async.rootDir, pathType);
    DiskFileRequest* req = g_async.fsReqPool.newInstance<>();

    if (!req) {
        if (g_async.callbacks)
            g_async.callbacks->onOpenError(filepath.cstr());
        return nullptr;
    }

    req->uri = uri;
    req->openReq.data = req;
        
    // Start Open for read
    if (uv_fs_open(&g_async.loop, &req->openReq, filepath.cstr(), 0, O_RDONLY, uvCallbackOpenForRead)) {
        if (g_async.callbacks)
            g_async.callbacks->onOpenError(uri);
        return nullptr;
    }

    return nullptr;
}

static void asyncRunAsyncLoop()
{
    uv_run(&g_async.loop, UV_RUN_NOWAIT);
}

static void uvCallbackWrite(uv_fs_t* req)
{
    DiskFileRequest* rr = (DiskFileRequest*)req->data;
    if (req->result >= 0) {
        if (g_async.callbacks)
            g_async.callbacks->onWriteComplete(rr->uri.cstr(), (size_t)req->result);
    } else {
        if (g_async.callbacks)
            g_async.callbacks->onWriteError(rr->uri.cstr());
    }

    uvCloseFile(req);
}

static void uvCallbackOpenForWrite(uv_fs_t* req)
{
    DiskFileRequest* rr = (DiskFileRequest*)req->data;
    if (req->result >= 0) {
        rr->rwReq.data = rr;
        rr->buff = uv_buf_init((char*)rr->mem->data, rr->mem->size);
        uv_fs_write(&g_async.loop, &rr->rwReq, (uv_file)rr->openReq.result, &rr->buff, 1, -1, uvCallbackWrite);
    } else {
        if (g_async.callbacks)
            g_async.callbacks->onOpenError(rr->uri.cstr());
        g_async.fsReqPool.deleteInstance(rr);
    }
}

static size_t asyncWrite(const char* uri, const MemoryBlock* mem, IoPathType::Enum pathType)
{
    bx::Path filepath = resolvePath(uri, g_async.rootDir, pathType);
    DiskFileRequest* req = g_async.fsReqPool.newInstance<>();

    if (!req) {
        if (g_async.callbacks)
            g_async.callbacks->onOpenError(filepath.cstr());
        return 0;
    }

    req->uri = uri;
    req->openReq.data = req;
    req->mem = g_core->refMemoryBlock(const_cast<MemoryBlock*>(mem));

    // Start Open for write
    if (uv_fs_open(&g_async.loop, &req->openReq, filepath.cstr(), 0, O_CREAT | O_WRONLY | O_TRUNC, uvCallbackOpenForWrite)) {
        if (g_async.callbacks)
            g_async.callbacks->onOpenError(uri);
        return 0;
    }

    return 0;
}

static IoOperationMode::Enum asyncGetOpMode()
{
    return IoOperationMode::Async;
}

static const char* asyncGetUri()
{
    return g_async.rootDir.cstr();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct BlockingDiskDriver
{
    bx::Path rootDir;
    bx::AllocatorI* alloc;

    BlockingDiskDriver()
    {
        alloc = nullptr;
    }
};
static BlockingDiskDriver g_blocking;

static int blockInit(bx::AllocatorI* alloc, const char* uri, const void* params, IoDriverEventsI* callbacks)
{
    g_blocking.alloc = alloc;

    g_blocking.rootDir = uri;
    g_blocking.rootDir.normalizeSelf();

#if !BX_PLATFORM_ANDROID
    if (g_blocking.rootDir.getType() != bx::PathType::Directory) {
        T_ERROR_API(g_core, "Root Directory '%s' does not exist", g_blocking.rootDir.cstr());
        return T_ERR_FAILED;
    }
#endif

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

static MemoryBlock* blockRead(const char* uri, IoPathType::Enum pathType)
{
    bx::Path filepath = resolvePath(uri, g_blocking.rootDir, pathType);

    bx::FileReader file;
    bx::Error err;
    if (!file.open(filepath.cstr(), &err)) {
        T_ERROR_API(g_core, "Unable to open file '%s' for reading", uri);
        return nullptr;
    }

    // Get file size
    int64_t size = file.seek(0, bx::Whence::End);
    file.seek(0, bx::Whence::Begin);

    // Read Contents
    MemoryBlock* mem = nullptr;
    if (size) {
        mem = g_core->createMemoryBlock((uint32_t)size, g_blocking.alloc);
        if (mem)
            file.read(mem->data, mem->size, &err);
    }

    file.close();
    return mem;
}

static size_t blockWrite(const char* uri, const MemoryBlock* mem, IoPathType::Enum pathType)
{
    bx::Path filepath = resolvePath(uri, g_blocking.rootDir, pathType);

    bx::FileWriter file;
    bx::Error err;
    if (!file.open(filepath.cstr(), false, &err)) {
        T_ERROR_API(g_core, "Unable to open file '%s' for writing", filepath.cstr());
        return 0;
    }

    // Write memory
    size_t size = file.write(mem->data, mem->size, &err);
    file.close();
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

PluginDesc* getDiskDriverDesc()
{
    static PluginDesc desc;
    strcpy(desc.name, "DiskIO");
    strcpy(desc.description, "DiskIO Driver (Blocking and Async)");
    desc.type = PluginType::IoDriver;
    desc.version = T_MAKE_VERSION(1, 0);
    return &desc;
}

void* initDiskDriver(bx::AllocatorI* alloc, GetApiFunc getApi)
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

    bx::memSet(&asyncApi, 0x00, sizeof(asyncApi));
    bx::memSet(&blockApi, 0x00, sizeof(blockApi));

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

void shutdownDiskDriver()
{
}

#ifdef termite_SHARED_LIB
T_PLUGIN_EXPORT void* termiteGetPluginApi(uint16_t apiId, uint32_t version)
{
    static PluginApi_v0 v0;

    if (T_VERSION_MAJOR(version) == 0) {
        v0.init = initDiskDriver;
        v0.shutdown = shutdownDiskDriver;
        v0.getDesc = getDiskDriverDesc;
        return &v0;
    } else {
        return nullptr;
    }
}
#endif