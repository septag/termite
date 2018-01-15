#include "termite/tee.h"
#include "termite/io_driver.h"

#include "bx/platform.h"
#include "bx/cpu.h"
#include "bx/file.h"
#include "bxx/path.h"
#include "bxx/pool.h"
#include "bxx/linked_list.h"
#include "bxx/queue.h"
#include "bx/mutex.h"

#define TEE_CORE_API
#include "termite/plugin_api.h"

#if USE_EFSW
#include "efsw/efsw.hpp"
#endif

#include "lz4/lz4.h"

#define MAX_DISK_JOBS 4

using namespace tee;

#if USE_EFSW
class FileWatchListener : public efsw::FileWatchListener
{
public:
    void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename,
                          efsw::Action action,
                          std::string oldFilename = "") override;

    void setRootDir(const bx::Path& rootDir);
private:
    bx::Path m_rootDir;
};

struct EfswResult
{
    efsw::Action action;
    bx::Path filepath;
    bx::Queue<EfswResult*>::Node qnode;
    EfswResult() : qnode(this)
    {
    }
};
#endif

struct BlockingAssetDriver
{
    bx::AllocatorI* alloc;
    bx::Path rootDir;
    IoFlags::Bits flags;

    BlockingAssetDriver()
    {
        alloc = nullptr;
        flags = 0;
    }
};

struct DiskJobMode
{
    enum Enum {
        Read,
        Write
    };
};

struct DiskJobResult
{
    enum Enum {
        OpenFailed,
        ReadFailed,
        ReadOk,
        WriteFailed,
        WriteOk
    };
};

struct DiskJob
{
    // Request
    DiskJobMode::Enum mode;
    bx::Path uri;
    IoPathType::Enum pathType;
    IoReadFlags::Bits flags;

    // Result
    DiskJobResult::Enum result;
    MemoryBlock* mem;      // Read memory (read mode)
    size_t bytesWritten;       // written bytes (write mode)

    JobHandle handle;          // JobHandle

    bx::List<DiskJob*>::Node lnode; 

    DiskJob() : lnode(this)
    {
        mem = nullptr;
        flags = 0;
        handle = nullptr;
        bytesWritten = 0;
    }
};

struct AsyncAssetDriver
{
    bx::AllocatorI* alloc;
    bx::Pool<DiskJob> jobPool;
    bx::List<DiskJob*> jobList;
    bx::List<DiskJob*> pendingJobList;
    IoDriverEventsI* callbacks;
    int numDiskJobs;
    int maxDiskJobsProcessed;

#if USE_EFSW
    FileWatchListener watchListener;
    efsw::FileWatcher* fileWatcher;
    efsw::WatchID rootWatch;
    bx::Mutex reqMutex;
    bx::Queue<EfswResult*> efswQueue;
#endif

    AsyncAssetDriver()
    {
        callbacks = nullptr;
        numDiskJobs = 0; 
        maxDiskJobsProcessed = 0;
#if USE_EFSW
        fileWatcher = nullptr;
        rootWatch = 0;
#endif
    }
};

static BlockingAssetDriver gBlockingIo;
static AsyncAssetDriver gAsyncIo;
static CoreApi* gTee = nullptr;

#if BX_PLATFORM_IOS
static int g_assetsBundleId = -1;
int iosAddBundle(const char* bundleName);
bx::Path iosResolveBundlePath(int bundleId, const char* filepath);
#elif BX_PLATFORM_ANDROID
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

static AAssetManager* g_assetMgr = nullptr;

extern "C" JNIEXPORT void JNICALL Java_com_termite_util_Platform_termiteInitAssetManager(JNIEnv* env, jclass cls, jobject jassetManager)
{
    BX_UNUSED(cls);
    g_assetMgr = AAssetManager_fromJava(env, jassetManager);
}

#endif

static bx::Path resolvePath(const char* uri, const bx::Path& rootDir, IoPathType::Enum pathType)
{
    bx::Path filepath;
    switch (pathType) {
    case IoPathType::Assets:
#if BX_PLATFORM_IOS
        filepath = iosResolveBundlePath(g_assetsBundleId, uri);
#elif BX_PLATFORM_ANDROID
        assert(false);      // Resolving from assetManager is not supported
#else
        filepath = rootDir;
        filepath.join("assets").join(uri);
#endif
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

// BlockingIO
static bool blockInit(bx::AllocatorI* alloc, const char* uri, const void* params, IoDriverEventsI* callbacks, IoFlags::Bits flags)
{
    gBlockingIo.alloc = alloc;
    gBlockingIo.flags = flags;
    gBlockingIo.rootDir = uri;
    gBlockingIo.rootDir.normalizeSelf();

#if BX_PLATFORM_ANDROID
    if (!g_assetMgr) {
        TEE_ERROR_API(gTee, "JNI AssetManager is not initialized. Call "
                    "com.termite.utils.PlatformUtils.termiteInitAssetManager before initializing the engine");
        return false;
    }
#endif

    return true;
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

static MemoryBlock* uncompressBlob(MemoryBlock* mem, bx::AllocatorI* alloc, const char* filepath)
{
    assert(mem);
    // check the last extension
    const char* ext = strrchr(filepath, '.');
    if (ext && bx::strCmpI(ext + 1, "lz4") == 0 && mem->size > sizeof(uint32_t)) {
        uint32_t size = *((uint32_t*)mem->data);
        assert(size > 0);
        MemoryBlock* uncompressed = gTee->createMemoryBlock(size, alloc);
        if (uncompressed) {
            LZ4_decompress_safe((const char*)(mem->data + sizeof(uint32_t)), (char*)uncompressed->data, mem->size, size);
            gTee->releaseMemoryBlock(mem);
            mem = uncompressed;
        } else {
            gTee->releaseMemoryBlock(mem);
            mem = nullptr;
        }
    }

    return mem;
}

static void blockingReadJob(int jobIdx, void* userParam)
{
    DiskJob* job = (DiskJob*)userParam;

#if BX_PLATFORM_ANDROID
    // Asset Loading has a different path on android platform
    if (job->pathType == IoPathType::Assets) {
        AAsset* asset = AAssetManager_open(g_assetMgr, job->uri.cstr(), AASSET_MODE_UNKNOWN);
        if (!asset) {
            job->result = DiskJobResult::OpenFailed;
            return;
        }

        off_t size = AAsset_getLength(asset);
        MemoryBlock* mem = nullptr;
        int r = -1;
        if (size) {
            mem = gTee->createMemoryBlock(size, gBlockingIo.alloc);
            if (mem)
                r = AAsset_read(asset, mem->data, size);
        }
        AAsset_close(asset);

        if (mem && (gBlockingIo.flags & IoFlags::ExtractLZ4) && !(job->flags & IoReadFlags::RawRead)) {
            mem = uncompressBlob(mem, gBlockingIo.alloc, job->uri.cstr());
        }

        if (mem && r > 0) {
            job->result = DiskJobResult::ReadOk;
            job->mem = mem;
        } else if (!mem) {
            job->result = DiskJobResult::OpenFailed;
        } else if (r <= 0) {
            job->result = DiskJobResult::ReadFailed;
            gTee->releaseMemoryBlock(mem);
        }

        return;
    }
#endif

    // Normal reading from DiskFs
    bx::Path filepath = resolvePath(job->uri.cstr(), gBlockingIo.rootDir, job->pathType);
    bx::FileReader file;
    bx::Error err;

    if (file.open(filepath.cstr(), &err)) {
        // Get file size
        int64_t size = file.seek(0, bx::Whence::End);
        file.seek(0, bx::Whence::Begin);
        MemoryBlock* mem = nullptr;
        int r = 0;

        // Read Contents
        if (size > 0) {
            mem = gTee->createMemoryBlock((uint32_t)size, gBlockingIo.alloc);
            if (mem)
                r = file.read(mem->data, mem->size, &err);
        }
        file.close();

        // LZ4 decompress if flag is set
        if (mem && (gBlockingIo.flags & IoFlags::ExtractLZ4) && !(job->flags & IoReadFlags::RawRead)) {
            mem = uncompressBlob(mem, gBlockingIo.alloc, job->uri.cstr());
        }

        if (mem && r > 0) {
            job->result = DiskJobResult::ReadOk;
            job->mem = mem;
        } else if (!mem) {
            job->result = DiskJobResult::ReadFailed;
        } else if (r <= 0) {
            job->result = DiskJobResult::ReadFailed;
            gTee->releaseMemoryBlock(mem);
        }
    } else {
        job->result = DiskJobResult::OpenFailed;
    }
}

static void blockingWriteJob(int jobIdx, void* userParam)
{
    DiskJob* job = (DiskJob*)userParam;

#if BX_PLATFORM_ANDROID || BX_PLATFORM_IOS
    if (job->pathType == IoPathType::Assets) {
        job->result = DiskJobResult::OpenFailed;
        return;
    }
#endif

    assert(job->mem);
    size_t size = 0;
    bx::Path filepath = resolvePath(job->uri.cstr(), gBlockingIo.rootDir, job->pathType);

    bx::FileWriter file;
    bx::Error err;
    if (file.open(filepath.cstr(), false, &err)) {
        size = file.write(job->mem->data, job->mem->size, &err);
        file.close();

        if (size > 0) {
            job->result = DiskJobResult::WriteOk;
            job->bytesWritten = size;
        } else {
            job->result = DiskJobResult::WriteFailed;
        }
    } else {
        job->result = DiskJobResult::OpenFailed;
    }
}

static MemoryBlock* blockRead(const char* uri, IoPathType::Enum pathType, IoReadFlags::Bits flags)
{
    DiskJob job;
    job.mode = DiskJobMode::Read;
    job.uri = uri;
    job.flags = flags;
    job.pathType = pathType;
    blockingReadJob(0, &job);

    switch (job.result) {
    case DiskJobResult::ReadOk:
        break;
    case DiskJobResult::OpenFailed:
        TEE_ERROR_API(gTee, "DiskDriver: Unable to open file '%s' for reading", uri);
        break;
    case DiskJobResult::ReadFailed:
        TEE_ERROR_API(gTee, "DiskDriver: Unable to read file '%s'", uri);
        break;
    }
    return job.mem;
}

static size_t blockWrite(const char* uri, const MemoryBlock* mem, IoPathType::Enum pathType)
{
    DiskJob job;
    job.mode = DiskJobMode::Write;
    job.uri = uri;
    job.pathType = pathType;
    job.mem = const_cast<MemoryBlock*>(mem);

    blockingWriteJob(0, &job);

    switch (job.result) {
    case DiskJobResult::WriteOk:
        break;
    case DiskJobResult::OpenFailed:
        TEE_ERROR_API(gTee, "Unable to open file '%s' for writing", uri);
        break;
    case DiskJobResult::WriteFailed:
        TEE_ERROR_API(gTee, "Unable write file '%s'", uri);
        break;
    }

    return job.bytesWritten;
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
    return gBlockingIo.rootDir.cstr();
}

// AsyncIO
static bool asyncInit(bx::AllocatorI* alloc, const char* uri, const void* params, IoDriverEventsI* callbacks, IoFlags::Bits flags)
{
    // Initialize pools and their queues
    gAsyncIo.alloc = alloc;
    if (!gAsyncIo.jobPool.create(64, alloc))
        return false;

#if USE_EFSW
    gAsyncIo.fileWatcher = BX_NEW(alloc, efsw::FileWatcher);
    if (!gAsyncIo.fileWatcher)
        return false;
    // Watch assets directory only
    bx::Path watchDir = gBlockingIo.rootDir;
    watchDir.join("assets");
    gAsyncIo.rootWatch = gAsyncIo.fileWatcher->addWatch(watchDir.cstr(), &gAsyncIo.watchListener, true);
    gAsyncIo.watchListener.setRootDir(watchDir);
    gAsyncIo.fileWatcher->watch();
#endif
    return true;
}

static void asyncShutdown()
{
    gAsyncIo.jobPool.destroy();

#if USE_EFSW
    if (gAsyncIo.fileWatcher) {
        if (gAsyncIo.rootWatch) {
            gAsyncIo.fileWatcher->removeWatch(gAsyncIo.rootWatch);
            gAsyncIo.rootWatch = 0;
        }

        EfswResult* result;
        while (gAsyncIo.efswQueue.pop(&result)) {
            BX_DELETE(gAsyncIo.alloc, result);
        }

        BX_DELETE(gAsyncIo.alloc, gAsyncIo.fileWatcher);
    }
#endif
}

static void asyncSetCallbacks(IoDriverEventsI* callbacks)
{  
    gAsyncIo.callbacks = callbacks;
}

static IoDriverEventsI* asyncGetCallbacks()
{
    return gAsyncIo.callbacks;
}

static MemoryBlock* asyncRead(const char* uri, IoPathType::Enum pathType, IoReadFlags::Bits flags)
{
    DiskJob* dj = gAsyncIo.jobPool.newInstance<>();
    dj->mode = DiskJobMode::Read;
    dj->pathType = pathType;
    dj->uri = uri;
    dj->flags = flags;

    if (gAsyncIo.numDiskJobs < MAX_DISK_JOBS) {
        JobDesc job(blockingReadJob, dj);
        dj->handle = gTee->dispatchSmallJobs(&job, 1);
        if (dj->handle) {
            ++gAsyncIo.numDiskJobs;
            gAsyncIo.maxDiskJobsProcessed = std::max(gAsyncIo.numDiskJobs, gAsyncIo.maxDiskJobsProcessed);
            gAsyncIo.jobList.add(&dj->lnode);
        } else {
            // Add to pending to process later
            gAsyncIo.pendingJobList.add(&dj->lnode);
        }
    } else {
        gAsyncIo.pendingJobList.add(&dj->lnode);
    }
    return nullptr;
}

static size_t asyncWrite(const char* uri, const MemoryBlock* mem, IoPathType::Enum pathType)
{
    DiskJob* dj = gAsyncIo.jobPool.newInstance<>();
    dj->mode = DiskJobMode::Write;
    dj->pathType = pathType;
    dj->uri = uri;
    dj->mem = const_cast<MemoryBlock*>(mem);

    if (gAsyncIo.numDiskJobs < MAX_DISK_JOBS) {
        JobDesc job(blockingWriteJob, dj);
        dj->handle = gTee->dispatchSmallJobs(&job, 1);
        if (dj->handle) {
            ++gAsyncIo.numDiskJobs;
            gAsyncIo.maxDiskJobsProcessed = std::max(gAsyncIo.numDiskJobs, gAsyncIo.maxDiskJobsProcessed);
            gAsyncIo.jobList.add(&dj->lnode);
        } else {
            // Add to pending to process later
            gAsyncIo.pendingJobList.add(&dj->lnode);
        }
    } else {
        gAsyncIo.pendingJobList.add(&dj->lnode);
    }
    return 0;
}

// runs in main thread
static void asyncRunAsyncLoop()
{
    if (!gAsyncIo.callbacks)
        return;

    // Process main disk jobs
    bx::List<DiskJob*>::Node* node = gAsyncIo.jobList.getFirst();
    while (node) {
        bx::List<DiskJob*>::Node* next = node->next;
        DiskJob* dj = node->data;
        if (gTee->isJobDone(dj->handle)) {
            switch (dj->result) {
            case DiskJobResult::ReadOk:
                gAsyncIo.callbacks->onReadComplete(dj->uri.cstr(), dj->mem);
                break;
            case DiskJobResult::OpenFailed:
                gAsyncIo.callbacks->onOpenError(dj->uri.cstr());
                break;
            case DiskJobResult::ReadFailed:
                gAsyncIo.callbacks->onReadError(dj->uri.cstr());
                break;
            case DiskJobResult::WriteOk:
                gAsyncIo.callbacks->onWriteComplete(dj->uri.cstr(), dj->bytesWritten);
                break;
            case DiskJobResult::WriteFailed:
                gAsyncIo.callbacks->onWriteError(dj->uri.cstr());
                break;
            }

            // Remove the job
            gTee->deleteJob(dj->handle);
            gAsyncIo.jobList.remove(node);
            gAsyncIo.jobPool.deleteInstance(dj);
            --gAsyncIo.numDiskJobs;
        }

        node = next;
    }

    // Process pending jobs
    node = gAsyncIo.pendingJobList.getFirst();
    while (node && gAsyncIo.numDiskJobs < MAX_DISK_JOBS) {
        bx::List<DiskJob*>::Node* next = node->next;
        DiskJob* dj = node->data;
        gAsyncIo.pendingJobList.remove(node);

        if (dj->mode == DiskJobMode::Read) {
            asyncRead(dj->uri.cstr(), dj->pathType, dj->flags);
        } else if (dj->mode == DiskJobMode::Write) {
            asyncWrite(dj->uri.cstr(), dj->mem, dj->pathType);
        }

        node = next;
    }

#if USE_EFSW
    // Process Hot-Loads
    EfswResult* result;
    while (gAsyncIo.efswQueue.pop(&result)) {
        switch (result->action) {
        case efsw::Action::Modified:
            assert(gAsyncIo.callbacks);
            gAsyncIo.callbacks->onModified(result->filepath.cstr());
            break;
        default:
            assert(0);  // not implemented
            break;
        }
        BX_DELETE(gAsyncIo.alloc, result);
    }
#endif

}

static IoOperationMode::Enum asyncGetOpMode()
{
    return IoOperationMode::Async;
}

static const char* asyncGetUri()
{
    return gBlockingIo.rootDir.cstr();
}

#if USE_EFSW
void FileWatchListener::handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename, efsw::Action action, std::string oldFilename /*= ""*/)
{
    auto getRelativePath = [this](bx::Path& filepath)->const char* {
        if (BX_ENABLED(BX_PLATFORM_WINDOWS))
            filepath.toUnix();
        if (BX_ENABLED(BX_PLATFORM_WINDOWS) || BX_ENABLED(BX_PLATFORM_OSX))
            filepath.toLower();

        char* subpath = strstr(filepath.getBuffer(), this->m_rootDir.cstr());
        int sz = m_rootDir.getLength();
        if (subpath[sz] == '/')
            sz++;
        return subpath ? subpath + sz : filepath.cstr();
    };

    if (watchid == gAsyncIo.rootWatch) {
        switch (action) {
        case efsw::Action::Moved:
            // TODO: Probably we'll want to implement move for resources that are renamed
        case efsw::Action::Modified:
        {
            bx::Path filepath(dir.c_str());
            filepath.join(filename.c_str());
            bx::FileInfo info;
            bx::stat(filepath.cstr(), info);
            if (info.m_type == bx::FileInfo::Regular && info.m_size > 0) {
                EfswResult* result = BX_NEW(gAsyncIo.alloc, EfswResult);
                result->filepath = getRelativePath(filepath);
                result->action = efsw::Action::Modified;

                bx::MutexScope mtx(gAsyncIo.reqMutex);
                gAsyncIo.efswQueue.push(&result->qnode);
            }
            break;
        }
        case efsw::Action::Add:
        case efsw::Action::Delete:
            // No implementation for these cases
            // TODO: Probably we'll want to implement Delete for resources
            break;

        default:
            assert(0);
        }
    }
}

void FileWatchListener::setRootDir(const bx::Path& rootDir)
{
    m_rootDir = rootDir;
    m_rootDir.toUnix();
    if (BX_ENABLED(BX_PLATFORM_WINDOWS) || BX_ENABLED(BX_PLATFORM_OSX))
        m_rootDir.toLower();
}

#endif

//
PluginDesc* getDiskLiteDriverDesc()
{
    static PluginDesc desc;
    strcpy(desc.name, "DiskIO_Lite");
    strcpy(desc.description, "DiskIO-Lite driver (Blocking and Async)");
    desc.type = PluginType::IoDriver;
    desc.version = TEE_MAKE_VERSION(1, 0);
    return &desc;
}

void* initDiskLiteDriver(bx::AllocatorI* alloc, GetApiFunc getApi)
{
    gTee = (CoreApi*)getApi(uint16_t(ApiId::Core), 0);
    if (!gTee)
        return nullptr;
    
    static IoDriver asyncApi;
    static IoDriver blockApi;
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
    
#if BX_PLATFORM_IOS
    if (g_assetsBundleId == -1)
        g_assetsBundleId = iosAddBundle("assets");
#endif

    return &driver;
}

void shutdownDiskLiteDriver()
{
}

#ifdef termite_SHARED_LIB
TEE_PLUGIN_EXPORT void* termiteGetPluginApi(uint16_t apiId, uint32_t version)
{
    static PluginApi v0;

    if (TEE_VERSION_MAJOR(version) == 0) {
        v0.init = initDiskLiteDriver;
        v0.shutdown = shutdownDiskLiteDriver;
        v0.getDesc = getDiskLiteDriverDesc;
        return &v0;
    } else {
        return nullptr;
    }
}
#endif

