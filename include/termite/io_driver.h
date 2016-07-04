#pragma once

#include "bx/bx.h"

namespace termite
{
    struct IoStream;

    class BX_NO_VTABLE IoDriverEventsI
    {
    public:
        virtual void onOpenError(const char* uri) = 0;
        virtual void onReadError(const char* uri) = 0;
        virtual void onWriteError(const char* uri) = 0;

        virtual void onReadComplete(const char* uri, MemoryBlock* mem) = 0;
        virtual void onWriteComplete(const char* uri, size_t size) = 0;
        virtual void onModified(const char* uri) = 0;

        virtual void onOpenStream(IoStream* stream) = 0;
        virtual void onReadStream(IoStream* stream, MemoryBlock* mem) = 0;
        virtual void onWriteStream(IoStream* stream, size_t size) = 0;
        virtual void onCloseStream(IoStream* stream) = 0;
    };

    enum IoStreamFlag : uint8_t
    {
        WRITE = 0x01,
        READ = 0x02
    };

    // Async: All driver operations are done in async mode, and every return value (read, write, openStream, etc...)
    //        will return invalid values. These values should be checked through the callbacks
    //        'runAsyncLoop' should also be called in every engine loop iteration
    // Sync: All driver operations are done in blocking mode, callbacks doesn't work, instead the caller should check
    //       For return values of functions
    enum IoOperationMode
    {
        Async,
        Blocking
    };

    // Backend interface for 
    class BX_NO_VTABLE IoDriverI
    {
    public:
        virtual result_t init(bx::AllocatorI* alloc, const char* uri, const void* params, 
                              IoDriverEventsI* callbacks = nullptr) = 0;
        virtual void shutdown() = 0;

        virtual void setCallbacks(IoDriverEventsI* callbacks) = 0;
        virtual IoDriverEventsI* getCallbacks() = 0;

        virtual MemoryBlock* read(const char* uri) = 0;
        virtual size_t write(const char* uri, const MemoryBlock* mem) = 0;

        virtual IoStream* openStream(const char* uri, IoStreamFlag flags) = 0;
        virtual size_t writeStream(IoStream* stream, const MemoryBlock* mem) = 0;
        virtual MemoryBlock* readStream(IoStream* stream) = 0;
        virtual void closeStream(IoStream* stream) = 0;

        virtual void runAsyncLoop() = 0;

        virtual IoOperationMode getOpMode() const = 0;
        virtual const char* getUri() const = 0;
    };

    // Used for plugins that support both async and blocking modes
    struct IoDriverDual
    {
        IoDriverI* blocking;
        IoDriverI* async;
    };
} // namespace termite
