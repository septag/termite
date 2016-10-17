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

    struct IoStreamFlag
    {
        enum Enum
        {
            WRITE = 0x01,
            READ = 0x02
        };

        typedef uint8_t Bits;
    };

    struct IoPathType
    {
        enum Enum
        {
            Assets,
            Relative,
            Absolute
        };
    };

    // Async: All driver operations are done in async mode, and every return value (read, write, openStream, etc...)
    //        will return invalid values. These values should be checked through the callbacks
    //        'runAsyncLoop' should also be called in every engine loop iteration
    // Sync: All driver operations are done in blocking mode, callbacks doesn't work, instead the caller should check
    //       For return values of functions
    struct IoOperationMode
    {
        enum Enum
        {
            Async,
            Blocking
        };
    };

    // Backend interface for 
    struct IoDriverApi
    {
    public:
        result_t(*init)(bx::AllocatorI* alloc, const char* uri, const void* params, IoDriverEventsI* callbacks/* = nullptr*/);
        void(*shutdown)();

        void(*setCallbacks)(IoDriverEventsI* callbacks);
        IoDriverEventsI* (*getCallbacks)();

        MemoryBlock* (*read)(const char* uri, IoPathType::Enum pathType/* = IoPathType::Assets*/);
        size_t(*write)(const char* uri, const MemoryBlock* mem, IoPathType::Enum pathType/* = IoPathType::Assets*/);

        IoStream* (*openStream)(const char* uri, IoStreamFlag flags);
        size_t(*writeStream)(IoStream* stream, const MemoryBlock* mem);
        MemoryBlock* (*readStream)(IoStream* stream);
        void(*closeStream)(IoStream* stream);

        void(*runAsyncLoop)();

        IoOperationMode::Enum(*getOpMode)();
        const char* (*getUri)();
    };

    // Used for plugins that support both async and blocking modes
    struct IoDriverDual
    {
        IoDriverApi* blocking;
        IoDriverApi* async;
    };
} // namespace termite
