#include "termite/core.h"
#include "termite/io_driver.h"

#define T_CORE_API
#include "termite/plugin_api.h"

#include "bx/crtimpl.h"
#include "bxx/path.h"
#include "bxx/logger.h"
#include "bxx/pool.h"

#include "uv.h"
#include <fcntl.h>
#include <ctime>

#define MAX_FILE_SIZE 1073741824    // 1GB

using namespace termite;

static CoreApi_v0* g_core = nullptr;

class AsyncDiskDriver;

struct DiskFileRequest
{
    AsyncDiskDriver* driver;
    bx::Path uri;
    uv_fs_t openReq;
    uv_fs_t rwReq;
    uv_fs_t statReq;
    uv_buf_t buff;
    MemoryBlock* mem;

    DiskFileRequest()
    {
        driver = nullptr;
        mem = nullptr;
        memset(&buff, 0x00, sizeof(buff));

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

class AsyncDiskDriver : public IoDriverI
{
private:
    IoDriverEventsI* m_callbacks;
    bx::Path m_rootDir;
    bx::AllocatorI* m_alloc;
    uv_loop_t m_loop;
    bx::Pool<DiskFileRequest> m_fsReqPool;
    uv_fs_event_t m_dirChange;

public:
    AsyncDiskDriver()
    {
        m_callbacks = nullptr;
        m_alloc = nullptr;
        memset(&m_dirChange, 0x00, sizeof(m_dirChange));
        memset(&m_loop, 0x00, sizeof(m_loop));
    }

    static void uvCallbackFsEvent(uv_fs_event_t* handle, const char* filename, int events, int status)
    {
        static bx::Path lastModFile;
        static double lastModTime = 0.0;

        AsyncDiskDriver* driver = (AsyncDiskDriver*)handle->data;

        if (events != UV_CHANGE)
            return;

        if (driver->m_callbacks) {
            bx::Path filepath(driver->m_rootDir);
            filepath.join(filename);

            if (filepath.getType() == bx::PathType::File) {
                // Ignore file changes less than 0.1 second
                double curTime = g_core->getElapsedTime();
                if (lastModFile.isEqual(filename) && (curTime - lastModTime) < 0.1)
                    return;
                driver->m_callbacks->onModified(filename);
                lastModFile = filename;
                lastModTime = curTime;
            }
        }
    }

    result_t init(bx::AllocatorI* alloc, const char* uri, const void* params, IoDriverEventsI* callbacks) override
    {
        m_alloc = alloc;

        m_rootDir = uri;
        m_rootDir.normalizeSelf();

        uv_loop_init(&m_loop);
        uv_fs_event_init(&m_loop, &m_dirChange);

        if (m_rootDir.getType() != bx::PathType::Directory) {
            T_ERROR_API(g_core, "Root Directory '%s' does not exist", m_rootDir.cstr());
            return T_ERR_FAILED;
        }

        m_callbacks = callbacks;

        if (!m_fsReqPool.create(20, alloc)) {
            return T_ERR_OUTOFMEM;
        }

        // Monitor root dir
        // Note: Recursive flag does not work under linux, according to documentation
        m_dirChange.data = this;
        if (uv_fs_event_start(&m_dirChange, uvCallbackFsEvent, m_rootDir.cstr(), UV_FS_EVENT_RECURSIVE)) {
            T_ERROR_API(g_core, "Could not monitor root directory '%s' for changes", m_rootDir.cstr());
            return T_ERR_FAILED;
        }

        return 0;
    }

    static void uvWalk(uv_handle_t* handle, void* arg)
    {
        uv_close(handle, nullptr);
    }

    void shutdown() override
    {
        uv_fs_event_stop(&m_dirChange);

        m_fsReqPool.destroy();

        // Walk the event loop handles and close all
        uv_walk(&m_loop, uvWalk, this);
        uv_run(&m_loop, UV_RUN_DEFAULT);
        uv_loop_close(&m_loop);
    }

    void setCallbacks(IoDriverEventsI* callbacks) override
    {
        m_callbacks = callbacks;
    }

    IoDriverEventsI* getCallbacks() override
    {
        return m_callbacks;
    }

    static void uvCloseFile(uv_fs_t* req)
    {
        DiskFileRequest* rr = (DiskFileRequest*)req->data;

        uv_fs_t closeReq;
        uv_fs_close(&rr->driver->m_loop, &closeReq, (uv_file)rr->openReq.result, nullptr);   // Synchronous
        rr->driver->m_fsReqPool.deleteInstance(rr);
    }

    // Read the file in blocks and put them into 
    static void uvCallbackRead(uv_fs_t* req)
    {
        DiskFileRequest* rr = (DiskFileRequest*)req->data;
        if (req->result < 0) {
            if (rr->driver->m_callbacks)
                rr->driver->m_callbacks->onReadError(rr->uri.cstr());
        } else if (req->result > 0) {
            if (rr->driver->m_callbacks)
                rr->driver->m_callbacks->onReadComplete(rr->uri.cstr(), g_core->refMemoryBlock(rr->mem));
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
                rr->mem = g_core->createMemoryBlock((uint32_t)req->statbuf.st_size, rr->driver->m_alloc);
                if (!rr->mem) {
                    if (rr->driver->m_callbacks)
                        rr->driver->m_callbacks->onOpenError(rr->uri.cstr());
                }
                rr->buff = uv_buf_init((char*)rr->mem->data, rr->mem->size);
                rr->rwReq.data = rr;
                uv_fs_read(&rr->driver->m_loop, &rr->rwReq, (uv_file)rr->openReq.result, &rr->buff, 1, -1, uvCallbackRead);
            } else {
                uvCloseFile(req);
            }
        } else {
            if (rr->driver->m_callbacks)
                rr->driver->m_callbacks->onOpenError(rr->uri.cstr());
            uvCloseFile(req);
        }
    }

    static void uvCallbackOpenForRead(uv_fs_t* req)
    {
        DiskFileRequest* rr = (DiskFileRequest*)req->data;
        if (req->result >= 0) {
            rr->statReq.data = rr;
            uv_fs_fstat(&rr->driver->m_loop, &rr->statReq, (uv_file)rr->openReq.result, uvCallbackStat);
        } else {
            if (rr->driver->m_callbacks)
                rr->driver->m_callbacks->onOpenError(rr->uri.cstr());
            rr->driver->m_fsReqPool.deleteInstance(rr);
        }
    }

    MemoryBlock* read(const char* uri) override
    {
        bx::Path filepath(m_rootDir);
        filepath.join(uri);

        DiskFileRequest* req = m_fsReqPool.newInstance();

        if (!req) {
            if (m_callbacks)
                m_callbacks->onOpenError(filepath.cstr());
            return nullptr;
        }

        req->driver = this;
        req->uri = uri;
        req->openReq.data = req;
        
        // Start Open for read
        if (uv_fs_open(&m_loop, &req->openReq, filepath.cstr(), 0, O_RDONLY, uvCallbackOpenForRead)) {
            if (m_callbacks)
                m_callbacks->onOpenError(filepath.cstr());
            return nullptr;
        }

        return nullptr;
    }

    void runAsyncLoop() override
    {
        uv_run(&m_loop, UV_RUN_NOWAIT);
    }

    static void uvCallbackWrite(uv_fs_t* req)
    {
        DiskFileRequest* rr = (DiskFileRequest*)req->data;
        if (req->result >= 0) {
            if (rr->driver->m_callbacks)
                rr->driver->m_callbacks->onWriteComplete(rr->uri.cstr(), (size_t)req->result);
        } else {
            if (rr->driver->m_callbacks)
                rr->driver->m_callbacks->onWriteError(rr->uri.cstr());
        }

        uvCloseFile(req);
    }

    static void uvCallbackOpenForWrite(uv_fs_t* req)
    {
        DiskFileRequest* rr = (DiskFileRequest*)req->data;
        if (req->result >= 0) {
            rr->rwReq.data = rr;
            rr->buff = uv_buf_init((char*)rr->mem->data, rr->mem->size);
            uv_fs_write(&rr->driver->m_loop, &rr->rwReq, (uv_file)rr->openReq.result, &rr->buff, 1, -1, uvCallbackWrite);
        } else {
            if (rr->driver->m_callbacks)
                rr->driver->m_callbacks->onOpenError(rr->uri.cstr());
            rr->driver->m_fsReqPool.deleteInstance(rr);
        }
    }

    size_t write(const char* uri, const MemoryBlock* mem) override
    {
        bx::Path filepath(m_rootDir);
        filepath.join(uri);

        DiskFileRequest* req = m_fsReqPool.newInstance();

        if (!req) {
            if (m_callbacks)
                m_callbacks->onOpenError(filepath.cstr());
            return 0;
        }

        req->driver = this;
        req->uri = uri;
        req->openReq.data = req;
        req->mem = g_core->refMemoryBlock(const_cast<MemoryBlock*>(mem));

        // Start Open for write
        if (uv_fs_open(&m_loop, &req->openReq, filepath.cstr(), 0, O_CREAT | O_WRONLY | O_TRUNC, uvCallbackOpenForWrite)) {
            if (m_callbacks)
                m_callbacks->onOpenError(filepath.cstr());
            return 0;
        }

        return 0;
    }

    IoStream* openStream(const char* uri, IoStreamFlag flags) override
    {
        return nullptr;
    }

    size_t writeStream(IoStream* stream, const MemoryBlock* mem) override
    {
        return 0;
    }

    MemoryBlock* readStream(IoStream* stream) override
    {
        return nullptr;
    }

    void closeStream(IoStream* stream) override
    {
    }

    IoOperationMode getOpMode() const override
    {
        return IoOperationMode::Async;
    }

    const char* getUri() const override
    {
        return m_rootDir.cstr();
    }
};

class BlockingDiskDriver : public IoDriverI
{
private:
    bx::Path m_rootDir;
    bx::AllocatorI* m_alloc;

public:
    BlockingDiskDriver()
    {
        m_alloc = nullptr;
    }

    int init(bx::AllocatorI* alloc, const char* uri, const void* params, IoDriverEventsI* callbacks) override
    {
        m_alloc = alloc;

        m_rootDir = uri;
        m_rootDir.normalizeSelf();

        if (m_rootDir.getType() != bx::PathType::Directory) {
            T_ERROR_API(g_core, "Root Directory '%s' does not exist", m_rootDir.cstr());
            return T_ERR_FAILED;
        }

        return 0;
    }

    void shutdown() override
    {
    }

    void setCallbacks(IoDriverEventsI* callbacks) override
    {
    }

    IoDriverEventsI* getCallbacks() override
    {
        return nullptr;
    }

    MemoryBlock* read(const char* uri) override
    {
        bx::Path filepath(m_rootDir);
        filepath.join(uri);

        bx::CrtFileReader file;
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
            mem = g_core->createMemoryBlock((uint32_t)size, m_alloc);
            if (mem)
                file.read(mem->data, mem->size, &err);
        }

        file.close();
        return mem;
    }

    size_t write(const char* uri, const MemoryBlock* mem) override
    {
        bx::Path filepath(m_rootDir);
        filepath.join(uri);

        bx::CrtFileWriter file;
        bx::Error err;
        if (file.open(filepath.cstr(), false, &err)) {
            T_ERROR_API(g_core, "Unable to open file '%s' for writing", filepath.cstr());
            return 0;
        }

        // Write memory
        size_t size = file.write(mem->data, mem->size, &err);
        file.close();
        return size;
    }

    IoStream* openStream(const char* uri, IoStreamFlag flags) override
    {
        return nullptr;
    }

    size_t writeStream(IoStream* stream, const MemoryBlock* mem) override
    {
        return 0;
    }

    MemoryBlock* readStream(IoStream* stream) override
    {
        return nullptr;
    }

    void closeStream(IoStream* stream) override
    {
    }

    void runAsyncLoop() override
    {
    }

    IoOperationMode getOpMode() const override
    {
        return IoOperationMode::Blocking;
    }

    const char* getUri() const override
    {
        return m_rootDir.cstr();
    }
};


#ifdef termite_SHARED_LIB
static BlockingDiskDriver g_blockingDiskDriver;
static AsyncDiskDriver g_asyncDiskDriver;
static IoDriverDual g_diskDriver = {
    &g_blockingDiskDriver,
    &g_asyncDiskDriver
};

static PluginDesc* getDiskDriverDesc()
{
    static PluginDesc desc;
    strcpy(desc.name, "DiskIO");
    strcpy(desc.description, "DiskIO Driver (Blocking and Async)");
    desc.type = PluginType::IoDriver;
    desc.version = T_MAKE_VERSION(0, 9);
    return &desc;
}

static void* initDiskDriver(bx::AllocatorI* alloc, GetApiFunc getApi)
{
    g_core = (CoreApi_v0*)getApi(uint16_t(ApiId::Core), 0);
    if (!g_core)
        return nullptr;

    return &g_diskDriver;
}

static void shutdownDiskDriver()
{
}

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