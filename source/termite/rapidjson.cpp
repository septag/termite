#include "pch.h"
#include "termite/rapidjson.h"

#include "tee.h"

namespace tee {

    void json::HeapAllocator::SetAlloc(bx::AllocatorI* alloc)
    {
        Alloc = alloc;
    }

    void json::HeapAllocator::Free(void *ptr)
    {
        BX_FREE(Alloc, ptr);
    }

    void* json::HeapAllocator::Realloc(void* originalPtr, size_t originalSize, size_t newSize)
    {
        BX_UNUSED(originalSize);
        if (newSize == 0) {
            BX_FREE(Alloc, originalPtr);
            return NULL;
        }
        return BX_REALLOC(Alloc, originalPtr, newSize);
    }

    void* json::HeapAllocator::Malloc(size_t size)
    {
        return BX_ALLOC(Alloc, size);
    }

    json::HeapAllocator::HeapAllocator()
    {
        BX_ASSERT(HeapAllocator::Alloc, "Static Allocator should be defined before use");
    }

    bx::AllocatorI* json::HeapAllocator::Alloc = nullptr;


    json::StackAllocator::StackAllocator()
    {
        BX_ASSERT(false, "NoFree allocator should not be constructed");
    }

    json::StackAllocator::StackAllocator(bx::AllocatorI* alloc) : m_alloc(alloc)
    {

    }

    void* json::StackAllocator::Malloc(size_t size)
    {
        return BX_ALLOC(m_alloc, size);
    }

    void* json::StackAllocator::Realloc(void* originalPtr, size_t originalSize, size_t newSize)
    {
        BX_UNUSED(originalSize);
        if (newSize == 0) {
            BX_FREE(m_alloc, originalPtr);
            return NULL;
        }
        return BX_REALLOC(m_alloc, originalPtr, newSize);
    }

    using namespace rapidjson;
    struct JsonHandler : public BaseReaderHandler<UTF8<>, JsonHandler>
    {
        bool StartObject() 
        {
            BX_TRACE("{");
            return true;
        }

        bool EndObject(SizeType)
        {
            BX_TRACE("}");
            return true;
        }

        bool String(const char* str, SizeType len, bool)
        {
            BX_TRACE("String: %s", str);
            return true;
        }

        bool Default()
        {
            return false;
        }

        bool Null()
        {
            BX_TRACE("[NULL]");
            return true;
        }

        bool Bool(bool v)
        {
            BX_TRACE("Boolean: %s", v ? "true" : "false");
            return true;
        }

        bool Int(int v)
        {
            BX_TRACE("Int: %d", v);
            return true;
        }

        bool Uint(int v)
        {
            BX_TRACE("UInt: %u", v);
            return true;
        }

        bool Int64(int64_t v)
        {
            BX_TRACE("Int64: %d", v);
            return true;
        }

        bool Uint64(uint64_t v)
        {
            BX_TRACE("UInt64: %u", v);
            return true;
        }

        bool Double(double v)
        {
            BX_TRACE("Float: %f", v);
            return true;
        }

        bool Key(const char* str, SizeType len, bool)
        {
            BX_TRACE("Key: %s", str);
            return true;
        }

        bool StartArray()
        {
            BX_TRACE("[");
            return true;
        }

        bool EndArray(SizeType)
        {
            BX_TRACE("]");
            return true;
        }
    };

    void testSax(const char* filepath)
    {
        json::HeapAllocator jalloc;
        GenericReader<UTF8<>, UTF8<>, json::HeapAllocator> reader(&jalloc, 256);
        JsonHandler handler;

        MemoryBlock* mem = readTextFile(filepath);
        if (mem) {
            StringStream ss((const char*)mem->data);
            reader.Parse(ss, handler);
            //InsituStringStream ss(
            //reader.Parse<kParseInsituFlag>(ss, handler);
        }
    }

} // namespace tee
 