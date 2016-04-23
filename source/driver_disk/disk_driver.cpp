#include "termite/core.h"
#include "termite/datastore_driver.h"
#include "termite/driver_mgr.h"
#include "termite/plugins.h"

#include "bx/crtimpl.h"
#include "bxx/path.h"
#include "bxx/logger.h"
#include "bxx/pool.h"

#include "uv.h"
#include <fcntl.h>
#include <ctime>

#define MAX_FILE_SIZE 1073741824    // 1GB

using namespace termite;

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

        uv_fs_req_cleanup(&openReq);
        uv_fs_req_cleanup(&rwReq);
        uv_fs_req_cleanup(&statReq);
    }

    ~DiskFileRequest()
    {
        if (mem)
            coreReleaseMemory(mem);
        uv_fs_req_cleanup(&openReq);
        uv_fs_req_cleanup(&rwReq);
        uv_fs_req_cleanup(&statReq);
    }
};

class AsyncDiskDriver : public dsDriver
{
private:
    dsDriverCallbacks* m_callbacks;
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
                double curTime = coreGetElapsedTime();
                if (lastModFile.isEqual(filename) && (curTime - lastModTime) < 0.1)
                    return;
                driver->m_callbacks->onModified(filename);
                lastModFile = filename;
                lastModTime = curTime;
            }
        }
    }

    int init(bx::AllocatorI* alloc, const char* uri, const void* params, dsDriverCallbacks* callbacks) override
    {
        BX_BEGINP("Initializing DataStore Async disk driver");
        m_alloc = alloc;

        m_rootDir = uri;
        m_rootDir.normalizeSelf();

        uv_loop_init(&m_loop);
        uv_fs_event_init(&m_loop, &m_dirChange);

        if (m_rootDir.getType() != bx::PathType::Directory) {
            BX_END_FATAL();
            T_ERROR("Root Directory '%s' does not exist", m_rootDir.cstr());
            return T_ERR_FAILED;
        }

        m_callbacks = callbacks;

        if (!m_fsReqPool.create(20, alloc)) {
            BX_END_FATAL();
            return T_ERR_OUTOFMEM;
        }

        // Monitor root dir
        // Note: Recursive flag does not work under linux, according to documentation
        m_dirChange.data = this;
        if (uv_fs_event_start(&m_dirChange, uvCallbackFsEvent, m_rootDir.cstr(), UV_FS_EVENT_RECURSIVE)) {
            BX_END_FATAL();
            T_ERROR("Could not monitor root directory '%s' for changes", m_rootDir.cstr());
            return T_ERR_FAILED;
        }

        BX_END_OK();
        return T_OK;
    }

    static void uvWalk(uv_handle_t* handle, void* arg)
    {
        uv_close(handle, nullptr);
    }

    void shutdown() override
    {
        BX_BEGINP("Shutting down DataStore disk backend");
        uv_fs_event_stop(&m_dirChange);

        m_fsReqPool.destroy();

        // Walk the event loop handles and close all
        uv_walk(&m_loop, uvWalk, this);
        uv_run(&m_loop, UV_RUN_DEFAULT);
        uv_loop_close(&m_loop);

        BX_END_OK();
    }

    void setCallbacks(dsDriverCallbacks* callbacks) override
    {
        m_callbacks = callbacks;
    }

    dsDriverCallbacks* getCallbacks() override
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
                rr->driver->m_callbacks->onReadComplete(rr->uri.cstr(), coreRefMemory(rr->mem));
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
                rr->mem = coreCreateMemory((uint32_t)req->statbuf.st_size, rr->driver->m_alloc);
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
        req->mem = coreRefMemory(const_cast<MemoryBlock*>(mem));

        // Start Open for write
        if (uv_fs_open(&m_loop, &req->openReq, filepath.cstr(), 0, O_CREAT | O_WRONLY | O_TRUNC, uvCallbackOpenForWrite)) {
            if (m_callbacks)
                m_callbacks->onOpenError(filepath.cstr());
            return 0;
        }

        return 0;
    }

    dsStream* openStream(const char* uri, dsStreamFlag flags) override
    {
        return nullptr;
    }

    size_t writeStream(dsStream* stream, const MemoryBlock* mem) override
    {
        return 0;
    }

    MemoryBlock* readStream(dsStream* stream) override
    {
        return nullptr;
    }

    void closeStream(dsStream* stream) override
    {
    }

    dsOperationMode getOpMode() override
    {
        return dsOperationMode::Async;
    }
};

class BlockingDiskDriver : public dsDriver
{
private:
    bx::Path m_rootDir;
    bx::AllocatorI* m_alloc;

public:
    BlockingDiskDriver()
    {
        m_alloc = nullptr;
    }

    int init(bx::AllocatorI* alloc, const char* uri, const void* params, dsDriverCallbacks* callbacks) override
    {
        BX_BEGINP("Initializing DataStore Blocking disk driver");
        m_alloc = alloc;

        m_rootDir = uri;
        m_rootDir.normalizeSelf();

        if (m_rootDir.getType() != bx::PathType::Directory) {
            BX_END_FATAL();
            T_ERROR("Root Directory '%s' does not exist", m_rootDir.cstr());
            return T_ERR_FAILED;
        }
        BX_END_OK();

        return T_OK;
    }

    void shutdown() override
    {
    }

    void setCallbacks(dsDriverCallbacks* callbacks) override
    {
    }

    dsDriverCallbacks* getCallbacks() override
    {
        return nullptr;
    }

    MemoryBlock* read(const char* uri) override
    {
        bx::Path filepath(m_rootDir);
        filepath.join(uri);

        bx::CrtFileReader file;
        bx::Error err;
        if (file.open(filepath.cstr(), &err)) {
            T_ERROR("Unable to open file '%s' for reading", filepath.cstr());
            return nullptr;
        }

        // Get file size
        int64_t size = file.seek(0, bx::Whence::End);
        file.seek(0, bx::Whence::Begin);

        // Read Contents
        MemoryBlock* mem = nullptr;
        if (size) {
            mem = coreCreateMemory((uint32_t)size, m_alloc);
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
            T_ERROR("Unable to open file '%s' for writing", filepath.cstr());
            return 0;
        }

        // Write memory
        size_t size = file.write(mem->data, mem->size, &err);
        file.close();
        return size;
    }

    dsStream* openStream(const char* uri, dsStreamFlag flags) override
    {
        return nullptr;
    }

    size_t writeStream(dsStream* stream, const MemoryBlock* mem) override
    {
        return 0;
    }

    MemoryBlock* readStream(dsStream* stream) override
    {
        return nullptr;
    }

    void closeStream(dsStream* stream) override
    {
    }

    void runAsyncLoop() override
    {
    }

    dsOperationMode getOpMode() override
    {
        return dsOperationMode::Blocking;
    }
};


#ifdef termite_SHARED_LIB
#define MY_VERSION T_MAKE_VERSION(1, 0)
static bx::AllocatorI* g_alloc = nullptr;
static termite::drvHandle gAsyncDriver = nullptr;
static termite::drvHandle gBlockingDriver = nullptr;

TERMITE_PLUGIN_API pluginDesc* stPluginGetDesc()
{
    static pluginDesc desc;
    desc.name = "DiskDriver";
    desc.description = "Disk Driver, Async and Blocking";
    desc.engineVersion = T_MAKE_VERSION(0, 1);
    desc.type = drvType::DataStoreDriver;
    desc.version = MY_VERSION;
    return &desc;
}

TERMITE_PLUGIN_API int stPluginInit(bx::AllocatorI* alloc)
{
    assert(alloc);

    g_alloc = alloc;
    AsyncDiskDriver* asyncDriver = BX_NEW(alloc, AsyncDiskDriver);
    if (asyncDriver) {
        gAsyncDriver = drvRegister(drvType::DataStoreDriver, "AsyncDiskDriver", MY_VERSION, asyncDriver);
        if (gAsyncDriver == nullptr) {
            BX_DELETE(alloc, asyncDriver);
            g_alloc = nullptr;
            return T_ERR_FAILED;
        }
    }

    BlockingDiskDriver* blockingDriver = BX_NEW(alloc, BlockingDiskDriver);
    if (blockingDriver) {
        gBlockingDriver = drvRegister(drvType::DataStoreDriver, "BlockingDiskDriver", MY_VERSION, blockingDriver);
        if (blockingDriver == nullptr) {
            BX_DELETE(alloc, blockingDriver);
            g_alloc = nullptr;
            return T_ERR_FAILED;
        }
    }

    return T_OK;
}

TERMITE_PLUGIN_API void stPluginShutdown()
{
    assert(g_alloc);

    if (gAsyncDriver) {
        BX_DELETE(g_alloc, drvGetDataStoreDriver(gAsyncDriver));
        drvUnregister(gAsyncDriver);
    }

    if (gBlockingDriver) {
        BX_DELETE(g_alloc, drvGetDataStoreDriver(gBlockingDriver));
        drvUnregister(gBlockingDriver);
    }

    g_alloc = nullptr;
    gAsyncDriver = nullptr;
    gBlockingDriver = nullptr;
}
#endif