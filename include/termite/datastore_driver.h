#pragma once

#include "bx/bx.h"

namespace termite
{
    struct dsStream;

    class BX_NO_VTABLE dsDriverCallbacks
    {
    public:
        virtual void onOpenError(const char* uri) = 0;
        virtual void onReadError(const char* uri) = 0;
        virtual void onWriteError(const char* uri) = 0;

        virtual void onReadComplete(const char* uri, MemoryBlock* mem) = 0;
        virtual void onWriteComplete(const char* uri, size_t size) = 0;
        virtual void onModified(const char* uri) = 0;

        virtual void onOpenStream(dsStream* stream) = 0;
        virtual void onReadStream(dsStream* stream, MemoryBlock* mem) = 0;
        virtual void onWriteStream(dsStream* stream, size_t size) = 0;
        virtual void onCloseStream(dsStream* stream) = 0;
    };

    enum dsStreamFlag : uint8_t
    {
        WRITE = 0x01,
        READ = 0x02
    };

    // Async: All driver operations are done in async mode, and every return value (read, write, openStream, etc...)
    //        will return invalid values. These values should be checked through the callbacks
    //        'runAsyncLoop' should also be called in every engine loop iteration
    // Sync: All driver operations are done in blocking mode, callbacks doesn't work, instead the caller should check
    //       For return values of functions
    enum dsOperationMode
    {
        Async,
        Blocking
    };

    // Backend interface for 
    class BX_NO_VTABLE dsDriver
    {
    public:
        virtual int init(bx::AllocatorI* alloc, const char* uri, const void* params, 
                         dsDriverCallbacks* callbacks = nullptr) = 0;
        virtual void shutdown() = 0;

        virtual void setCallbacks(dsDriverCallbacks* callbacks) = 0;
        virtual dsDriverCallbacks* getCallbacks() = 0;

        virtual MemoryBlock* read(const char* uri) = 0;
        virtual size_t write(const char* uri, const MemoryBlock* mem) = 0;

        virtual dsStream* openStream(const char* uri, dsStreamFlag flags) = 0;
        virtual size_t writeStream(dsStream* stream, const MemoryBlock* mem) = 0;
        virtual MemoryBlock* readStream(dsStream* stream) = 0;
        virtual void closeStream(dsStream* stream) = 0;

        virtual void runAsyncLoop() = 0;

        virtual dsOperationMode getOpMode() = 0;
    };
} // namespace st
